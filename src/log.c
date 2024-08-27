#include <app.h>
#include <stdarg.h>
#include <time.h>
#define LINE_BUF_SIZE 2048

static char line_buffer[LINE_BUF_SIZE];
static bool redirected_stdio = false;

static size_t init_line_buffer()
{
	time_t rawtime;
	struct tm *local_time;
	
	if (unlikely(time(&rawtime) < 0))
		return 0;
	local_time = localtime(&rawtime);
	if (unlikely(!local_time))
		return 0;
	return strftime(line_buffer, LINE_BUF_SIZE, "%b %d %H:%M:%S ", local_time);
}

/* Initialize time string at the beginning of the line */
#define INIT_LINE_PTR(line_ptr)																\
	line_ptr += init_line_buffer();															\
	if (unlikely(line_ptr == line_buffer))													\
		line_ptr += strftime(line_buffer, LINE_BUF_SIZE, "%b %d %H:%M:%S ", localtime(0));	\
	if (unlikely(line_ptr == line_buffer))													\
		line_ptr[0] = '\0';

static Action check_lprintf_format(const char *restrict format)
{
	Action a;

	/* 
	 * DEFAULT - to stdout_fileno
	 * SPECIAL - to stderr_fileno
	*/

	if (format[0] != '[')
		a = FALLBACK;
	else switch(format[1]) {
		case 'D':
			a = SPECIAL;
			if (log_level < LOG_DEBUG)
				a = ABORT;
			break;
		case 'I':
			a = DEFAULT;
			if (log_level < LOG_INFO)
				a = ABORT;
			break;
		case 'W':
			a = SPECIAL;
			if (log_level < LOG_WARNING)
				a = ABORT;
			break;
		case 'E':
			a = SPECIAL;
			if (log_level < LOG_ERROR)
				a = ABORT;
			break;
		case 'R':
			a = DEFAULT;
			break;
		default:
			a = FALLBACK;
	}
	if (a == FALLBACK && log_level < LOG_INFO)
		a = ABORT;
	
	return a;
}

int lprintf(const char *restrict format, ...)
{
	va_list ptr;
	int ret, olderrno = errno;
	Action a = check_lprintf_format(format);
	
	va_start(ptr, format);
	if (a == SPECIAL)
		ret = lvfprintf(stderr, format, ptr, &a);
	else
		ret = lvfprintf(stdout, format, ptr, &a);
	va_end(ptr);
	errno = olderrno;
	return ret;
}

int lfprintf(FILE *restrict stream, const char *restrict format, ...)
{
	va_list ptr;
	int ret, olderrno = errno;

	va_start(ptr, format);
	/* Override the stream from Action (Same Action for DEFAULT and SPECIAL) */
	ret = lvfprintf(stream, format, ptr, NULL);
	va_end(ptr);
	errno = olderrno;
	return ret;
}

int lvfprintf(FILE *restrict stream, const char *restrict format, va_list ap, Action *_Nullable ac)
{
	char *line_ptr = line_buffer;
	int ret, olderrno = errno;
	Action a;

	if (ac)
		a = *ac;
	else
		a = check_lprintf_format(format);
	if (a == ABORT)
		return 0;
	if (!redirected_stdio) {
		/* Bypass log formatting */
		ret = vfprintf(stream, format, ap);
		errno = olderrno;
		return ret;
	}

	INIT_LINE_PTR(line_ptr)
	if (a == FALLBACK)
		snprintf(line_ptr, LINE_BUF_SIZE-(line_ptr-line_buffer), "[INFO]: %s", format);
	else
		snprintf(line_ptr, LINE_BUF_SIZE-(line_ptr-line_buffer), "%s", format);

	if (flock(fileno(stream), LOCK_EX) < 0)
		destructor(errno);	/* Logging isn't reliable without locking. Just exit. */

	/* Override the stream from Action (Same Action for DEFAULT and SPECIAL) */
	ret = vfprintf(stream, line_buffer, ap);

	if (flock(fileno(stream), LOCK_UN) < 0)
		destructor(errno);	/* We can't keep the file deadlocked. Exit. */
	errno = olderrno;
	return ret;
}

void lperror(const char *s)
{
	char *line_ptr = line_buffer;
	int olderrno = errno;
	Action ret = check_lprintf_format(s);

	if (ret == ABORT)
		return;
	if (ret == FALLBACK && log_level < LOG_DEBUG)
		return;
	if (!redirected_stdio) {
		/* Bypass log formatting */
		if (ret == FALLBACK)
			fprintf(stderr, "[DEBUG]: ");
		perror(s);
		errno = olderrno;
		return;
	}

	INIT_LINE_PTR(line_ptr)
	if (flock(STDERR_FILENO, LOCK_EX) < 0)
		destructor(errno);	/* Logging isn't reliable without locking. Just exit. */
	fprintf(stderr, "%s", line_buffer);
	if (ret == FALLBACK)
		fprintf(stderr, "[DEBUG]: ");
	perror(s);
	if (flock(STDERR_FILENO, LOCK_UN) < 0)
		destructor(errno);	/* We can't keep the file deadlocked. Exit. */
	errno = olderrno;
}

void __server log_stdio()
{
	int fd, nullfd;

	if (options.is_foreground)
		return;
	fd = open(log_path, O_CREAT | O_RDWR | O_APPEND | O_CLOEXEC, 0600);
	check( nullfd = open("/dev/null", O_RDWR | O_CLOEXEC) )
	if (fd < 0) {
		dlperror("open");
		lprintf("[WARNING]: Cannot open %s\n", log_path);
		lprintf("[WARNING]: No logging functionality present.\n");
		fd = nullfd;
	}
	check( dup2(nullfd, STDIN_FILENO) )
	check( dup2(fd, STDOUT_FILENO) )
	check( dup2(fd, STDERR_FILENO) )

	if (likely(nullfd > STDERR_FILENO))
		check( close(nullfd) )
	if (likely(fd > STDERR_FILENO) && fd != nullfd)
		check( close(fd) )
	redirected_stdio = true;
}