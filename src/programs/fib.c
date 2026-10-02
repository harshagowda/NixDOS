// fib.c - Fibonacci numbers, recursive and iterative
int fib_rec(int n)
{
    if (n < 2) return n;
    return fib_rec(n - 1) + fib_rec(n - 2);
}

int main()
{
    int a = 0, b = 1, i, t;
    for (i = 0; i <= 20; i++) {
        printf("fib(%d) = %d\n", i, a);
        t = a + b;
        a = b;
        b = t;
    }
    int start = ticks();
    int r = fib_rec(25);
    printf("recursive fib(25) = %d in %d ms\n", r, ticks() - start);
    return 0;
}
