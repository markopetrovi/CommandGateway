#include <app.h>

static bool manualDestructorCall = false;
static time_t timeout_seconds = 5;

char version[] = "CommandGateway 1.0 (C) 2024 Marko Petrovic\n";
struct program_options options;
bool shouldDeleteSocket = false;
int log_level = LOG_WARNING;
char *sockPath = NULL, *rootPath = "/";
char __server_data *log_path = "/var/log/cg.log";
char *group_testdev = "jmatestdev", *group_dev = "jmadev";
char *group_admin = "jmaadmin", *group_superuser = "jmaroot";

void __server daemonize()
{
	pid_t pid;

	check( pid = fork() )
	if (pid != 0)
		exit(0);
	// Do setsid(2) in child. It might have failed in parent if it's PID = PGID
	setsid();
	// Daemon shall not be the session leader, thus fork again
	check( pid = fork() )
	if (pid != 0)
		exit(0);
}

bool does_file_exist(char *path)
{
	int fd;
	fd = open(path, O_PATH);
	if (fd < 0)
		return false;
	check( close(fd) )
	return true;
}

void clear_environment()
{
	unsetenv("SOCK_PATH");
	unsetenv("ROOT_PATH");
	unsetenv("LOG_LEVEL");
	unsetenv("TIMEOUT");
}

static void load_groups()
{
	char *isPresent;

	isPresent = getenv("GROUP_TESTDEV");
	if (isPresent)
		group_testdev = isPresent;
	isPresent = getenv("GROUP_DEV");
	if (isPresent)
		group_dev = isPresent;
	isPresent = getenv("GROUP_ADMIN");
	if (isPresent)
		group_admin = isPresent;
	isPresent = getenv("GROUP_SUPERUSER");
	if (isPresent)
		group_superuser = isPresent;

	lprintf("[INFO]: Using %s for GROUP_TESTDEV\n", group_testdev);
	lprintf("[INFO]: Using %s for GROUP_DEV\n", group_dev);
	lprintf("[INFO]: Using %s for GROUP_ADMIN\n", group_admin);
	lprintf("[INFO]: Using %s for GROUP_SUPERUSER\n", group_superuser);
}

static void __server load_server_env()
{
	char *isPresent;

	isPresent = getenv("ROOT_PATH");
	if (isPresent)
		rootPath = isPresent;
	lprintf("[INFO]: Using %s for ROOT_PATH\n", rootPath);
	if (!does_file_exist(rootPath)) {
		lprintf("[ERROR]: Supplied ROOT_PATH folder \"%s\" does not exist\n", rootPath);
		destructor(ENOENT);
	}

	isPresent = getenv("LOG_PATH");
	if (isPresent)
		log_path = isPresent;
	lprintf("[INFO]: Using %s for LOG_PATH\n", log_path);
}

static void load_environment()
{
	char *log_level_str, *relative_sockPath, *timeout_str;

	#ifdef SERVER_BUILD
	load_server_env();
	#endif
	load_groups();
	relative_sockPath = getenv("SOCK_PATH");
	if (!relative_sockPath)
		relative_sockPath = "/tmp/cgsocket";
	lprintf("[INFO]: Using %s for SOCK_PATH\n", relative_sockPath);
	check( asprintf(&sockPath, "%s/%s", rootPath, relative_sockPath) )

	timeout_str = getenv("TIMEOUT");
	if (timeout_str) {
		timeout_seconds = atoi(timeout_str);
		if (!timeout_seconds)
			lprintf("[WARNING]: Socket timeout disabled. Operations can block indefinitely.\n");
		lprintf("[INFO]: Socket TIMEOUT set to %i\n", timeout_seconds);
	}

	log_level_str = getenv("LOG_LEVEL");
	if (log_level_str) {
		switch (log_level_str[0]) {
			case 'N':
			case 'n':
				log_level = LOG_NONE;
				break;
			case 'E':
			case 'e':
				log_level = LOG_ERROR;
				break;
			case 'W':
			case 'w':
				log_level = LOG_WARNING;
				break;
			case 'I':
			case 'i':
				log_level = LOG_INFO;
				break;
			case 'D':
			case 'd':
				log_level = LOG_DEBUG;
				break;
			default:
				log_level = LOG_WARNING;
				lprintf("[WARNING]: Unknown LOG_LEVEL value. Using the default value: LOG_WARNING.\n");
		}
	}
}

void _destructor(int signum)
{
	const char *signal;

	if (sockfd != -1)
		close(sockfd);
	if (shouldDeleteSocket)
		unlink(sockPath);
	if (!manualDestructorCall) {
		signal = sigdescr_np(signum);
		if (!signal)
			signal = "UNKNOWN";
		lprintf("[WARNING]: Received signal %s - exiting...\n", signal);
		exit(0);
	}
	if (signum != 0) {
		lprintf("[ERROR]: A fatal error has occured and the program has to exit.\n");
		/* lprintf("[ERROR]: %s\n", strerror(signum)); */
	}
	exit(-signum);	// destructor called manually
}

void destructor(int error)
{
	manualDestructorCall = true;
	_destructor(error);
}

static struct program_options parse_program_options(int argc, char* argv[])
{
	struct program_options options = { 0 };
	
	#ifdef SERVER_BUILD
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--foreground")) {
			options.is_foreground = true;
			continue;
		}
		lprintf("[WARNING]: Unknown option %s\n", argv[i]);
	}
	#elif CLIENT_BUILD
	options.is_foreground = true;
	#endif

	return options;
}

void init_program(int argc, char* argv[])
{
	struct sigaction sig = {
		.sa_flags = SA_NOCLDWAIT,
		.sa_handler = SIG_DFL
	};

	#ifdef SERVER_BUILD
	check( prctl(PR_SET_NAME, "jma_Iserver") )
	#elif CLIENT_BUILD
	check( prctl(PR_SET_NAME, "jma_Iclient") )
	#endif
	load_environment();
	check_sig(SIGTERM, _destructor)
	check_sig(SIGINT, _destructor)
	check_sig(SIGPIPE, _destructor)
	check( sigaction(SIGCHLD, &sig, NULL) )
	open_socket();
	options = parse_program_options(argc, argv);
	if (!options.is_foreground) {
		daemonize();
		log_stdio();
	}
}

void set_timeout(int fd)
{
	struct timeval timeout;

	timeout.tv_sec = timeout_seconds;
	timeout.tv_usec = 0;
	check( setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) )
	check( setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval)) )
}

void fill_sockaddr(struct sockaddr_un *sock)
{
	sock->sun_family = AF_UNIX;
	if (strlen(sockPath) >= sizeof(sock->sun_path)) {
		lprintf("[ERROR]: Cannot create socket. Path %s too long (max length " TOSTRING(sizeof(sock->sun_path)-1) ")\n", sockPath);
		destructor(ENAMETOOLONG);
	}
	strcpy(sock->sun_path, sockPath);
}