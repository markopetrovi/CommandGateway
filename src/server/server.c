#include <app.h>

int sockfd = -1;
static bool opened = false;
static bool manualHandlerCall = false;

void _destructor(int signum)
{
	const char *signal;

	if (sockfd != -1)
		close(sockfd);
	if (opened)
		unlink(sockPath);
	if (!manualHandlerCall) {
		signal = sigdescr_np(signum);
		if (!signal)
			signal = "UNKNOWN";
		lprintf("[ERROR]: Received signal %s - exiting...\n", signal);
		exit(0);
	}
	exit(signum);	// destructor called manually
}

void destructor(int error)
{
	manualHandlerCall = true;
	_destructor(error);
}

static void fill_sockaddr(struct sockaddr_un *sock)
{
	sock->sun_family = AF_UNIX;
	if (strlen(sockPath) >= sizeof(sock->sun_path)) {
		lprintf("[ERROR]: Cannot create socket. Path %s too long (max length " TOSTRING(sizeof(sock->sun_path)-1) ")\n", sockPath);
		destructor(ENAMETOOLONG);
	}
	strcpy(sock->sun_path, sockPath);
}

void set_timeout(int fd)
{
	struct timeval timeout;

	timeout.tv_sec = timeout_seconds;
	timeout.tv_usec = 0;
	check( setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) )
	check( setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval)) )
}

static void get_server_info()
{
	struct ucred cred;
	socklen_t s = sizeof(struct ucred);
	int fd;
	char *commPath;
	char *buf = malloc(BUF_SIZE+1);

	if (unlikely(!buf)) {
		dlperror("malloc");
		destructor(errno);
	}

	buf[BUF_SIZE] = '\0';
	if (getsockopt(sockfd, SOL_SOCKET, SO_PEERCRED, &cred, &s) < 0) {
		dlperror("getsockopt");
		lprintf("[WARNING]: Unknown process holds the socket open.\n");
	}
	else {
		check( asprintf(&commPath, "/proc/%i/comm", cred.pid) )
		fd = open(commPath, O_RDONLY);
		if (fd < 0) {
			dlperror("open");
			lprintf("[INFO]: Socket held open by process UNKNOWN pid: %i\n", cred.pid);
			goto out;
		}
		if (read(fd, buf, BUF_SIZE) <= 0) {
			dlperror("read");
			lprintf("[INFO]: Socket held open by process UNKNOWN pid: %i\n", cred.pid);
			goto out;
		}
		
		lprintf("[INFO]: Socket held open by process %s pid: %i\n", buf, cred.pid);
		
out:
		if (fd >= 0)
			check( close(fd) )
		free(commPath);
	}

	if (write(sockfd, "info", 5) < 0)
		dlperror("write");

	if (read(sockfd, buf, BUF_SIZE) > 0) {
		lprintf("[INFO]: Process responded to info query with: %s\n", buf);
	}
	else {
		dlperror("read");
		lprintf("[INFO]: Process didn't respond to info query.");
	}
	free(buf);
}

void dummy() { return; }
static void open_socket()
{
	struct sockaddr_un sock = { 0 };
	
	fill_sockaddr(&sock);
	check( sockfd=socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0) )
	if (does_file_exist()) {
		check_sig(SIGALRM, dummy)
		alarm(1);	// Interrupt connect if taking too long
		if (connect(sockfd, &sock, sizeof(struct sockaddr_un))) {
			alarm(0);
			check_sig(SIGALRM, SIG_DFL)
			lprintf("[WARNING]: Inactive socket found at %s\tDeleting it...\n", sockPath);
			check( unlink(sockPath) )
		}
		else {
			alarm(0);
			check_sig(SIGALRM, SIG_DFL)
			set_timeout(sockfd);
			lprintf("[ERROR]: Socket already in use. Aborting...\n");
			get_server_info();
			destructor(EADDRINUSE);
		}
	}

	check( listen(sockfd, 5) )
	check( bind(sockfd, &sock, sizeof(struct sockaddr_un)) )
	opened = true;
}

int main()
{
	int fd, pid;
	sigset_t handled;
	struct sigaction sig = {
		.sa_flags = SA_NOCLDWAIT,
		.sa_handler = SIG_DFL
	};

	check( prctl(PR_SET_NAME, "jma_Iserver") )
	load_environment();
	check_sig(SIGTERM, _destructor)
	check_sig(SIGINT, _destructor)
	check( sigaction(SIGCHLD, &sig, NULL) )
	open_socket();
	daemonize();
	log_stdio();

	check( sigaddset(&handled, SIGTERM) )
	check( sigaddset(&handled, SIGINT) )
	while (1) {
		check( fd = accept4(sockfd, NULL, NULL, SOCK_CLOEXEC) )
		check( sigprocmask(SIG_BLOCK, &handled, NULL) )	// Prevent race condition in which the child calls destructor() too early
		check( pid = fork() )
		if (pid == 0) {
			sockfd = fd;
			opened = false;
			check( sigprocmask(SIG_UNBLOCK, &handled, NULL) )
			check( close(sockfd) )
			log_stdio();	// Reopen log file so that flock() works on separate fds
			check( kill(pid, SIGCONT) )
			dispatch_request();
			destructor(0);
		}
		raise(SIGSTOP);
		check( sigprocmask(SIG_UNBLOCK, &handled, NULL) )
	}

	return 0;
}