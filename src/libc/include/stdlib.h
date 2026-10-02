#ifndef _STDLIB_H
#define _STDLIB_H
#include "_cdefs.h"
__BEGIN_DECLS
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7FFF
void *malloc(size_t n);
void *calloc(size_t n, size_t size);
void *realloc(void *p, size_t n);
void  free(void *p);
int   atoi(const char *s);
long  atol(const char *s);
double atof(const char *s);
long  strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
double strtod(const char *s, char **end);
int   abs(int v);
long  labs(long v);
int   rand(void);
void  srand(unsigned s);
void  qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
char *getenv(const char *name);
void  exit(int code) __attribute__((noreturn));
void  _Exit(int code) __attribute__((noreturn));
void  abort(void) __attribute__((noreturn));
int   atexit(void (*fn)(void));
int   system(const char *cmd);
__END_DECLS
#endif
