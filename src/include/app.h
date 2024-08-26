#ifndef _APP_H
#define _APP_H
#define _GNU_SOURCE

#include <compiler.h>

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>
#include <sys/file.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <fcntl.h>

#ifdef CLIENT_BUILD
#include <client.h>
#elif defined(SERVER_BUILD)
#include <server.h>
#else
#error "Please specify whether you're building the server or the client."
#endif

#define check(...)									\
	if (unlikely((__VA_ARGS__) < 0)) {				\
		dlperror(#__VA_ARGS__);						\
		destructor(errno);							\
	}

#define check_sig(sig, handler)									\
	if (unlikely(signal(sig, handler) == SIG_ERR)) {			\
		dlperror("signal");										\
		destructor(errno);										\
	}

typedef enum {
	ABORT,
	FALLBACK,
	DEFAULT,
	SPECIAL
} Action;

/* ************* main.c ************* */
extern int sockfd;

void open_socket();
/* ************* request_dispatcher.c ************* */
#define PRIV_NONE		0
#define PRIV_TESTDEV	1
#define PRIV_DEV		2
#define PRIV_ADMIN		3
#define PRIV_SUPERUSER	4

extern int privs;

void dispatch_request();
void report_error_and_die(const char *restrict error);	/* To use after the connection is established */
/* ************* environment.c ************* */
extern const char version[];
extern char *sockPath, *rootPath;
extern int log_level;
extern bool shouldDeleteSocket;

bool does_file_exist();
void clear_environment();
void init_program(int argc, char* argv[]);
void destructor(int signum);
void set_timeout(int fd);
void fill_sockaddr(struct sockaddr_un *sock);
/* ************* log.c ************* */
#define LOG_NONE	0
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_INFO	3
#define LOG_DEBUG	4

/* Following logging functions preserve errno */
/* THREAD SAFE */
int lfprintf(FILE *restrict stream, const char *restrict format, ...);
int lprintf(const char *restrict format, ...);
int lvfprintf(FILE *restrict stream, const char *restrict format, va_list ap, Action *_Nullable ac);
void lperror(const char *s);
#define dlperror(s)	lperror(__FILE__ ":" TOSTRING(__LINE__) ":" s)
/* ************* IO.c ************* */
ssize_t lread(int fd, void *buf, size_t count);
ssize_t lwrite(int fd, const void *buf, size_t count);

#endif /* APP_H */