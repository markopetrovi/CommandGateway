#include <app.h>
#include <pwd.h>
#include <grp.h>

char *peerName = "UNIDENTIFIED";
int privs = PRIV_NONE;

static void report_error_and_die(char *error)
{
	int olderrno = errno;

	send_socket(sockfd, "PRINTERR", error);
	send_socket(sockfd, "PRINTERR", "Connection closed.\n");
	lprintf("[ERROR]: %s", error);
	lprintf("[ERROR]: Connection with %s closed.\n", peerName);
	destructor(olderrno);
}

static uid_t get_peer_uid()
{
	struct ucred cred;
	socklen_t s = sizeof(struct ucred);

	if (getsockopt(sockfd, SOL_SOCKET, SO_PEERCRED, &cred, &s) < 0) {
		dlperror("getsockopt");
		report_error_and_die("Unknown peer credentails.\n");
	}
	return cred.uid;
}

static void get_peer_name(uid_t uid)
{
	FILE *fp;
	char *path;
	struct passwd *p;

	check( asprintf(&path, "%s/etc/passwd", rootPath) )
	fp = fopen(path, "r");
	free(path);
	if (fp == NULL) {
		dlperror("fopen");
		report_error_and_die("Cannot get peer name.\n");
	}
	while (p=fgetpwent(fp)) {
		if (p->pw_uid == uid) {
			peerName = malloc(strlen(p->pw_name) + 1);
			if (!peerName) {
				dlperror("malloc");
				report_error_and_die("Dispatcher failed.\n");
			}
			strcpy(peerName, p->pw_name);
			fclose(fp);
			return;
		}
	}
	fclose(fp);
	errno = ENOENT;
	report_error_and_die("Cannot get peer name.\n");
}

static bool find_uid(char* gr_mem[])
{
	for (int i = 0; gr_mem[i] != NULL; i++) {
		if (!strcmp(gr_mem[i], peerName))
			return true;
	}
	return false;
}

static void get_peer_credentails()
{
	FILE *fp;
	char *path;
	struct group *p;
	uid_t uid;

	uid = get_peer_uid();
	if (uid == 0) {
		peerName = "root";
		privs = PRIV_SUPERUSER;
		return;
	}
	get_peer_name(uid);

	check( asprintf(&path, "%s/etc/group", rootPath) )
	fp = fopen(path, "r");
	free(path);
	if (fp == NULL) {
		dlperror("fopen");
		report_error_and_die("Cannot get peer group privileges.\n");
	}
	while (p=fgetgrent(fp)) {
		if (!strcmp(p->gr_name, group_testdev) && find_uid(p->gr_mem))
			privs = PRIV_TESTDEV;
		if (!strcmp(p->gr_name, group_dev) && find_uid(p->gr_mem))
			privs = PRIV_DEV;
		if (!strcmp(p->gr_name, group_admin) && find_uid(p->gr_mem))
			privs = PRIV_ADMIN;
		if (!strcmp(p->gr_name, group_superuser) && find_uid(p->gr_mem))
			privs = PRIV_SUPERUSER;
	}
	fclose(fp);
	errno = ENOENT;
	report_error_and_die("Cannot get peer group privileges.\n");
}

static short parse_commands(struct iovec *io)
{
	if (!strcmp(io[0].iov_base, "END")) {
		return 1;
	}
	if (!strcmp(io[0].iov_base, "info")) {
		print_version();
		return 0;
	}
	if (!strcmp(io[0].iov_base, "PRINT")) {
		print_from_remote(false, io[1].iov_base);
		return 0;
	}
	if (!strcmp(io[0].iov_base, "PRINTERR")) {
		print_from_remote(true, io[1].iov_base);
		return 0;
	}
	return -1;
}

/* command_id args*/
void dispatch_request()
{
	struct iovec io[2];
	short ret = 0;

	check( prctl(PR_SET_NAME, "jma_Idispatcher") )

	get_peer_credentails();
	lprintf("[INFO]: Getting command requests from %s...\n", peerName);
	while (ret == 0) {
		read_socket(sockfd, io);
		ret = parse_commands(io);
		if (ret == -1) {
			errno = ENOSYS;
			char *buf;
			check( asprintf(&buf, "Peer requested unimplemented command: %s\n", buf) )
			report_error_and_die(buf);
		}
		lprintf("[INFO]: Peer %s executed %s command.\n", peerName, io[0].iov_base);
	}
	lprintf("[INFO]: Successfully disconnected from peer %s\n", peerName);
}