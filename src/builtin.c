#include <app.h>

void print_version()
{
	send_socket(sockfd, "PRINT", version);
	send_socket(sockfd, "END", "");
}

void print_from_remote(bool isError, const char *string)
{
	if (privs < PRIV_DEV) {
		lprintf("[WARNING]: Unprivileged peer %s trying to write to log\n", peerName);
		send_socket(sockfd, "PRINTERR", "Permission denied");
	}
	if (isError)
		lprintf("[ERROR] [REMOTE]: %s", string);
	else
		lprintf("[REMOTE]: %s", string);
	send_socket(sockfd, "END", "");
}