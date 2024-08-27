#include <app.h>

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
		lprintf("[ERROR] [REMOTE]: %s", string);
	else if (type == MESSAGE_REMOTE)
		lprintf("[REMOTE]: %s", string);
	else
		lprintf("%s", string);
	send_socket(sockfd, "END", "");
}