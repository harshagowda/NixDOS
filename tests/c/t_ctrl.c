// control flow, recursion, switch, globals, #define
#define N 10
#define NEG (-3)

int counter = 5;
int table[] = {3, 1, 4, 1, 5, 9, 2, 6};
char greeting[] = "hi there";
char *names[] = {"alpha", "beta", "gamma"};
int zeros[100];

int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }
int fib(int n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }
int later(int x);           /* prototype */

int hanoi(int n, int from, int to, int via)
{
    if (n == 0) return 0;
    return hanoi(n - 1, from, via, to) + 1 + hanoi(n - 1, via, to, from);
}

char *kind(int v)
{
    switch (v) {
    case 0: return "zero";
    case 1:
    case 2: return "small";
    case NEG: return "negative three";
    default: return "big";
    }
}

int main()
{
    int i, s = 0;
    for (i = 0; i < N; i++) {
        if (i == 3) continue;
        if (i == 8) break;
        s += i;
    }
    printf("for %d\n", s);

    i = 0;
    while (1) { if (++i >= 5) break; }
    printf("while %d\n", i);

    i = 0;
    do { i += 3; } while (i < 10);
    printf("do %d\n", i);

    int nested = 0, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            if (j > i) break;
            nested++;
        }
    printf("nested %d\n", nested);

    printf("fact %d fib %d hanoi %d later %d\n", fact(10), fib(20), hanoi(10, 1, 3, 2), later(4));
    printf("switch %s %s %s %s\n", kind(0), kind(2), kind(-3), kind(50));

    int fall = 0;
    switch (2) {
    case 1: fall += 1;
    case 2: fall += 2;
    case 3: fall += 3; break;
    case 4: fall += 4;
    }
    printf("fallthrough %d\n", fall);

    int t = 0;
    for (i = 0; i < 8; i++) t += table[i];
    printf("globals %d %d %s %s %d\n", counter, t, greeting, names[2], sizeof(table));
    counter++;
    zeros[99] = 1;
    printf("counter %d zeros %d %d\n", counter, zeros[0], zeros[99]);

    for (int k = 0; k < 3; k++) {
        int inner = k * 2;
        s = inner;
    }
    printf("scoped %d\n", s);

    int sw = 0;
    for (i = 0; i < 5; i++) {
        switch (i) {
        case 1: continue;
        case 3: sw += 100; break;
        default: sw += 1;
        }
        sw += 10;
    }
    printf("switch in loop %d\n", sw);
    return 0;
}

int later(int x) { return x * x; }
