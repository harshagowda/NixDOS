/* hello_native.c - a native NixDOS program built with GCC and src/libc.
 * Tests libc, the heap, files, 64-bit math, floats and C++-free startup. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>

static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(int argc, char **argv)
{
    printf("native hello, argc=%d argv[0]=%s\n", argc, argv[0]);
    char *p = malloc(1000000);
    memset(p, 7, 1000000);
    printf("malloc 1MB ok: %d\n", p[999999]);
    p = realloc(p, 3000000);
    printf("realloc ok: %d\n", p[999999]);
    free(p);

    long long big = 1234567890123LL;
    printf("64-bit: %lld / 7 = %lld rem %lld\n", big, big / 7, big % 7);
    printf("float: sqrt(2)=%.5f pi=%.4f atan2(1,1)*4=%.4f floor(-2.5)=%.1f pow(2,10)=%.0f\n",
           sqrt(2.0), M_PI, atan2(1.0, 1.0) * 4, floor(-2.5), pow(2, 10));

    int v[] = { 5, 3, 9, 1, 7, 2, 8, 6, 4, 0, 11, 10 };
    qsort(v, 12, sizeof(int), cmp);
    printf("qsort:");
    for (int i = 0; i < 12; i++) printf(" %d", v[i]);
    printf("\n");

    FILE *f = fopen("native.txt", "w");
    fprintf(f, "line %d\n", 1);
    fputs("line 2\n", f);
    fclose(f);
    f = fopen("native.txt", "r");
    char line[64];
    while (fgets(line, sizeof(line), f)) printf("read: %s", line);
    fclose(f);

    int fd = open("native.txt", O_RDONLY);
    lseek(fd, 5, SEEK_SET);
    char c;
    read(fd, &c, 1);
    close(fd);
    printf("lseek/read: %c\n", c);
    return 3;
}
