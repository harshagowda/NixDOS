// sort.c - quicksort on random numbers
#define N 200

int data[N];

void quicksort(int *a, int lo, int hi)
{
    if (lo >= hi) return;
    int pivot = a[(lo + hi) / 2];
    int i = lo, j = hi;
    while (i <= j) {
        while (a[i] < pivot) i++;
        while (a[j] > pivot) j--;
        if (i <= j) {
            int t = a[i];
            a[i] = a[j];
            a[j] = t;
            i++;
            j--;
        }
    }
    quicksort(a, lo, j);
    quicksort(a, i, hi);
}

int main()
{
    int i, ok = 1;
    srand(time());
    for (i = 0; i < N; i++) data[i] = rand() % 1000;
    quicksort(data, 0, N - 1);
    for (i = 0; i < N; i++) {
        printf("%4d", data[i]);
        if (i % 16 == 15) printf("\n");
        if (i > 0 && data[i - 1] > data[i]) ok = 0;
    }
    printf("\nsorted: %s\n", ok ? "yes" : "NO");
    return 0;
}
