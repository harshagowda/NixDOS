// strings.c - string handling with pointers
void upcase(char *s)
{
    while (*s) {
        *s = toupper(*s);
        s++;
    }
}

int is_palindrome(char *s)
{
    char *e = s + strlen(s) - 1;
    while (s < e) {
        if (*s++ != *e--) return 0;
    }
    return 1;
}

int count_char(char *s, char c)
{
    int n = 0;
    while ((s = strchr(s, c)) != NULL) {
        n++;
        s++;
    }
    return n;
}

int main()
{
    char name[32];
    strcpy(name, "nixdos");
    upcase(name);
    printf("%s has %d characters\n", name, strlen(name));

    char *words[] = {"level", "kernel", "racecar", "compiler"};
    int i;
    for (i = 0; i < 4; i++)
        printf("%-10s %s\n", words[i], is_palindrome(words[i]) ? "is a palindrome" : "is not");

    printf("'s' appears %d times in 'mississippi'\n", count_char("mississippi", 's'));

    char buf[64];
    sprintf(buf, "%s-%d.%d", "NixDOS", 2, 0);
    printf("formatted: %s\n", buf);
    return 0;
}
