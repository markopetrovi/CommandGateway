#ifndef _CLIENT_H
#define _CLIENT_H

#ifdef _SERVER_H
#error "Trying to build the server with client headers included"
#endif
#define __server __attribute__((section(".discard.server.text")))
#define __client __attribute__((section(".client.text")))
#define __server_data __attribute__((section(".discard.server.data")))
#define __client_data __attribute__((section(".client.data")))

struct argv_options { char unimplemented; };

#endif /* _CLIENT_H */