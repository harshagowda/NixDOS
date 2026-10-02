#ifndef _MATH_H
#define _MATH_H
#include "_cdefs.h"
__BEGIN_DECLS
#define M_PI 3.14159265358979323846
#define HUGE_VAL __builtin_huge_val()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")
double sin(double x); double cos(double x); double tan(double x);
double atan(double x); double atan2(double y, double x);
double asin(double x); double acos(double x);
double sqrt(double x); double fabs(double x); double floor(double x); double ceil(double x);
double pow(double x, double y); double exp(double x); double log(double x); double log10(double x);
double fmod(double x, double y); double round(double x); double trunc(double x);
float sinf(float x); float cosf(float x); float sqrtf(float x); float fabsf(float x);
float floorf(float x); float ceilf(float x); float atan2f(float y, float x); float tanf(float x);
long lrint(double x); long lround(double x);
__END_DECLS
#endif
