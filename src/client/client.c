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
    if (argc <= 1) {
        fprintf(stderr, "[ERROR]: Missing command parameter.\n");
        destructor(EINVAL);
    }
    init_program(argc, argv);

    if (argc == 2)
        send_socket(sockfd, argv[1], "");
    else {
        for (int i = 3; i < argc; i++)
            *(argv[i]-1) = ' ';
        send_socket(sockfd, argv[1], argv[2]);
    }
    dispatch_request();
    send_socket(sockfd, "END", "");
}