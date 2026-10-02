// pointers, arrays and strings
void swap(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

int sum(int *v, int n)
{
    int s = 0;
    while (n--) s += *v++;
    return s;
}

int my_strlen(char *s)
{
    char *p = s;
    while (*p) p++;
    return p - s;
}

void reverse(char *s)
{
    int i = 0, j = strlen(s) - 1;
    while (i < j) {
        char t = s[i];
        s[i++] = s[j];
        s[j--] = t;
    }
}

void set_ptr(int **pp, int *target) { *pp = target; }

int main()
{
    int a = 1, b = 2;
    swap(&a, &b);
    printf("swap %d %d\n", a, b);

    int arr[5];
    int i;
    for (i = 0; i < 5; i++) arr[i] = i * i;
    printf("arr %d %d %d sum %d\n", arr[0], arr[2], arr[4], sum(arr, 5));

    int *p = arr;
    p += 2;
    printf("ptr %d %d %d diff %d\n", *p, p[1], *(p - 1), &arr[4] - p);
    printf("sizeof arr %d\n", sizeof(arr));

    char buf[32];
    strcpy(buf, "hello");
    strcat(buf, " world");
    printf("buf '%s' len %d %d\n", buf, strlen(buf), my_strlen(buf));
    reverse(buf);
    printf("rev '%s'\n", buf);

    char s[] = "abc";
    s[1] = 'X';
    printf("s %s sizeof %d\n", s, sizeof(s));

    char *words[3];
    words[0] = "zero";
    words[1] = "one";
    words[2] = "two";
    for (i = 0; i < 3; i++) printf("%s%c", words[i], i < 2 ? ',' : '\n');

    int *q;
    set_ptr(&q, &b);
    *q = 99;
    printf("ptrptr %d\n", b);

    int *heap = malloc(10 * sizeof(int));
    for (i = 0; i < 10; i++) heap[i] = i + 1;
    printf("heap sum %d\n", sum(heap, 10));
    free(heap);

    char *str = malloc(16);
    sprintf(str, "%d-%x-%s", 42, 255, "ok");
    printf("sprintf %s\n", str);
    free(str);

    int init[4] = {10, 20, 30};
    printf("init %d %d %d %d\n", init[0], init[1], init[2], init[3]);

    char c = 200;
    printf("char wrap %d\n", c);
    char *cp = buf;
    cp[0] = 'Q';
    printf("char ptr %c\n", *cp);

    int m[3];
    int *mp = m;
    *mp++ = 7;
    *mp++ = 8;
    *mp = 9;
    printf("postinc store %d %d %d\n", m[0], m[1], m[2]);
    return 0;
}
