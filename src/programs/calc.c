// calc.c - recursive-descent expression calculator
// Type expressions like  2 + 3 * (4 - 1)  ; 'quit' to exit.
char *p;
int error;

int expr();

void skip() { while (*p == ' ') p++; }

int number()
{
    int v = 0;
    skip();
    if (*p == '(') {
        p++;
        v = expr();
        skip();
        if (*p == ')') p++; else error = 1;
        return v;
    }
    if (*p == '-') { p++; return -number(); }
    if (!isdigit(*p)) { error = 1; return 0; }
    while (isdigit(*p)) v = v * 10 + (*p++ - '0');
    return v;
}

int term()
{
    int v = number();
    for (;;) {
        skip();
        char op = *p;
        if (op != '*' && op != '/' && op != '%') return v;
        p++;
        int r = number();
        if ((op == '/' || op == '%') && r == 0) { error = 2; return 0; }
        if (op == '*') v = v * r;
        else if (op == '/') v = v / r;
        else v = v % r;
    }
}

int expr()
{
    int v = term();
    for (;;) {
        skip();
        if (*p == '+') { p++; v += term(); }
        else if (*p == '-') { p++; v -= term(); }
        else return v;
    }
}

int main()
{
    char line[80];
    printf("NixDOS calculator - integer + - * / %% and ( ), 'quit' exits\n");
    for (;;) {
        printf("calc> ");
        gets(line, 80);
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;
        if (!line[0]) continue;
        p = line;
        error = 0;
        int v = expr();
        skip();
        if (*p) error = 1;
        if (error == 2) printf("division by zero\n");
        else if (error) printf("syntax error\n");
        else printf("= %d\n", v);
    }
    return 0;
}
