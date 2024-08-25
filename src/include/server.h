#ifndef _SERVER_H
#define _SERVER_H

#ifdef _CLIENT_H
#error "Trying to build the client with server headers included"
#endif
#define __server __attribute__((section(".server"TOSTRING(__COUNTER__))))
#define __client __attribute__((section(".discard.client"TOSTRING(__COUNTER__))))

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <fcntl.h>

/* ************* log.c ************* */
void log_stdio();

/* ************* environment.c ************* */
extern time_t timeout_seconds;

void daemonize();
/* ************* server.c ************* */
extern int sockfd;

void destructor(int signum);
void set_timeout(int fd);
/* ************* request_dispatcher.c ************* */
#define PRIV_NONE		0
#define PRIV_TESTDEV	1
#define PRIV_DEV		2
#define PRIV_ADMIN		3
#define PRIV_SUPERUSER	4

extern int privs;

void dispatch_request();
void report_error_and_die(const char *restrict error);	/* To use after the connection is established */

#endif /* _SERVER_H */