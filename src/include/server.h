#ifndef _SERVER_H
#define _SERVER_H

#ifdef _CLIENT_H
#error "Trying to build the client with server headers included"
#endif
#define __server __attribute__((section(".server.text")))
#define __client __attribute__((section(".discard.client.text")))
#define __server_data __attribute__((section(".server.data")))
#define __client_data __attribute__((section(".discard.client.data")))

struct argv_options {
    bool is_foreground;
};

/* ************* log.c ************* */
void log_stdio();

/* ************* environment.c ************* */
extern char *log_path, *group_testdev, *group_dev, *group_admin, *group_superuser;

void daemonize();

#endif /* _SERVER_H */