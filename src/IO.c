#include <app.h>

ssize_t lread(int fd, void *buf, size_t count)
{
	ssize_t ret = read(fd, buf, count);

	if (ret < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			report_error_and_die("Socket taking too long to respond.\n");
		dlperror("read");
		report_error_and_die("Failed to read the request.\n");
	}

	return ret;
}

ssize_t lwrite(int fd, const void *buf, size_t count)
{
	ssize_t ret = write(fd, buf, count);
	
	if (ret < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			report_error_and_die("Socket taking too long to respond.\n");
		dlperror("write");
		report_error_and_die("Failed to send the response.\n");
	}
	
	return ret;
}