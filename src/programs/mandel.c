// mandel.c - ASCII Mandelbrot set using fixed-point integer math
#define SCALE 4096

int main()
{
    char *shades = " .:-=+*%#@";
    int y, x, i;
    for (y = 0; y < 22; y++) {
        for (x = 0; x < 78; x++) {
            int cr = (x - 52) * SCALE * 3 / 78;     // -2.0 .. 1.0
            int ci = (y - 11) * SCALE * 2 / 22;     // -1.0 .. 1.0
            int zr = 0, zi = 0;
            for (i = 0; i < 40; i++) {
                int zr2 = zr * zr / SCALE, zi2 = zi * zi / SCALE;
                if (zr2 + zi2 > 4 * SCALE) break;
                zi = 2 * zr * zi / SCALE + ci;
                zr = zr2 - zi2 + cr;
            }
            putchar(i == 40 ? '#' : shades[i % 9]);
        }
        putchar('\n');
    }
    return 0;
}
