#include <app.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/wait.h>

extern char **environ;

void print_version()
{
	send_socket(sockfd, "PRINT", version);
	send_socket(sockfd, "END", "");
}

void print_from_remote(short type, const char *string)
{
	if (privs < PRIV_DEV) {
		lprintf("[WARNING]: Unprivileged peer %s trying to write to log\n", peerName);
		send_socket(sockfd, "PRINTERR", "Permission denied\n");
		send_socket(sockfd, "END", "");
		return;
	}
	if (type == MESSAGE_ERROR)
		lprintf("[ERROR] [REMOTE %s]: %s", peerName, string);
	else if (type == MESSAGE_REMOTE)
		lprintf("[REMOTE %s]: %s", peerName, string);
	else
		lprintf("%s", string);
	send_socket(sockfd, "END", "");
}

static bool check_ownership(char *path)
{
	struct statx fileinfo;
	int ret;

	ret = statx(AT_FDCWD, path, 0, STATX_UID, &fileinfo);
	if (ret < 0 && errno == ENOENT)
		return -1;
	if (ret < 0) {
		int olderrno = errno;
		dlperror("statx");
		send_socket(sockfd, "PRINTERR", "Cannot stat the executable\n");
		send_socket(sockfd, "PRINTERR", "Connection closed.\n");
		send_socket(sockfd, "END", "");
		lprintf("[ERROR]: Connection with %s closed.\n", peerName);
		destructor(olderrno);
	}

	return fileinfo.stx_uid == getuid();
}

static void enable_waiting()
{
	struct sigaction sig = {
		.sa_handler = SIG_DFL
	};
	check( sigaction(SIGCHLD, &sig, NULL) )
}

static void disable_waiting()
{
	struct sigaction sig = {
		.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP,
		.sa_handler = SIG_DFL
	};
	check( sigaction(SIGCHLD, &sig, NULL) )
}

void dummy() { return; }
int terminate_child(int pid)
{
	pid_t ret;
	int wstatus;

	if (kill(pid, SIGTERM) < 0 && errno != ESRCH) {
		dlperror("kill");
		destructor(errno);
	}

	check_sig(SIGALRM, dummy)
	alarm(timeout_seconds);
	ret = waitpid(pid, &wstatus, 0);
	alarm(0);
	check_sig(SIGALRM, SIG_DFL)

	if (ret == pid) {
		if (!WIFEXITED(wstatus)) {
			lprintf("[DEBUG]: waitpid on %i completed, but WIFEXITED == false\n", pid);
			return -EINVAL;
		}
		return WEXITSTATUS(wstatus);
	}
	else if (ret < 0) {
		if (errno == ECHILD) {
			lprintf("[DEBUG]: Tried to terminate a non-existent child %i or SA_NOCLDWAIT is on.\n", pid);
			return -ECHILD;
		}
		dlperror("waitpid");
		destructor(errno);
	}

	if (kill(pid, SIGKILL) < 0 && errno != ESRCH) {
		dlperror("kill");
		destructor(errno);
	}
	waitpid(pid, &wstatus, 0);
	return -EINTR;
}

static __client char *find_in_path(char *command)
{
	char *PATH, *execPath;

	if (privs < PRIV_SUPERUSER) {
		lprintf("[WARNING]: Unprivileged peer %s trying to execute non-native command %s", peerName, command);
		send_socket(sockfd, "PRINTERR", "Permission denied");
		send_socket(sockfd, "END", "");
		return 0;
	}
	PATH = getenv("PATH");
	if (!PATH)
		return NULL;
	PATH = strdup(PATH);
	char *pathDir = strtok(PATH, ":");
	while (pathDir != NULL) {
		check( asprintf(&execPath, "%s/%s", pathDir, command) )
		if (check_ownership(execPath))
			break;
		free(execPath);
		pathDir = strtok(NULL, ":");
	}
	free(PATH);
	return (pathDir) ? execPath : NULL;
}

short try_external(char *command, char *string)
{
	static char buf[BUF_SIZE+1];
	ssize_t ret;
	char *dirPath, *execPath;
	int pipefd[2], pid, nullfd;

	check( ret = readlink("/proc/self/exe", buf, BUF_SIZE) )
	buf[ret] = '\0';
	dirPath = dirname(buf);
	check( asprintf(&execPath, "%s/%s", dirPath, command) )

	if (!check_ownership(execPath)) {
		free(execPath);
		#ifdef CLIENT_BUILD
		execPath = find_in_path(command);
		if (!execPath)
			return -1;
		#elif SERVER_BUILD
		return -1;
		#endif
	}
	check( pipe(pipefd) )
	enable_waiting();
	check( pid = fork() )
	if (pid == 0) {
		check( close(pipefd[0]) )
		check( nullfd = open("/dev/null", O_RDWR | O_CLOEXEC) )

		check( dup2(nullfd, STDIN_FILENO) )
		check( dup2(pipefd[1], STDOUT_FILENO) )
		check( dup2(pipefd[1], STDERR_FILENO) )

		if (likely(nullfd > STDERR_FILENO))
			check( close(nullfd) )
		if (likely(pipefd[1] > STDERR_FILENO))
			check( close(pipefd[1]) )
		char *argv[] = {command, string, NULL};
		execve(execPath, argv, environ);
		int olderrno = errno;
		if (!options.is_foreground)
			log_stdio();	/* Reopen log file so that flock() works on separate fds */
		send_socket(sockfd, "PRINTERR", "Failed to execute command");
		dlperror("execve");
		exit(olderrno);
	}
	free(execPath);
	check( close(pipefd[1]) )
	while (ret = read(pipefd[0], buf, BUF_SIZE)) {
		if (ret < 0) {
			dlperror("read");
			destructor(errno);
		}
		buf[ret] = '\0';
		send_socket(sockfd, "OUTPUT", buf);
	}
	terminate_child(pid);
	disable_waiting();
	send_socket(sockfd, "END", "");
	return 0;
}