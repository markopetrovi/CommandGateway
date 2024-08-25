#ifndef _SERVER_H
#define _SERVER_H

#ifdef _CLIENT_H
#error "Trying to build the client with server headers included"
#endif
#define __server __attribute__((section(".text")))
#define __client __attribute__((section(".discard.text")))



#endif /* _SERVER_H */