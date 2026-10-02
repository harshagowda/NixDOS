/* NixDOS 2 - math library for native programs, using the x87 FPU */
#include <math.h>

double fabs(double x) { return __builtin_fabs(x); }
double sqrt(double x) { double r; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }

double sin(double x) { double r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
double cos(double x) { double r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }

double tan(double x)
{
    double r, one;
    __asm__("fptan" : "=t"(one), "=u"(r) : "0"(x));
    return r;
}

double atan2(double y, double x)
{
    double r;
    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

double atan(double x) { return atan2(x, 1.0); }
double asin(double x) { return atan2(x, sqrt((1.0 - x) * (1.0 + x))); }
double acos(double x) { return atan2(sqrt((1.0 - x) * (1.0 + x)), x); }

static double round_mode(double x, unsigned short mode)
{
    unsigned short cw, ncw;
    double r;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    ncw = (unsigned short)((cw & ~0x0C00) | mode);
    __asm__ volatile("fldcw %0" : : "m"(ncw));
    __asm__ volatile("frndint" : "=t"(r) : "0"(x));
    __asm__ volatile("fldcw %0" : : "m"(cw));
    return r;
}

double floor(double x) { return round_mode(x, 0x0400); }
double ceil(double x) { return round_mode(x, 0x0800); }
double trunc(double x) { return round_mode(x, 0x0C00); }
double round(double x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }
long lrint(double x) { return (long)round_mode(x, 0x0000); }
long lround(double x) { return (long)round(x); }

double fmod(double x, double y)
{
    double r;
    __asm__("1: fprem; fnstsw %%ax; sahf; jp 1b" : "=t"(r) : "0"(x), "u"(y) : "ax", "cc");
    return r;
}

/* 2^x for any x, via f2xm1 + fscale */
static double exp2_(double x)
{
    double ip = round_mode(x, 0x0C00), fp = x - ip, r;
    __asm__("f2xm1" : "=t"(r) : "0"(fp));
    r += 1.0;
    __asm__("fscale" : "=t"(r) : "0"(r), "u"(ip));
    return r;
}

double log(double x)
{
    double r;
    __asm__("fldln2; fxch; fyl2x" : "=t"(r) : "0"(x) : "st(1)");
    return r;
}

double log10(double x)
{
    double r;
    __asm__("fldlg2; fxch; fyl2x" : "=t"(r) : "0"(x) : "st(1)");
    return r;
}

double exp(double x) { return exp2_(x * 1.4426950408889634); }

double pow(double x, double y)
{
    if (y == 0) return 1;
    if (x == 0) return 0;
    if (x < 0) {
        double yi = trunc(y);
        if (yi != y) return 0.0 / 0.0;
        double r = exp2_(y * (log(-x) * 1.4426950408889634));
        return ((long long)yi & 1) ? -r : r;
    }
    return exp2_(y * (log(x) * 1.4426950408889634));
}

float sinf(float x) { return (float)sin(x); }
float cosf(float x) { return (float)cos(x); }
float tanf(float x) { return (float)tan(x); }
float sqrtf(float x) { return (float)sqrt(x); }
float fabsf(float x) { return __builtin_fabsf(x); }
float floorf(float x) { return (float)floor(x); }
float ceilf(float x) { return (float)ceil(x); }
float atan2f(float y, float x) { return (float)atan2(y, x); }
