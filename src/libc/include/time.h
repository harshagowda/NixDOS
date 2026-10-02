#ifndef _TIME_H
#define _TIME_H
#include "_cdefs.h"
#include <sys/types.h>
typedef long clock_t;
#define CLOCKS_PER_SEC 1000
__BEGIN_DECLS
time_t time(time_t *t);
clock_t clock(void);
__END_DECLS
#endif
