#include <app.h>

const char version[] = "CommandGateway 1.0 (C) 2024 Marko Petrovic";
int log_level = LOG_WARNING;
char *sockPath = NULL;
char __server *rootPath = NULL;
time_t __server timeout_seconds = 5;

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

bool check_existing()
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

void __server load_specific_env()
{
	char *containerID, *timeout_str;

	containerID = getenv("CONTAINER_SUBVOL_ID");
	if (!containerID) {
		lprintf("[ERROR]: Missing CONTAINER_SUBVOL_ID environment variable.\n");
		destructor(EINVAL);
	}
	check( asprintf(&rootPath, "/var/lib/docker/btrfs/subvolumes/%s", containerID) )
	if (!check_existing(rootPath)) {
		lprintf("[ERROR]: Supplied container does not exist in /var/lib/docker/btrfs/subvolumes\n");
		destructor(ENOENT);
	}

	timeout_str = getenv("TIMEOUT");
	if (timeout_str) {
		timeout_seconds = atoi(timeout_str);
		if (!timeout_seconds)
			lprintf("[WARNING]: Socket timeout disabled. Operations can block indefinitely.\n");
	}
}

void __client load_specific_env()
{

}

void load_environment()
{
	char *log_level_str, *relative_sockPath;

	load_specific_env();
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