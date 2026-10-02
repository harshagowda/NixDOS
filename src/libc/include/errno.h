#ifndef _ERRNO_H
#define _ERRNO_H
#include "_cdefs.h"
__BEGIN_DECLS
extern int errno;
__END_DECLS
#define ENOENT 2
#define EIO 5
#define EBADF 9
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define EINVAL 22
#define ENOSPC 28
#define ERANGE 34
#endif
