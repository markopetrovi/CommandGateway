#include <app.h>

const char version[] = "CommandGateway 1.0 (C) 2024 Marko Petrovic";
bool shouldDeleteSocket = false;
static bool manualDestructorCall = false;
static time_t timeout_seconds = 5;

int log_level = LOG_WARNING;
char *sockPath = NULL;
char __server *rootPath = NULL;

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

bool does_file_exist()
{
	int fd;
	fd = open(sockPath, O_PATH);
	if (fd < 0)
		return false;
	check( close(fd) )
	return true;
}

void clear_environment()
{
	unsetenv("SOCK_PATH");
	unsetenv("CONTAINER_SUBVOL_ID");
	unsetenv("LOG_LEVEL");
	unsetenv("TIMEOUT");
}

void __server load_server_env()
{
	char *containerID;

	containerID = getenv("CONTAINER_SUBVOL_ID");
	if (!containerID) {
		lprintf("[ERROR]: Missing CONTAINER_SUBVOL_ID environment variable.\n");
		destructor(EINVAL);
	}
	check( asprintf(&rootPath, "/var/lib/docker/btrfs/subvolumes/%s", containerID) )
	if (!does_file_exist(rootPath)) {
		lprintf("[ERROR]: Supplied container does not exist in /var/lib/docker/btrfs/subvolumes\n");
		destructor(ENOENT);
	}
}

void load_environment()
{
	char *log_level_str, *relative_sockPath, *timeout_str;

	#ifdef SERVER_BUILD
	load_server_env();
	#endif
	relative_sockPath = getenv("SOCK_PATH");
	if (!relative_sockPath) {
		#ifdef SERVER_BUILD
		lprintf("[ERROR]: Missing SOCK_PATH environment variable.\n");
		destructor(EINVAL);
		#elif CLIENT_BUILD
		relative_sockPath = "/tmp/cgsocket";
		#endif
	}
	#ifdef SERVER_BUILD
	check( asprintf(&sockPath, "%s/%s", rootPath, relative_sockPath) )
	#elif CLIENT_BUILD
	sockPath = relative_sockPath;
	#endif

	timeout_str = getenv("TIMEOUT");
	if (timeout_str) {
		timeout_seconds = atoi(timeout_str);
		if (!timeout_seconds)
			lprintf("[WARNING]: Socket timeout disabled. Operations can block indefinitely.\n");
	}

	log_level_str = getenv("LOG_LEVEL");
	if (log_level_str) {
		switch (log_level_str[0]) {
			case 'N':
				log_level = LOG_NONE;
				break;
			case 'E':
				log_level = LOG_ERROR;
				break;
			case 'W':
				log_level = LOG_WARNING;
				break;
			case 'I':
				log_level = LOG_INFO;
				break;
			case 'D':
				log_level = LOG_DEBUG;
				break;
			default:
				log_level = LOG_INFO;
				#ifdef SERVER_BUILD
				lprintf("[WARNING]: Unknown LOG_LEVEL value. Using the default value.\n");
				#endif
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
	exit(signum);	// destructor called manually
}

void destructor(int error)
{
	manualDestructorCall = true;
	_destructor(error);
}

void init_program()
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
	check( sigaction(SIGCHLD, &sig, NULL) )
	open_socket();
	#ifdef SERVER_BUILD
	daemonize();
	log_stdio();
	#endif
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