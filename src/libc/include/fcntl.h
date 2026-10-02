#ifndef _FCNTL_H
#define _FCNTL_H
#include "_cdefs.h"
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0x40
#define O_EXCL   0x80
#define O_TRUNC  0x200
#define O_APPEND 0x400
#define O_BINARY 0
#define O_TEXT   0
__BEGIN_DECLS
int open(const char *name, int flags, ...);
__END_DECLS
#endif
