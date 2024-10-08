#ifndef _SERVER_H
#define _SERVER_H

#ifdef _CLIENT_H
#error "Trying to build the client with server headers included"
#endif
#define __server __attribute__((section(".server.text")))
#define __client __attribute__((section(".discard.client.text"), unused))
#define __server_data __attribute__((section(".server.data")))
#define __client_data __attribute__((section(".discard.client.data"), unused))

/* ************* environment.c ************* */
void daemonize();

#endif /* _SERVER_H */