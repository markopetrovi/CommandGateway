#ifndef _SERVER_H
#define _SERVER_H

#ifdef _CLIENT_H
#error "Trying to build the client with server headers included"
#endif
#define __server __attribute__((section(".server"TOSTRING(__COUNTER__))))
#define __client __attribute__((section(".discard.client"TOSTRING(__COUNTER__))))

/* ************* log.c ************* */
void log_stdio();

/* ************* environment.c ************* */
void daemonize();

#endif /* _SERVER_H */