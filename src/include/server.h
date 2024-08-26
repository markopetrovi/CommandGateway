#ifndef _SERVER_H
#define _SERVER_H

#ifdef _CLIENT_H
#error "Trying to build the client with server headers included"
#endif
#define __server __attribute__((section(".server"TOSTRING(__COUNTER__))))
#define __client __attribute__((section(".discard.client"TOSTRING(__COUNTER__))))

struct argv_options {
    bool is_foreground;
};

/* ************* log.c ************* */
void log_stdio();

/* ************* environment.c ************* */
extern char *log_path, *group_testdev, *group_dev, *group_admin, *group_superuser;

void daemonize();

#endif /* _SERVER_H */