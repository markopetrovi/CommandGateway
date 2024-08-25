#ifndef _COMPILER_H
#define _COMPILER_H

#define BUF_SIZE 300    /* Default size for temporary, stack-allocated buffers */
#define bool short int
#define false 0
#define true 1
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define _Nullable

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#endif /* _COMPILER_H */