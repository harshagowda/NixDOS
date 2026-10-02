// queens.c - count solutions of the 8 queens puzzle (backtracking)
#define N 8

int col[N];

int safe(int row, int c)
{
    int r;
    for (r = 0; r < row; r++) {
        if (col[r] == c) return 0;
        if (abs(col[r] - c) == row - r) return 0;
    }
    return 1;
}

int solve(int row)
{
    if (row == N) return 1;
    int c, total = 0;
    for (c = 0; c < N; c++) {
        if (safe(row, c)) {
            col[row] = c;
            total += solve(row + 1);
        }
    }
    return total;
}

int main()
{
    printf("%d queens: %d solutions\n", N, solve(0));
    return 0;
}
