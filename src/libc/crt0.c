/* NixDOS 2 - startup code and runtime support for native programs */
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char **argv);

typedef void (*ctor_fn)(void);
extern ctor_fn __init_array_start[], __init_array_end[];

__attribute__((section(".text.start"), used))
void _start(int argc, char **argv)
{
    for (ctor_fn *c = __init_array_start; c < __init_array_end; c++) (*c)();
    exit(main(argc, argv));
}

/* ---- C++ runtime ------------------------------------------------------------ */
void *__dso_handle;
int __cxa_atexit(void (*fn)(void *), void *arg, void *dso) { (void)fn; (void)arg; (void)dso; return 0; }
void __cxa_pure_virtual(void) { abort(); }

/* ---- 64-bit integer division (normally provided by libgcc) --------------------- */
static uint64_t udivmod64(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;
    if (d == 0) { volatile int z = 0; z = 1 / z; }
    if ((d >> 32) == 0 && (n >> 32) == 0) {
        if (rem) *rem = (uint32_t)n % (uint32_t)d;
        return (uint32_t)n / (uint32_t)d;
    }
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ULL << i; }
    }
    if (rem) *rem = r;
    return q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d) { return udivmod64(n, d, 0); }
uint64_t __umoddi3(uint64_t n, uint64_t d) { uint64_t r; udivmod64(n, d, &r); return r; }

int64_t __divdi3(int64_t n, int64_t d)
{
    int neg = (n < 0) != (d < 0);
    uint64_t q = udivmod64(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, 0);
    return neg ? -(int64_t)q : (int64_t)q;
}

int64_t __moddi3(int64_t n, int64_t d)
{
    uint64_t r;
    udivmod64(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, &r);
    return n < 0 ? -(int64_t)r : (int64_t)r;
}

uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem) { return udivmod64(n, d, rem); }
