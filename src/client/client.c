#include <app.h>

int sockfd;

void open_socket()
{
    struct sockaddr_un sock = { 0 };
	
	fill_sockaddr(&sock);
    check( sockfd=socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0) )
    lprintf("[DEBUG]: Connecting to socket...\n");
    check( connect(sockfd, &sock, sizeof(struct sockaddr_un)) )
    lprintf("[DEBUG]: Connected.\n");
}

int main(int argc, char *argv[])
{
    init_program();
}