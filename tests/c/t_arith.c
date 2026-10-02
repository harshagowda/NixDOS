// arithmetic, operators and precedence
int check(int got, int want, char *what)
{
    if (got != want) {
        printf("FAIL %s: got %d want %d\n", what, got, want);
        return 1;
    }
    return 0;
}

int side;
int bump() { side++; return 1; }

int main()
{
    int f = 0;
    int a = 7, b = 3, c;
    f += check(a + b * 2, 13, "precedence");
    f += check((a + b) * 2, 20, "parens");
    f += check(a / b, 2, "div");
    f += check(-a / b, -2, "neg div");
    f += check(a % b, 1, "mod");
    f += check(-a % b, -1, "neg mod");
    f += check(a - b - 1, 3, "left assoc");
    f += check(1 << 10, 1024, "shl");
    f += check(-16 >> 2, -4, "sar");
    f += check(0x0F & 0x3C, 0x0C, "and");
    f += check(0x0F | 0x30, 0x3F, "or");
    f += check(0x0F ^ 0xFF, 0xF0, "xor");
    f += check(~0, -1, "not");
    f += check(!5, 0, "lnot");
    f += check(!0, 1, "lnot0");
    f += check(-(-a), 7, "neg neg");
    f += check(a > b, 1, "gt");
    f += check(a < b, 0, "lt");
    f += check(a >= 7, 1, "ge");
    f += check(a <= 6, 0, "le");
    f += check(a == 7, 1, "eq");
    f += check(a != 7, 0, "ne");
    f += check(1 < 2 == 1, 1, "rel before eq");
    f += check(2 + 3 << 1, 10, "add before shift");
    f += check(1 | 2 & 3, 3, "and before or");

    side = 0;
    c = 0 && bump();
    f += check(c + side, 0, "&& short circuit");
    c = 1 || bump();
    f += check(c + side, 1, "|| short circuit");
    c = 1 && bump();
    f += check(c + side, 2, "&& evaluates rhs");
    f += check(0 || 0 || 3, 1, "|| chain");
    f += check(1 && 2 && 0, 0, "&& chain");

    f += check(a > b ? 100 : 200, 100, "ternary");
    f += check(a < b ? 100 : b > 2 ? 300 : 400, 300, "nested ternary");

    c = 10;
    c += 5;  f += check(c, 15, "+=");
    c -= 3;  f += check(c, 12, "-=");
    c *= 2;  f += check(c, 24, "*=");
    c /= 5;  f += check(c, 4, "/=");
    c %= 3;  f += check(c, 1, "%=");
    c <<= 4; f += check(c, 16, "<<=");
    c >>= 2; f += check(c, 4, ">>=");
    c |= 3;  f += check(c, 7, "|=");
    c &= 5;  f += check(c, 5, "&=");
    c ^= 1;  f += check(c, 4, "^=");

    c = 5;
    f += check(c++, 5, "post inc value");
    f += check(c, 6, "post inc effect");
    f += check(++c, 7, "pre inc");
    f += check(c--, 7, "post dec");
    f += check(--c, 5, "pre dec");

    int x, y, z;
    x = y = z = 42;
    f += check(x + y + z, 126, "chained assign");
    f += check((x = 3, x + 1), 4, "comma");
    f += check(sizeof(int), 4, "sizeof int");
    f += check(sizeof(char), 1, "sizeof char");
    f += check(sizeof(char *), 4, "sizeof ptr");
    f += check('A' + 1, 66, "char literal");
    f += check('\n', 10, "escape");
    f += check(0x7fffffff + 1 < 0, 1, "overflow wraps");
    f += check(100000 * 3000, 300000000, "large mul");
    f += check(017, 15, "octal");

    if (f == 0) printf("arith ok\n");
    return f;
}
