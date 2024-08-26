#include <app.h>
#include <pwd.h>
#include <grp.h>

static char *clientName = "UNIDENTIFIED";
int privs = PRIV_NONE;

#ifdef SERVER_BUILD
void report_error_and_die(const char *restrict error)
{
	static const char r[] = "Connection closed.\n";
	int olderrno = errno;

	write(sockfd, error, strlen(error));
	write(sockfd, r, sizeof(r));
	lprintf("[ERROR]: %s\n", error);
	lprintf("[ERROR]: Connection with client %s closed.\n", clientName);
	destructor(olderrno);
}
#elif CLIENT_BUILD
void report_error_and_die(const char *restrict error)
{
	lprintf("[ERROR] [Client]: %s\n", error);
	destructor(errno);
}
#endif

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
			clientName = malloc(strlen(p->pw_name) + 1);
			if (!clientName) {
				dlperror("malloc");
				report_error_and_die("Server failed.");
			}
			strcpy(clientName, p->pw_name);
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
		if (!strcmp(gr_mem[i], clientName))
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

/* command_id args*/
void dispatch_request()
{
	char *buf = malloc(BUF_SIZE+1);

	if (unlikely(!buf)) {
		dlperror("malloc");
		report_error_and_die("Server failed.");
	}

	buf[BUF_SIZE] = '\0';
	buf[0] = '\0';
	check( prctl(PR_SET_NAME, "jma_Idispatcher") )
	set_timeout(sockfd);

	get_peer_credentails();
	lprintf("[INFO]: Established connection with %s\n", clientName);
	lread(sockfd, buf, BUF_SIZE);

	/* Check command */
	static const char info[] = "info";

	if (!strcmp(buf, info)) {
		lwrite(sockfd, version, strlen(version));
		goto out;
	}

	errno = ENOSYS;
	lprintf("[WARNING]: Client %s requested unimplemented command: %s\n", clientName, buf);
	report_error_and_die("Unimplemented command.\n");
out:
	lprintf("[INFO]: Client %s executed %s command.\n", clientName, buf);
	lprintf("[INFO]: Successfully disconnected from client %s\n", clientName);
	destructor(0);
}