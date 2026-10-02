// primes.c - Sieve of Eratosthenes
#define LIMIT 1000

char composite[LIMIT + 1];

int main()
{
    int i, j, count = 0;
    for (i = 2; i * i <= LIMIT; i++)
        if (!composite[i])
            for (j = i * i; j <= LIMIT; j += i)
                composite[j] = 1;

    printf("Primes up to %d:\n", LIMIT);
    for (i = 2; i <= LIMIT; i++) {
        if (!composite[i]) {
            printf("%d ", i);
            count++;
        }
    }
    printf("\n%d primes found.\n", count);
    return 0;
}
