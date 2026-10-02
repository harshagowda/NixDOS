#ifndef _STDIO_H
#define _STDIO_H
#include "_cdefs.h"
#include <stdarg.h>
__BEGIN_DECLS
typedef struct _nix_file FILE;
extern FILE *stdin, *stdout, *stderr;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 1024
#define FILENAME_MAX 40
typedef long fpos_t;
int   printf(const char *fmt, ...);
int   fprintf(FILE *f, const char *fmt, ...);
int   sprintf(char *buf, const char *fmt, ...);
int   snprintf(char *buf, size_t n, const char *fmt, ...);
int   vprintf(const char *fmt, va_list ap);
int   vfprintf(FILE *f, const char *fmt, va_list ap);
int   vsprintf(char *buf, const char *fmt, va_list ap);
int   vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int   sscanf(const char *s, const char *fmt, ...);
int   puts(const char *s);
int   fputs(const char *s, FILE *f);
int   putchar(int c);
int   fputc(int c, FILE *f);
int   putc(int c, FILE *f);
int   getchar(void);
int   fgetc(FILE *f);
int   getc(FILE *f);
char *fgets(char *buf, int n, FILE *f);
FILE *fopen(const char *name, const char *mode);
int   fclose(FILE *f);
size_t fread(void *buf, size_t size, size_t n, FILE *f);
size_t fwrite(const void *buf, size_t size, size_t n, FILE *f);
int   fseek(FILE *f, long off, int whence);
long  ftell(FILE *f);
void  rewind(FILE *f);
int   feof(FILE *f);
int   ferror(FILE *f);
int   fflush(FILE *f);
int   remove(const char *name);
int   rename(const char *from, const char *to);
void  perror(const char *s);
int   fileno(FILE *f);
__END_DECLS
#endif
