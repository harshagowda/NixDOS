// a divide by zero must be caught and must not take the OS down
int main()
{
    int zero = 0;
    printf("before\n");
    printf("%d\n", 10 / zero);
    printf("not reached\n");
    return 0;
}
