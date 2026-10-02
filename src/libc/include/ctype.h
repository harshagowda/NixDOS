#ifndef _CTYPE_H
#define _CTYPE_H
#include "_cdefs.h"
__BEGIN_DECLS
int isdigit(int c); int isalpha(int c); int isalnum(int c); int isspace(int c);
int isupper(int c); int islower(int c); int isxdigit(int c); int isprint(int c);
int ispunct(int c); int iscntrl(int c); int isgraph(int c);
int toupper(int c); int tolower(int c);
__END_DECLS
#endif
