#include <app.h>

const char version[] = "CommandGateway 1.0 (C) 2024 Marko Petrovic";
bool shouldDeleteSocket = false;
static bool manualDestructorCall = false;
static time_t timeout_seconds = 5;

int log_level = LOG_WARNING;
char *sockPath = NULL;
char __server *rootPath = NULL;
char __server *group_testdev = "jmatestdev", *group_dev = "jmadev";
char __server *group_admin = "jmaadmin", *group_superuser = "jmaroot";


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
	char *groupName;

	rootPath = getenv("ROOT_PATH");
	if (!rootPath) {
		lprintf("[ERROR]: Missing ROOT_PATH environment variable.\n");
		destructor(EINVAL);
	}
	if (!does_file_exist(rootPath)) {
		lprintf("[ERROR]: Supplied ROOT_PATH folder does not exist\n");
		destructor(ENOENT);
	}

	groupName = getenv("GROUP_TESTDEV");
	if (groupName)
		group_testdev = groupName;
	groupName = getenv("GROUP_DEV");
	if (groupName)
		group_dev = groupName;
	groupName = getenv("GROUP_ADMIN");
	if (groupName)
		group_admin = groupName;
	groupName = getenv("GROUP_SUPERUSER");
	if (groupName)
		group_superuser = groupName;
	lprintf("[INFO]: Using %s for GROUP_TESTDEV\n", group_testdev);
	lprintf("[INFO]: Using %s for GROUP_DEV\n", group_dev);
	lprintf("[INFO]: Using %s for GROUP_ADMIN\n", group_admin);
	lprintf("[INFO]: Using %s for GROUP_SUPERUSER\n", group_superuser);
}

static void load_environment()
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
				log_level = LOG_WARNING;
				#ifdef SERVER_BUILD
				lprintf("[WARNING]: Unknown LOG_LEVEL value. Using the default value: LOG_WARNING.\n");
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
	if (signum != 0) {
		lprintf("[ERROR]: A fatal error has occured and the program has to exit.\n");
		lprintf("[ERROR]: %s\n", strerror(signum));
	}
	exit(signum);	// destructor called manually
}

void destructor(int error)
{
	manualDestructorCall = true;
	_destructor(error);
}

static __server struct argv_options parse_server_options(int argc, char* argv[])
{
	struct argv_options options = { 0 };
	
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--foreground")) {
			options.is_foreground = true;
			continue;
		}
		lprintf("[WARNING]: Unknown option %s\n", argv[i]);
	}

	return options;
}

void init_program(int argc, char* argv[])
{
	struct argv_options options;
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
	options = parse_server_options(argc, argv);
	if (!options.is_foreground) {
		daemonize();
		log_stdio();
	}
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