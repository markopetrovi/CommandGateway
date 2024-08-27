#include <app.h>

static void check_value(ssize_t ret, struct msghdr *msg)
{
	if (ret < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			lprintf("[ERROR]: Socket taking too long to respond.\n");
		dlperror("recvmsg/sendmsg");
		lprintf("[ERROR]: Failed to transmit data.\n");
		destructor(errno);
	}
}

static void add_null_termination(ssize_t ret, struct msghdr *msg)
{
	char *string = msg->msg_iov->iov_base;

	if (ret == 0 || !string[ret-1])
		return;
	if (ret == msg->msg_iov->iov_len) {
		char *str = malloc(ret+1);
		if (unlikely(!str)) {
			dlperror("malloc");
			destructor(errno);
		}
		memcpy(str, string, ret);
		str[ret] = '\0';
		lprintf("[DEBUG]: Got non-null-terminated string: %s\n", str);

		/* Danger zone */
		if (string[ret] != '\0') {
			lprintf("[ERROR]: Buffer too small for NULL byte and got non-null-terminated string: %s\n", str);
			destructor(ENOMEM);
		}
		free(str);
		return;
	}
	string[ret] = '\0';
}

static bool check_eor(ssize_t ret, struct msghdr *msg)
{
	char *string = msg->msg_iov->iov_base;

	if (msg->msg_flags & MSG_EOR)
		return true;
	if (ret < 3)
		return false;
	if (string[0] == 'E' && string[1] == 'O' && string[2] == 'R')
		return true;
	return false;
}

int sread(int fd, struct iovec *buf, int count)
{
	struct msghdr msg = { 0 };
	ssize_t ret;
	
	if (count < 1) {
		lprintf("[DEBUG]: sread called with %i-length buffer\n", count);
		return 0;
	}
	msg.msg_iovlen = 1;
	for (int i = 0; i < count-1; i++) {
		msg.msg_iov = &buf[i];
		ret = recvmsg(fd, &msg, 0);
		check_value(ret, &msg);
		add_null_termination(ret, &msg);
		if (check_eor(ret, &msg)) {
			lprintf("[DEBUG]: Received MSG_EOR earlier than expected. Expected %i packets, got %i\n", count, i+1);
			return i+1;
		}
	}
	msg.msg_iov = &buf[count-1];
	ret = recvmsg(fd, &msg, 0);
	check_value(ret, &msg);
	add_null_termination(ret, &msg);
	if(!check_eor(ret, &msg))
		lprintf("[DEBUG]: Didn't receive MSG_EOR at the end of transmission. There could be more packets waiting in the queue.\n");
	return count;
}

void swrite(int fd, struct iovec *buf, int count)
{
	struct msghdr msg = { 0 };
	ssize_t ret;

	if (count < 1) {
		lprintf("[DEBUG]: swrite called with %i-length buffer\n", count);
		return;
	}
	msg.msg_iovlen = 1;
	for (int i = 0; i < count-1; i++) {
		msg.msg_iov = &buf[i];
		ret = sendmsg(fd, &msg, 0);
		check_value(ret, &msg);
		if (ret < msg.msg_iov->iov_len)
			lprintf("[DEBUG]: Transmitted less bytes than expected. Expected: %lu Got: %lu\n", msg.msg_iov->iov_len, ret);
	}
	msg.msg_iov = &buf[count-1];
	ret = sendmsg(fd, &msg, MSG_EOR);
	check_value(ret, &msg);
	if (ret < msg.msg_iov->iov_len)
		lprintf("[DEBUG]: Transmitted less bytes than expected. Expected: %lu Got: %lu\n", msg.msg_iov->iov_len, ret);
}

void send_socket(int fd, char *anc, char *data)
{
	struct iovec io[2];

	io[0].iov_base = anc;
	io[0].iov_len = strlen(anc);
	io[1].iov_base = data;
	io[1].iov_len = strlen(data);
	swrite(fd, io, 2);
}

void read_socket(int fd, struct iovec *io)
{
	static char bufanc[BUF_SIZE+1] = { 0 };
	static char bufdata[BUF_SIZE+1] = { 0 };

	io[0].iov_base = bufanc;
	io[0].iov_len = BUF_SIZE;
	io[1].iov_base = bufdata;
	io[1].iov_len = BUF_SIZE;

	sread(fd, io, 2);
}