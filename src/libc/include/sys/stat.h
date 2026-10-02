#ifndef _SYS_STAT_H
#define _SYS_STAT_H
#include "../_cdefs.h"
#include <sys/types.h>
struct stat { off_t st_size; mode_t st_mode; time_t st_mtime; };
#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IREAD S_IRUSR
#define S_IWRITE S_IWUSR
__BEGIN_DECLS
int stat(const char *name, struct stat *st);
int fstat(int fd, struct stat *st);
int mkdir(const char *path, mode_t mode);
__END_DECLS
#endif
