#ifndef _UNISTD_H
#define _UNISTD_H
#include "_cdefs.h"
#include <sys/types.h>
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
__BEGIN_DECLS
int     close(int fd);
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
off_t   lseek(int fd, off_t off, int whence);
int     unlink(const char *name);
int     access(const char *name, int mode);
char   *getcwd(char *buf, size_t n);
int     chdir(const char *path);
unsigned sleep(unsigned s);
int     usleep(unsigned us);
__END_DECLS
#endif
