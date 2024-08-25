#ifndef _CLIENT_H
#define _CLIENT_H

#ifdef _SERVER_H
#error "Trying to build the server with client headers included"
#endif
#define __server __attribute__((section(".discard.text")))
#define __client __attribute__((section(".text")))



#endif /* _CLIENT_H */