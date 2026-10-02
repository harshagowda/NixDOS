/* NixDOS 2 - NixC: a one-pass C compiler that emits 32-bit x86 machine code
 *
 * Supported language subset:
 *   types      int, char, void, pointers (any depth), 1-D arrays
 *              (unsigned/signed/long/short/const/static are accepted, treated as int/ignored)
 *   globals    with constant / string / { list } initialisers
 *   functions  parameters, recursion, prototypes, calls before definition
 *   statements if/else, while, do/while, for, switch/case/default,
 *              break, continue, return, blocks, local declarations anywhere
 *   operators  = += -= *= /= %= &= |= ^= <<= >>=  ?:  || &&  | ^ &
 *              == != < > <= >=  << >>  + - * / %  unary - + ! ~ * & ++ --
 *              (prefix and postfix), casts, sizeof, [] indexing, pointer arithmetic
 *   literals   decimal/hex/octal, 'c' with escapes, "strings" (adjacent ones concatenated)
 *   #define    NAME <integer constant>; other # lines (e.g. #include) are ignored
 *
 * Code generation: expression results live in EAX, the left operand of a binary
 * operator is kept on the stack. Locals live at [EBP-n], parameters at [EBP+8+4i].
 * Generated code only touches EAX/ECX/EDX (+EBP/ESP), so it can call kernel
 * functions compiled by GCC with the cdecl convention directly.
 *
 * Code is emitted straight to PROG_CODE and data to PROG_DATA, where it runs.
 */
#include "kernel.h"
#include "api.h"

/* ---- tokens ---------------------------------------------------------------- */
enum {
    TK_EOF = 256, TK_NUM, TK_STR, TK_ID,
    TK_INT, TK_CHAR, TK_VOID, TK_INTMOD, TK_QUAL,
    TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_DO, TK_RETURN, TK_BREAK, TK_CONTINUE,
    TK_SIZEOF, TK_SWITCH, TK_CASE, TK_DEFAULT,
    TK_EQ, TK_NE, TK_LE, TK_GE, TK_AND, TK_OR, TK_INC, TK_DEC, TK_SHL, TK_SHR,
    TK_ADDEQ, TK_SUBEQ, TK_MULEQ, TK_DIVEQ, TK_MODEQ, TK_ANDEQ, TK_OREQ, TK_XOREQ,
    TK_SHLEQ, TK_SHREQ
};

static const struct { const char *s; int t; } keywords[] = {
    {"int", TK_INT}, {"char", TK_CHAR}, {"void", TK_VOID},
    {"unsigned", TK_INTMOD}, {"signed", TK_INTMOD}, {"long", TK_INTMOD}, {"short", TK_INTMOD},
    {"const", TK_QUAL}, {"static", TK_QUAL}, {"extern", TK_QUAL}, {"volatile", TK_QUAL},
    {"register", TK_QUAL}, {"inline", TK_QUAL},
    {"if", TK_IF}, {"else", TK_ELSE}, {"while", TK_WHILE}, {"for", TK_FOR}, {"do", TK_DO},
    {"return", TK_RETURN}, {"break", TK_BREAK}, {"continue", TK_CONTINUE},
    {"sizeof", TK_SIZEOF}, {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    {NULL, 0}
};

/* ---- symbols --------------------------------------------------------------- */
enum { C_GLOBAL = 1, C_LOCAL, C_FUNC, C_BUILTIN, C_CONST };

typedef struct {
    char name[32];
    u8   cls;
    u8   defined;
    u8   predef;        /* built-in name that user code may redefine */
    i16  nparams;
    int  type;
    int  val;           /* local: EBP offset, global: data offset, func: code offset,
                           builtin: API index, const: value */
    int  arrlen;        /* > 0 for arrays */
} sym_t;

#define MAX_GSYMS 1024
#define MAX_LSYMS 256
#define MAX_FIX   4096
#define MAX_JMP   128
#define MAX_CASES 256
#define MAX_LOOPS 32
#define MAX_INIT  4096
#define STRBUF    4096

struct fixup { u32 pos; int sym; int line; };

struct loopctx {
    int is_switch;
    int nbrk, ncont, ncase;
    u32 brk[MAX_JMP], cont[MAX_JMP];
    int case_val[MAX_CASES];
    u32 case_pos[MAX_CASES];
    int has_default;
    u32 default_pos;
};

struct lexstate { const char *p; int line; int tok; int val; };

/* ---- compiler state (allocated per compilation) ---------------------------- */
static const char *filename;
static struct lexstate L;
static char tokname[64];
static char *tokstr;
static int tokslen;

static sym_t *gsyms, *lsyms;
static int ngsyms, nlsyms;
static struct fixup *fixes;
static int nfix;
static struct loopctx *loops;
static int nloops;
static int *initvals;

static u8 *code;
static u32 cpos;
static u8 *data;
static u32 dpos;

static int frame, maxframe;     /* local variable area of the current function */
static int ty, lv;              /* type of the expression in EAX / EAX holds an lvalue address */

static k_jmp_buf cc_jb;

static void cc_error(const char *fmt, ...) __attribute__((noreturn));
static void cc_error(const char *fmt, ...)
{
    va_list ap;
    con_setcolor(12, 0);
    kprintf("%s:%d: error: ", filename, L.line);
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\n");
    con_setcolor(7, 0);
    k_longjmp(cc_jb, 1);
}

/* ---- lexer ----------------------------------------------------------------- */
static sym_t *find_global(const char *name);
static sym_t *add_global(const char *name, int cls);

static int read_escape(const char **pp)
{
    const char *p = *pp;
    int c = *p++;
    switch (c) {
    case 'n': c = '\n'; break;
    case 't': c = '\t'; break;
    case 'r': c = '\r'; break;
    case 'a': c = 7; break;
    case 'b': c = 8; break;
    case 'f': c = 12; break;
    case 'v': c = 11; break;
    case 'e': c = 27; break;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
        c -= '0';
        for (int i = 0; i < 2 && *p >= '0' && *p <= '7'; i++) c = c * 8 + (*p++ - '0');
        break;
    case 'x':
        c = 0;
        while (isalnum(*p) && (isdigit(*p) || (tolower(*p) >= 'a' && tolower(*p) <= 'f'))) {
            c = c * 16 + (isdigit(*p) ? *p - '0' : tolower(*p) - 'a' + 10);
            p++;
        }
        break;
    case 0:
        cc_error("unterminated escape");
    default:
        break;      /* \\ \' \" \? -> the character itself */
    }
    *pp = p;
    return c & 0xFF;
}

static int parse_number(const char **pp)
{
    const char *p = *pp;
    u32 v = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        while (isdigit(*p) || (tolower(*p) >= 'a' && tolower(*p) <= 'f')) {
            v = v * 16 + (isdigit(*p) ? (u32)(*p - '0') : (u32)(tolower(*p) - 'a' + 10));
            p++;
        }
    } else if (p[0] == '0') {
        while (*p >= '0' && *p <= '7') v = v * 8 + (u32)(*p++ - '0');
    } else {
        while (isdigit(*p)) v = v * 10 + (u32)(*p++ - '0');
    }
    while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') p++;
    if (isalnum(*p) || *p == '_') cc_error("bad number");
    *pp = p;
    return (int)v;
}

static void preprocessor_line(void)
{
    const char *p = L.p;            /* just after '#' */
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "define", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        char name[32];
        int n = 0, neg = 0, v;
        p += 6;
        while (*p == ' ' || *p == '\t') p++;
        while ((isalnum(*p) || *p == '_') && n < 31) name[n++] = *p++;
        name[n] = 0;
        if (!n) cc_error("#define needs a name");
        while (*p == ' ' || *p == '\t') p++;
        int paren = 0;
        if (*p == '(') { paren = 1; p++; }
        if (*p == '-') { neg = 1; p++; }
        if (isdigit(*p)) {
            v = parse_number(&p);
        } else if (*p == '\'') {
            p++;
            v = (*p == '\\') ? (p++, read_escape(&p)) : (u8)*p++;
            if (*p++ != '\'') cc_error("bad character constant");
        } else {
            cc_error("#define %s: only integer constants are supported", name);
        }
        if (paren && *p++ != ')') cc_error("#define %s: missing ')'", name);
        sym_t *s = find_global(name);
        if (!s || s->cls != C_CONST) s = add_global(name, C_CONST);
        s->val = neg ? -v : v;
        s->type = TY_INT;
    }
    while (*p && *p != '\n') p++;
    L.p = p;
}

static void next(void)
{
    const char *p;
    for (;;) {
        p = L.p;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            if (*p == '\n') L.line++;
            p++;
        }
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            L.p = p;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') L.line++;
                p++;
            }
            if (!*p) { L.p = p; cc_error("unterminated comment"); }
            L.p = p + 2;
            continue;
        }
        if (*p == '#') {
            L.p = p + 1;
            preprocessor_line();
            continue;
        }
        break;
    }

    if (!*p) { L.p = p; L.tok = TK_EOF; return; }

    if (isalpha(*p) || *p == '_') {
        int n = 0;
        while (isalnum(*p) || *p == '_') {
            if (n < 31) tokname[n++] = *p;
            p++;
        }
        tokname[n] = 0;
        L.p = p;
        L.tok = TK_ID;
        for (int i = 0; keywords[i].s; i++)
            if (strcmp(keywords[i].s, tokname) == 0) { L.tok = keywords[i].t; break; }
        return;
    }

    if (isdigit(*p)) {
        L.val = parse_number(&p);
        L.p = p;
        L.tok = TK_NUM;
        return;
    }

    if (*p == '\'') {
        p++;
        if (*p == '\\') { p++; L.val = read_escape(&p); }
        else if (*p && *p != '\n') L.val = (u8)*p++;
        else { L.p = p; cc_error("bad character constant"); }
        if (*p != '\'') { L.p = p; cc_error("unterminated character constant"); }
        L.p = p + 1;
        L.tok = TK_NUM;
        return;
    }

    if (*p == '"') {
        p++;
        tokslen = 0;
        while (*p != '"') {
            if (!*p || *p == '\n') { L.p = p; cc_error("unterminated string"); }
            int c = (*p == '\\') ? (p++, read_escape(&p)) : (u8)*p++;
            if (tokslen >= STRBUF - 1) { L.p = p; cc_error("string too long"); }
            tokstr[tokslen++] = (char)c;
        }
        L.p = p + 1;
        L.tok = TK_STR;
        return;
    }

    /* punctuators: three-, two-, then one-character */
    static const struct { const char *s; int t; } ops[] = {
        {"<<=", TK_SHLEQ}, {">>=", TK_SHREQ},
        {"==", TK_EQ}, {"!=", TK_NE}, {"<=", TK_LE}, {">=", TK_GE}, {"&&", TK_AND},
        {"||", TK_OR}, {"++", TK_INC}, {"--", TK_DEC}, {"<<", TK_SHL}, {">>", TK_SHR},
        {"+=", TK_ADDEQ}, {"-=", TK_SUBEQ}, {"*=", TK_MULEQ}, {"/=", TK_DIVEQ},
        {"%=", TK_MODEQ}, {"&=", TK_ANDEQ}, {"|=", TK_OREQ}, {"^=", TK_XOREQ},
        {NULL, 0}
    };
    for (int i = 0; ops[i].s; i++) {
        size_t n = strlen(ops[i].s);
        if (strncmp(p, ops[i].s, n) == 0) {
            L.p = p + n;
            L.tok = ops[i].t;
            return;
        }
    }
    if (strchr("+-*/%=<>!~&|^?:;,.(){}[]", *p)) {
        L.tok = *p;
        L.p = p + 1;
        return;
    }
    L.p = p;
    cc_error("unexpected character '%c'", *p);
}

static const char *tokdesc(int t)
{
    static char buf[8];
    switch (t) {
    case TK_EOF: return "end of file";
    case TK_NUM: return "number";
    case TK_STR: return "string";
    case TK_ID: return "identifier";
    }
    if (t < 256) { buf[0] = '\''; buf[1] = (char)t; buf[2] = '\''; buf[3] = 0; return buf; }
    return "keyword/operator";
}

static void expect(int t)
{
    if (L.tok != t) {
        char want[8];
        strcpy(want, tokdesc(t));
        cc_error("expected %s but found %s", want, tokdesc(L.tok));
    }
    next();
}

static int peek(void)
{
    struct lexstate saved = L;
    char name[64];
    strcpy(name, tokname);
    next();
    int t = L.tok;
    L = saved;
    strcpy(tokname, name);
    return t;
}

/* ---- symbol tables --------------------------------------------------------- */
static sym_t *find_global(const char *name)
{
    for (int i = 0; i < ngsyms; i++)
        if (strcmp(gsyms[i].name, name) == 0) return &gsyms[i];
    return NULL;
}

static sym_t *lookup(const char *name)
{
    for (int i = nlsyms - 1; i >= 0; i--)
        if (strcmp(lsyms[i].name, name) == 0) return &lsyms[i];
    return find_global(name);
}

static sym_t *add_global(const char *name, int cls)
{
    sym_t *s = find_global(name);
    if (s) {
        if (!s->predef) cc_error("'%s' redefined", name);
    } else {
        if (ngsyms >= MAX_GSYMS) cc_error("too many global symbols");
        s = &gsyms[ngsyms++];
    }
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, 31);
    s->cls = (u8)cls;
    return s;
}

static sym_t *add_local(const char *name, int type, int arrlen, int offset)
{
    if (nlsyms >= MAX_LSYMS) cc_error("too many local variables");
    sym_t *s = &lsyms[nlsyms++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, 31);
    s->cls = C_LOCAL;
    s->type = type;
    s->arrlen = arrlen;
    s->val = offset;
    return s;
}

/* ---- types ----------------------------------------------------------------- */
static int is_ptr(int t) { return t >= TY_PTR; }
static int ty_size(int t) { return t == TY_CHAR ? 1 : 4; }
static int elem_size(int t)
{
    int b = t - TY_PTR;
    return (b == TY_CHAR || b == TY_VOID) ? 1 : 4;
}
static int is_type_tok(int t)
{
    return t == TK_INT || t == TK_CHAR || t == TK_VOID || t == TK_INTMOD || t == TK_QUAL;
}

static int parse_base_type(void)
{
    int base = 0;
    while (is_type_tok(L.tok)) {
        if (L.tok == TK_CHAR) base = TY_CHAR;
        else if (L.tok == TK_VOID) base = TY_VOID;
        else if ((L.tok == TK_INT || L.tok == TK_INTMOD) && !base) base = TY_INT;
        next();
    }
    return base ? base : TY_INT;
}

static int parse_stars(int t)
{
    while (L.tok == '*' || L.tok == TK_QUAL) {
        if (L.tok == '*') t += TY_PTR;
        next();
    }
    return t;
}

/* ---- code emission --------------------------------------------------------- */
static void e1(int b)
{
    if (cpos >= PROG_CODE_MAX) cc_error("program too large (code > %d KiB)", PROG_CODE_MAX / 1024);
    code[cpos++] = (u8)b;
}
static void e2(int a, int b) { e1(a); e1(b); }
static void e3(int a, int b, int c) { e1(a); e1(b); e1(c); }
static void e4(u32 v) { e1(v & 0xFF); e1((v >> 8) & 0xFF); e1((v >> 16) & 0xFF); e1(v >> 24); }
static void put4(u32 pos, u32 v) { memcpy(code + pos, &v, 4); }

static void emit_imm(int v) { e1(0xB8); e4((u32)v); }        /* mov eax, imm32 */
static void emit_push(void) { e1(0x50); }                     /* push eax */
static void emit_pop_ecx(void) { e1(0x59); }                  /* pop ecx */
static void emit_epilogue(void) { e2(0x89, 0xEC); e1(0x5D); e1(0xC3); }

static u32 emit_jmp(void) { e1(0xE9); e4(0); return cpos - 4; }
static u32 emit_jz(void) { e2(0x85, 0xC0); e2(0x0F, 0x84); e4(0); return cpos - 4; }
static u32 emit_jnz(void) { e2(0x85, 0xC0); e2(0x0F, 0x85); e4(0); return cpos - 4; }
static void patch_to(u32 at, u32 target) { put4(at, target - (at + 4)); }
static void patch_here(u32 at) { patch_to(at, cpos); }
static void emit_jmp_to(u32 target) { e1(0xE9); e4(target - (cpos + 4)); }

static void emit_scale(int size) { if (size == 4) e3(0xC1, 0xE0, 0x02); }    /* shl eax, 2 */

static void load(int t)                 /* eax = *(t *)eax */
{
    if (t == TY_CHAR) e3(0x0F, 0xBE, 0x00);
    else e2(0x8B, 0x00);
}

static void load_ecx(int t)             /* eax = *(t *)ecx */
{
    if (t == TY_CHAR) e3(0x0F, 0xBE, 0x01);
    else e2(0x8B, 0x01);
}

static void store(int t)                /* *(t *)ecx = eax */
{
    if (t == TY_CHAR) { e2(0x88, 0x01); e3(0x0F, 0xBE, 0xC0); }
    else e2(0x89, 0x01);
}

static void rvalue(void)
{
    if (lv) {
        load(ty);
        lv = 0;
    }
}

static u32 data_alloc(u32 size, u32 align)
{
    dpos = (dpos + align - 1) & ~(align - 1);
    if (dpos + size > PROG_DATA_MAX) cc_error("program data too large (> %d KiB)", PROG_DATA_MAX / 1024);
    u32 off = dpos;
    dpos += size;
    return off;
}

/* Place the current string token (plus adjacent ones) in the data area. */
static u32 string_literal(void)
{
    u32 off = data_alloc(0, 1);
    while (L.tok == TK_STR) {
        data_alloc((u32)tokslen, 1);
        memcpy(data + off + (dpos - off - (u32)tokslen), tokstr, (u32)tokslen);
        next();
    }
    data_alloc(1, 1);
    data[dpos - 1] = 0;
    return PROG_DATA + off;
}

/* ---- expressions ----------------------------------------------------------- */
static void expr(void);
static void assign_expr(void);
static void unary(void);
static int const_expr(void);

static void need_lvalue(void)
{
    if (!lv) cc_error("lvalue required");
}

static void gen_call(const char *name)
{
    char fname[32];
    int n = 0;
    strcpy(fname, name);
    next();     /* '(' */
    if (L.tok != ')') {
        for (;;) {
            assign_expr();
            rvalue();
            emit_push();
            n++;
            if (L.tok != ',') break;
            next();
        }
    }
    expect(')');

    /* arguments were pushed left to right; cdecl wants them reversed */
    for (int i = 0; i < n / 2; i++) {
        u32 a = (u32)i * 4, b = (u32)(n - 1 - i) * 4;
        e3(0x8B, 0x84, 0x24); e4(a);     /* mov eax, [esp+a] */
        e3(0x8B, 0x8C, 0x24); e4(b);     /* mov ecx, [esp+b] */
        e3(0x89, 0x8C, 0x24); e4(a);     /* mov [esp+a], ecx */
        e3(0x89, 0x84, 0x24); e4(b);     /* mov [esp+b], eax */
    }

    sym_t *s = find_global(fname);
    if (s && s->cls != C_FUNC && s->cls != C_BUILTIN) cc_error("'%s' is not a function", fname);
    if (!s) {
        s = add_global(fname, C_FUNC);      /* implicit declaration, must be defined later */
        s->type = TY_INT;
        s->nparams = -1;
    }
    if (s->nparams >= 0 && s->nparams != n)
        cc_error("%s() takes %d argument%s, %d given", fname, s->nparams, s->nparams == 1 ? "" : "s", n);

    if (s->cls == C_BUILTIN) {
        e1(0xA1); e4(API_PTR_ADDR);          /* mov eax, [API_PTR_ADDR] */
        e2(0xFF, 0x90); e4((u32)s->val * 4); /* call [eax + 4*index] */
    } else {
        e1(0xE8);
        if (s->defined) {
            e4(0);
            patch_to(cpos - 4, (u32)s->val);
        } else {
            if (nfix >= MAX_FIX) cc_error("too many calls");
            e4(0);
            fixes[nfix].pos = cpos - 4;
            fixes[nfix].sym = (int)(s - gsyms);
            fixes[nfix].line = L.line;
            nfix++;
        }
    }
    if (n) { e2(0x81, 0xC4); e4((u32)n * 4); }  /* add esp, 4n */
    ty = s->type;
    lv = 0;
}

static void primary(void)
{
    switch (L.tok) {
    case TK_NUM:
        emit_imm(L.val);
        ty = TY_INT;
        lv = 0;
        next();
        return;
    case TK_STR:
        emit_imm((int)string_literal());
        ty = TY_CHAR + TY_PTR;
        lv = 0;
        return;
    case '(':
        next();
        expr();
        expect(')');
        return;
    case TK_ID: {
        char name[32];
        strcpy(name, tokname);
        next();
        if (L.tok == '(') {
            gen_call(name);
            return;
        }
        sym_t *s = lookup(name);
        if (!s) cc_error("'%s' undeclared", name);
        if (s->cls == C_CONST) {
            emit_imm(s->val);
            ty = TY_INT;
            lv = 0;
            return;
        }
        if (s->cls == C_FUNC || s->cls == C_BUILTIN)
            cc_error("function pointers are not supported ('%s')", name);
        if (s->cls == C_LOCAL) {
            e2(0x8D, 0x85); e4((u32)s->val);         /* lea eax, [ebp+off] */
        } else {
            emit_imm((int)(PROG_DATA + (u32)s->val));
        }
        if (s->arrlen > 0) {
            ty = s->type + TY_PTR;                  /* arrays decay to pointers */
            lv = 0;
        } else {
            ty = s->type;
            lv = 1;
        }
        return;
    }
    default:
        cc_error("expected an expression but found %s", tokdesc(L.tok));
    }
}

static void incdec(int pre, int delta)
{
    need_lvalue();
    int t = ty;
    int step = is_ptr(t) ? elem_size(t) : 1;
    e2(0x89, 0xC1);                     /* mov ecx, eax */
    load_ecx(t);
    if (!pre) emit_push();
    e1(0x05); e4((u32)(delta * step)); /* add eax, step */
    store(t);
    if (!pre) e1(0x58);                 /* pop eax */
    ty = t;
    lv = 0;
}

static void postfix(void)
{
    primary();
    for (;;) {
        if (L.tok == '[') {
            next();
            rvalue();
            if (!is_ptr(ty)) cc_error("subscripted value is not an array or pointer");
            int t = ty;
            emit_push();
            expr();
            rvalue();
            emit_scale(elem_size(t));
            emit_pop_ecx();
            e2(0x01, 0xC8);             /* add eax, ecx */
            expect(']');
            ty = t - TY_PTR;
            if (ty == TY_VOID) cc_error("cannot index a void pointer");
            lv = 1;
        } else if (L.tok == TK_INC || L.tok == TK_DEC) {
            int d = L.tok == TK_INC ? 1 : -1;
            next();
            incdec(0, d);
        } else {
            return;
        }
    }
}

static int sizeof_expr(void)
{
    int paren = 0, size;
    next();
    if (L.tok == '(') {
        int p = peek();
        if (is_type_tok(p)) {
            next();
            size = ty_size(parse_stars(parse_base_type()));
            expect(')');
            return size;
        }
        paren = 1;
    }
    if (paren) next();
    if (L.tok == TK_ID) {
        sym_t *s = lookup(tokname);
        int after = peek();
        if (s && (s->cls == C_LOCAL || s->cls == C_GLOBAL) &&
            ((paren && after == ')') || (!paren && after != '[' && after != '(' ))) {
            size = s->arrlen > 0 ? s->arrlen * ty_size(s->type) : ty_size(s->type);
            next();
            if (paren) expect(')');
            return size;
        }
    }
    /* general expression: compile it to learn its type, then throw the code away */
    u32 save_c = cpos, save_d = dpos;
    int save_fix = nfix;
    if (paren) { expr(); expect(')'); } else unary();
    cpos = save_c;
    dpos = save_d;
    nfix = save_fix;
    size = ty_size(ty);
    return size;
}

static void unary(void)
{
    switch (L.tok) {
    case '-':
        next(); unary(); rvalue();
        e2(0xF7, 0xD8);                         /* neg eax */
        ty = TY_INT;
        return;
    case '+':
        next(); unary(); rvalue();
        return;
    case '!':
        next(); unary(); rvalue();
        e2(0x85, 0xC0); e3(0x0F, 0x94, 0xC0); e3(0x0F, 0xB6, 0xC0);   /* test; sete; movzx */
        ty = TY_INT;
        return;
    case '~':
        next(); unary(); rvalue();
        e2(0xF7, 0xD0);                         /* not eax */
        ty = TY_INT;
        return;
    case '*':
        next(); unary(); rvalue();
        if (!is_ptr(ty)) cc_error("cannot dereference a non-pointer");
        ty -= TY_PTR;
        if (ty == TY_VOID) cc_error("cannot dereference a void pointer");
        lv = 1;
        return;
    case '&':
        next(); unary();
        need_lvalue();
        ty += TY_PTR;
        lv = 0;
        return;
    case TK_INC:
    case TK_DEC: {
        int d = L.tok == TK_INC ? 1 : -1;
        next(); unary();
        incdec(1, d);
        return;
    }
    case TK_SIZEOF: {
        int size = sizeof_expr();
        emit_imm(size);
        ty = TY_INT;
        lv = 0;
        return;
    }
    case '(':
        if (is_type_tok(peek())) {              /* cast */
            next();
            int t = parse_stars(parse_base_type());
            expect(')');
            unary();
            rvalue();
            if (t == TY_CHAR) e3(0x0F, 0xBE, 0xC0);     /* movsx eax, al */
            ty = t;
            return;
        }
        postfix();
        return;
    default:
        postfix();
    }
}

static int binprec(int t)
{
    switch (t) {
    case '|': return 1;
    case '^': return 2;
    case '&': return 3;
    case TK_EQ: case TK_NE: return 4;
    case '<': case '>': case TK_LE: case TK_GE: return 5;
    case TK_SHL: case TK_SHR: return 6;
    case '+': case '-': return 7;
    case '*': case '/': case '%': return 8;
    }
    return 0;
}

/* EAX = right operand, top of stack = left operand */
static void gen_binop(int op, int lt, int rt)
{
    if (op == '+' || op == '-') {
        if (op == '-' && is_ptr(lt) && is_ptr(rt)) {
            emit_pop_ecx();
            e2(0x29, 0xC1);                     /* sub ecx, eax */
            e2(0x89, 0xC8);                     /* mov eax, ecx */
            if (elem_size(lt) == 4) e3(0xC1, 0xF8, 0x02);   /* sar eax, 2 */
            ty = TY_INT;
            return;
        }
        if (is_ptr(lt) && !is_ptr(rt)) {
            emit_scale(elem_size(lt));
            ty = lt;
        } else if (is_ptr(rt) && !is_ptr(lt) && op == '+') {
            e2(0x89, 0xC1);                     /* mov ecx, eax  (pointer) */
            e1(0x58);                           /* pop eax       (integer) */
            emit_scale(elem_size(rt));
            e2(0x01, 0xC8);                     /* add eax, ecx */
            ty = rt;
            return;
        } else {
            ty = TY_INT;
        }
        e2(0x89, 0xC1);                         /* mov ecx, eax */
        e1(0x58);                               /* pop eax */
        if (op == '+') e2(0x01, 0xC8);          /* add eax, ecx */
        else e2(0x29, 0xC8);                    /* sub eax, ecx */
        return;
    }

    e2(0x89, 0xC1);                             /* mov ecx, eax */
    e1(0x58);                                   /* pop eax */
    ty = TY_INT;
    switch (op) {
    case '*': e3(0x0F, 0xAF, 0xC1); break;      /* imul eax, ecx */
    case '/': e1(0x99); e2(0xF7, 0xF9); break;  /* cdq; idiv ecx */
    case '%': e1(0x99); e2(0xF7, 0xF9); e2(0x89, 0xD0); break;
    case '&': e2(0x21, 0xC8); break;
    case '|': e2(0x09, 0xC8); break;
    case '^': e2(0x31, 0xC8); break;
    case TK_SHL: e2(0xD3, 0xE0); break;         /* shl eax, cl */
    case TK_SHR: e2(0xD3, 0xF8); break;         /* sar eax, cl */
    default: {
        int cc = 0;
        switch (op) {
        case TK_EQ: cc = 0x94; break;
        case TK_NE: cc = 0x95; break;
        case '<':   cc = 0x9C; break;
        case TK_GE: cc = 0x9D; break;
        case TK_LE: cc = 0x9E; break;
        case '>':   cc = 0x9F; break;
        }
        e2(0x39, 0xC8);                         /* cmp eax, ecx */
        e3(0x0F, cc, 0xC0);                     /* setcc al */
        e3(0x0F, 0xB6, 0xC0);                   /* movzx eax, al */
    }
    }
}

static void binary(int minprec)
{
    unary();
    for (;;) {
        int op = L.tok, p = binprec(op);
        if (p == 0 || p < minprec) return;
        next();
        rvalue();
        int lt = ty;
        emit_push();
        binary(p + 1);
        rvalue();
        gen_binop(op, lt, ty);
        lv = 0;
    }
}

static void logic_and(void)
{
    binary(1);
    if (L.tok != TK_AND) return;
    u32 jumps[MAX_JMP];
    int n = 0;
    rvalue();
    while (L.tok == TK_AND) {
        next();
        if (n >= MAX_JMP) cc_error("expression too complex");
        jumps[n++] = emit_jz();
        binary(1);
        rvalue();
    }
    e2(0x85, 0xC0); e3(0x0F, 0x95, 0xC0); e3(0x0F, 0xB6, 0xC0);   /* eax = eax != 0 */
    u32 end = emit_jmp();
    for (int i = 0; i < n; i++) patch_here(jumps[i]);
    e2(0x31, 0xC0);                                                /* xor eax, eax */
    patch_here(end);
    ty = TY_INT;
    lv = 0;
}

static void logic_or(void)
{
    logic_and();
    if (L.tok != TK_OR) return;
    u32 jumps[MAX_JMP];
    int n = 0;
    rvalue();
    while (L.tok == TK_OR) {
        next();
        if (n >= MAX_JMP) cc_error("expression too complex");
        jumps[n++] = emit_jnz();
        logic_and();
        rvalue();
    }
    e2(0x85, 0xC0); e3(0x0F, 0x95, 0xC0); e3(0x0F, 0xB6, 0xC0);
    u32 end = emit_jmp();
    for (int i = 0; i < n; i++) patch_here(jumps[i]);
    emit_imm(1);
    patch_here(end);
    ty = TY_INT;
    lv = 0;
}

static void cond_expr(void)
{
    logic_or();
    if (L.tok != '?') return;
    next();
    rvalue();
    u32 jelse = emit_jz();
    expr();
    rvalue();
    int t1 = ty;
    u32 jend = emit_jmp();
    expect(':');
    patch_here(jelse);
    cond_expr();
    rvalue();
    patch_here(jend);
    if (is_ptr(t1)) ty = t1;
    lv = 0;
}

static void assign_expr(void)
{
    cond_expr();
    int op = L.tok;
    if (op == '=') {
        need_lvalue();
        int t = ty;
        next();
        emit_push();                    /* address */
        assign_expr();
        rvalue();
        emit_pop_ecx();
        store(t);
        ty = t;
        lv = 0;
        return;
    }
    int bop;
    switch (op) {
    case TK_ADDEQ: bop = '+'; break;
    case TK_SUBEQ: bop = '-'; break;
    case TK_MULEQ: bop = '*'; break;
    case TK_DIVEQ: bop = '/'; break;
    case TK_MODEQ: bop = '%'; break;
    case TK_ANDEQ: bop = '&'; break;
    case TK_OREQ:  bop = '|'; break;
    case TK_XOREQ: bop = '^'; break;
    case TK_SHLEQ: bop = TK_SHL; break;
    case TK_SHREQ: bop = TK_SHR; break;
    default: return;
    }
    need_lvalue();
    int t = ty;
    next();
    emit_push();                        /* address */
    load(t);
    emit_push();                        /* old value */
    assign_expr();
    rvalue();
    gen_binop(bop, t, ty);              /* pops the old value */
    emit_pop_ecx();
    store(t);
    ty = t;
    lv = 0;
}

static void expr(void)
{
    assign_expr();
    while (L.tok == ',') {
        next();
        rvalue();
        assign_expr();
    }
}

/* Integer constant expressions (for array sizes, case labels, global initialisers) */
static int const_primary(void)
{
    int v;
    if (L.tok == '-') { next(); return -const_primary(); }
    if (L.tok == '~') { next(); return ~const_primary(); }
    if (L.tok == '+') { next(); return const_primary(); }
    if (L.tok == '(') {
        next();
        v = const_expr();
        expect(')');
        return v;
    }
    if (L.tok == TK_NUM) { v = L.val; next(); return v; }
    if (L.tok == TK_SIZEOF) return sizeof_expr();
    if (L.tok == TK_ID) {
        sym_t *s = lookup(tokname);
        if (s && s->cls == C_CONST) { next(); return s->val; }
    }
    cc_error("constant expression expected");
}

static int const_mul(void)
{
    int v = const_primary();
    for (;;) {
        int op = L.tok;
        if (op != '*' && op != '/' && op != '%' && op != TK_SHL && op != TK_SHR) return v;
        next();
        int r = const_primary();
        if ((op == '/' || op == '%') && r == 0) cc_error("division by zero in constant");
        if (op == '*') v *= r;
        else if (op == '/') v /= r;
        else if (op == '%') v %= r;
        else if (op == TK_SHL) v <<= r;
        else v >>= r;
    }
}

static int const_expr(void)
{
    int v = const_mul();
    for (;;) {
        int op = L.tok;
        if (op != '+' && op != '-' && op != '|' && op != '&') return v;
        next();
        int r = const_mul();
        if (op == '+') v += r;
        else if (op == '-') v -= r;
        else if (op == '|') v |= r;
        else v &= r;
    }
}

/* ---- statements ------------------------------------------------------------ */
static void stmt(void);

static int local_alloc(int size)
{
    frame += (size + 3) & ~3;
    if (frame > maxframe) maxframe = frame;
    return -frame;
}

/* count the elements of a { ... } initialiser without consuming it */
static int count_init_elems(void)
{
    struct lexstate saved = L;
    char name[64];
    strcpy(name, tokname);
    int depth = 0, n = 0, any = 0;
    for (;;) {
        if (L.tok == TK_EOF) cc_error("unterminated initialiser");
        if (L.tok == '{' || L.tok == '(' || L.tok == '[') depth++;
        else if (L.tok == '}' || L.tok == ')' || L.tok == ']') {
            if (--depth == 0) break;
        } else if (depth == 1) {
            if (L.tok == ',') { n++; any = 0; }
            else any = 1;
        }
        next();
    }
    L = saved;
    strcpy(tokname, name);
    return n + any;
}

static void local_decl(void)
{
    int base = parse_base_type();
    for (;;) {
        int t = parse_stars(base);
        if (L.tok != TK_ID) cc_error("variable name expected");
        char name[32];
        strcpy(name, tokname);
        next();
        int arrlen = 0;
        if (L.tok == '[') {
            next();
            if (L.tok == ']') {
                arrlen = -1;
            } else {
                arrlen = const_expr();
                if (arrlen <= 0) cc_error("array size must be positive");
            }
            expect(']');
            if (arrlen == -1) {
                if (L.tok != '=') cc_error("array '%s' needs a size", name);
                /* peek at the initialiser to size the array */
                struct lexstate saved = L;
                next();
                if (L.tok == TK_STR && t == TY_CHAR) arrlen = tokslen + 1;
                else if (L.tok == '{') arrlen = count_init_elems();
                else cc_error("bad initialiser for array '%s'", name);
                L = saved;
                if (arrlen <= 0) arrlen = 1;
            }
        }
        if (t == TY_VOID) cc_error("variable '%s' declared void", name);
        int size = arrlen > 0 ? arrlen * ty_size(t) : 4;
        int off = local_alloc(size);
        add_local(name, t, arrlen, off);

        if (L.tok == '=') {
            next();
            if (arrlen > 0) {
                int es = ty_size(t);
                if (L.tok == TK_STR && t == TY_CHAR) {
                    if (tokslen + 1 > arrlen) cc_error("string too long for '%s'", name);
                    for (int i = 0; i <= tokslen; i++) {
                        e2(0xC6, 0x85); e4((u32)(off + i)); e1(i < tokslen ? tokstr[i] : 0);
                    }
                    next();
                } else {
                    expect('{');
                    int i = 0;
                    while (L.tok != '}') {
                        if (i >= arrlen) cc_error("too many initialisers for '%s'", name);
                        assign_expr();
                        rvalue();
                        e2(0x8D, 0x8D); e4((u32)(off + i * es));   /* lea ecx, [ebp+off] */
                        store(t);
                        i++;
                        if (L.tok != ',') break;
                        next();
                    }
                    expect('}');
                    for (; i < arrlen; i++) {                     /* zero the rest */
                        emit_imm(0);
                        e2(0x8D, 0x8D); e4((u32)(off + i * es));
                        store(t);
                    }
                }
            } else {
                assign_expr();
                rvalue();
                e2(0x8D, 0x8D); e4((u32)off);                      /* lea ecx, [ebp+off] */
                store(t);
            }
        }
        if (L.tok != ',') break;
        next();
    }
    expect(';');
}

static struct loopctx *push_loop(int is_switch)
{
    if (nloops >= MAX_LOOPS) cc_error("loops nested too deeply");
    struct loopctx *c = &loops[nloops++];
    c->is_switch = is_switch;
    c->nbrk = c->ncont = c->ncase = 0;
    c->has_default = 0;
    return c;
}

static void pop_loop(u32 brk_target, u32 cont_target)
{
    struct loopctx *c = &loops[--nloops];
    for (int i = 0; i < c->nbrk; i++) patch_to(c->brk[i], brk_target);
    for (int i = 0; i < c->ncont; i++) patch_to(c->cont[i], cont_target);
}

static void block(void)
{
    int saved_locals = nlsyms, saved_frame = frame;
    expect('{');
    while (L.tok != '}') {
        if (L.tok == TK_EOF) cc_error("missing '}'");
        stmt();
    }
    next();
    nlsyms = saved_locals;
    frame = saved_frame;
}

static void stmt(void)
{
    switch (L.tok) {
    case '{':
        block();
        return;
    case ';':
        next();
        return;
    case TK_INT: case TK_CHAR: case TK_VOID: case TK_INTMOD: case TK_QUAL:
        local_decl();
        return;

    case TK_IF: {
        next();
        expect('(');
        expr();
        rvalue();
        expect(')');
        u32 jelse = emit_jz();
        stmt();
        if (L.tok == TK_ELSE) {
            next();
            u32 jend = emit_jmp();
            patch_here(jelse);
            stmt();
            patch_here(jend);
        } else {
            patch_here(jelse);
        }
        return;
    }

    case TK_WHILE: {
        next();
        u32 top = cpos;
        expect('(');
        expr();
        rvalue();
        expect(')');
        u32 jexit = emit_jz();
        push_loop(0);
        stmt();
        emit_jmp_to(top);
        patch_here(jexit);
        pop_loop(cpos, top);
        return;
    }

    case TK_DO: {
        next();
        u32 top = cpos;
        push_loop(0);
        stmt();
        if (L.tok != TK_WHILE) cc_error("expected 'while' after do-body");
        next();
        u32 cond = cpos;
        expect('(');
        expr();
        rvalue();
        expect(')');
        expect(';');
        e2(0x85, 0xC0); e2(0x0F, 0x85); e4(top - (cpos + 4));   /* jnz top */
        pop_loop(cpos, cond);
        return;
    }

    case TK_FOR: {
        int saved_locals = nlsyms, saved_frame = frame;
        next();
        expect('(');
        if (is_type_tok(L.tok)) local_decl();
        else {
            if (L.tok != ';') expr();
            expect(';');
        }
        u32 cond = cpos;
        u32 jexit = 0;
        int has_cond = 0;
        if (L.tok != ';') {
            expr();
            rvalue();
            jexit = emit_jz();
            has_cond = 1;
        }
        expect(';');
        u32 jbody = emit_jmp();
        u32 inc = cpos;
        if (L.tok != ')') expr();
        expect(')');
        emit_jmp_to(cond);
        patch_here(jbody);
        push_loop(0);
        stmt();
        emit_jmp_to(inc);
        if (has_cond) patch_here(jexit);
        pop_loop(cpos, inc);
        nlsyms = saved_locals;
        frame = saved_frame;
        return;
    }

    case TK_SWITCH: {
        next();
        expect('(');
        expr();
        rvalue();
        expect(')');
        int tmp = local_alloc(4);
        e2(0x89, 0x85); e4((u32)tmp);       /* mov [ebp+tmp], eax */
        u32 jdispatch = emit_jmp();
        struct loopctx *c = push_loop(1);
        stmt();
        u32 jend = emit_jmp();
        patch_here(jdispatch);
        for (int i = 0; i < c->ncase; i++) {
            e2(0x8B, 0x85); e4((u32)tmp);   /* mov eax, [ebp+tmp] */
            e1(0x3D); e4((u32)c->case_val[i]);  /* cmp eax, imm32 */
            e2(0x0F, 0x84); e4(c->case_pos[i] - (cpos + 4));    /* je case */
        }
        if (c->has_default) emit_jmp_to(c->default_pos);
        patch_here(jend);
        pop_loop(cpos, 0);
        return;
    }

    case TK_CASE:
    case TK_DEFAULT: {
        int i;
        for (i = nloops - 1; i >= 0 && !loops[i].is_switch; i--) ;
        if (i < 0) cc_error("'case' outside of switch");
        struct loopctx *c = &loops[i];
        if (L.tok == TK_CASE) {
            next();
            int v = const_expr();
            if (c->ncase >= MAX_CASES) cc_error("too many case labels");
            for (int k = 0; k < c->ncase; k++)
                if (c->case_val[k] == v) cc_error("duplicate case value %d", v);
            c->case_val[c->ncase] = v;
            c->case_pos[c->ncase] = cpos;
            c->ncase++;
        } else {
            next();
            if (c->has_default) cc_error("multiple default labels");
            c->has_default = 1;
            c->default_pos = cpos;
        }
        expect(':');
        return;
    }

    case TK_BREAK:
    case TK_CONTINUE: {
        int is_break = L.tok == TK_BREAK;
        next();
        expect(';');
        int i = nloops - 1;
        if (!is_break)
            while (i >= 0 && loops[i].is_switch) i--;
        if (i < 0) cc_error(is_break ? "'break' outside of a loop or switch" : "'continue' outside of a loop");
        struct loopctx *c = &loops[i];
        u32 j = emit_jmp();
        if (is_break) {
            if (c->nbrk >= MAX_JMP) cc_error("too many breaks");
            c->brk[c->nbrk++] = j;
        } else {
            if (c->ncont >= MAX_JMP) cc_error("too many continues");
            c->cont[c->ncont++] = j;
        }
        return;
    }

    case TK_RETURN:
        next();
        if (L.tok != ';') {
            expr();
            rvalue();
        }
        expect(';');
        emit_epilogue();
        return;

    default:
        expr();
        expect(';');
        return;
    }
}

/* ---- top level ------------------------------------------------------------- */
static void function(const char *name, int rettype)
{
    char pnames[16][32];
    int ptypes[16];
    int np = 0;

    next();     /* '(' */
    if (L.tok == TK_VOID && peek() == ')') next();
    while (L.tok != ')') {
        if (np >= 16) cc_error("too many parameters");
        if (!is_type_tok(L.tok)) cc_error("parameter type expected");
        int t = parse_stars(parse_base_type());
        if (L.tok == TK_ID) {
            strcpy(pnames[np], tokname);
            next();
        } else {
            pnames[np][0] = 0;
        }
        if (L.tok == '[') {         /* array parameter == pointer */
            next();
            if (L.tok != ']') const_expr();
            expect(']');
            t += TY_PTR;
        }
        ptypes[np++] = t;
        if (L.tok != ',') break;
        next();
    }
    expect(')');

    sym_t *s = find_global(name);
    if (s && s->cls == C_FUNC) {
        if (s->nparams >= 0 && s->nparams != np) cc_error("conflicting declaration of '%s'", name);
    } else {
        s = add_global(name, C_FUNC);
    }
    s->type = rettype;
    s->nparams = (i16)np;

    if (L.tok == ';') {         /* prototype only */
        next();
        return;
    }
    if (s->defined) cc_error("function '%s' defined twice", name);
    s->defined = 1;
    s->val = (int)cpos;

    nlsyms = 0;
    frame = maxframe = 0;
    for (int i = 0; i < np; i++)
        if (pnames[i][0]) add_local(pnames[i], ptypes[i], 0, 8 + 4 * i);

    e1(0x55); e2(0x89, 0xE5);               /* push ebp; mov ebp, esp */
    e2(0x81, 0xEC); e4(0);                  /* sub esp, frame (patched) */
    u32 frame_patch = cpos - 4;
    block();
    e2(0x31, 0xC0);                         /* return 0 by default */
    emit_epilogue();
    put4(frame_patch, (u32)((maxframe + 15) & ~15));
    nlsyms = 0;
}

static void global_decl(void)
{
    int base = parse_base_type();
    for (;;) {
        int t = parse_stars(base);
        if (L.tok != TK_ID) cc_error("identifier expected");
        char name[32];
        strcpy(name, tokname);
        next();
        if (L.tok == '(') {
            function(name, t);
            return;
        }

        int arrlen = 0;
        if (L.tok == '[') {
            next();
            if (L.tok == ']') arrlen = -1;
            else {
                arrlen = const_expr();
                if (arrlen <= 0) cc_error("array size must be positive");
            }
            expect(']');
        }
        if (t == TY_VOID) cc_error("variable '%s' declared void", name);

        int es = ty_size(t);
        int ninit = 0;
        int strinit = -1;           /* char array initialised from a string: data offset */
        int strlen_init = 0;

        if (L.tok == '=') {
            next();
            if (arrlen != 0 && t == TY_CHAR && L.tok == TK_STR) {
                u32 a = string_literal() - PROG_DATA;
                strlen_init = (int)(dpos - a);      /* includes the NUL */
                strinit = (int)a;
            } else if (arrlen != 0) {
                expect('{');
                while (L.tok != '}') {
                    if (ninit >= MAX_INIT) cc_error("initialiser too long");
                    if (L.tok == TK_STR) initvals[ninit++] = (int)string_literal();
                    else initvals[ninit++] = const_expr();
                    if (L.tok != ',') break;
                    next();
                }
                expect('}');
            } else {
                if (L.tok == TK_STR) initvals[ninit++] = (int)string_literal();
                else initvals[ninit++] = const_expr();
            }
        }

        if (arrlen == -1) {
            arrlen = strinit >= 0 ? strlen_init : ninit;
            if (arrlen <= 0) cc_error("array '%s' needs a size", name);
        }
        if (arrlen > 0 && ninit > arrlen) cc_error("too many initialisers for '%s'", name);
        if (strinit >= 0 && strlen_init > arrlen) cc_error("string too long for '%s'", name);

        u32 off = data_alloc((u32)((arrlen > 0 ? arrlen : 1) * es), es == 4 ? 4 : 1);
        sym_t *s = add_global(name, C_GLOBAL);
        s->type = t;
        s->arrlen = arrlen;
        s->val = (int)off;
        if (strinit >= 0) memcpy(data + off, data + strinit, (u32)strlen_init);
        for (int i = 0; i < ninit; i++) {
            if (es == 1) data[off + i] = (u8)initvals[i];
            else memcpy(data + off + i * 4, &initvals[i], 4);
        }

        if (L.tok != ',') break;
        next();
    }
    expect(';');
}

/* ---- driver ---------------------------------------------------------------- */
const struct cc_builtin cc_builtins[] = {
#define API(name, fn, nargs, ret) { #name, nargs, ret },
    API_LIST
#undef API
    { NULL, 0, 0 }
};

static const struct { const char *name; int val; } predef_consts[] = {
    {"NULL", 0}, {"EOF", -1}, {"true", 1}, {"false", 0},
    {"KEY_UP", KEY_UP}, {"KEY_DOWN", KEY_DOWN}, {"KEY_LEFT", KEY_LEFT},
    {"KEY_RIGHT", KEY_RIGHT}, {"KEY_HOME", KEY_HOME}, {"KEY_END", KEY_END},
    {"KEY_PGUP", KEY_PGUP}, {"KEY_PGDN", KEY_PGDN}, {"KEY_DEL", KEY_DEL},
    {"KEY_ESC", 27}, {"KEY_ENTER", '\n'}, {"KEY_BACKSPACE", '\b'},
    {"BLACK", 0}, {"BLUE", 1}, {"GREEN", 2}, {"CYAN", 3}, {"RED", 4}, {"MAGENTA", 5},
    {"BROWN", 6}, {"LIGHTGRAY", 7}, {"DARKGRAY", 8}, {"LIGHTBLUE", 9},
    {"LIGHTGREEN", 10}, {"LIGHTCYAN", 11}, {"LIGHTRED", 12}, {"LIGHTMAGENTA", 13},
    {"YELLOW", 14}, {"WHITE", 15}, {"SCREEN_W", CON_W}, {"SCREEN_H", CON_H},
    {NULL, 0}
};

static void cleanup(void)
{
    kfree(gsyms);
    kfree(lsyms);
    kfree(fixes);
    kfree(loops);
    kfree(initvals);
    kfree(tokstr);
    gsyms = lsyms = NULL;
    fixes = NULL;
    loops = NULL;
    initvals = NULL;
    tokstr = NULL;
}

int cc_compile(const char *src, const char *fname, struct cc_result *out)
{
    filename = fname;
    gsyms = kzalloc(sizeof(sym_t) * MAX_GSYMS);
    lsyms = kzalloc(sizeof(sym_t) * MAX_LSYMS);
    fixes = kzalloc(sizeof(struct fixup) * MAX_FIX);
    loops = kzalloc(sizeof(struct loopctx) * MAX_LOOPS);
    initvals = kzalloc(sizeof(int) * MAX_INIT);
    tokstr = kzalloc(STRBUF);
    if (!gsyms || !lsyms || !fixes || !loops || !initvals || !tokstr) {
        kprintf("cc: out of memory\n");
        cleanup();
        return -1;
    }
    ngsyms = nlsyms = nfix = nloops = 0;
    code = (u8 *)PROG_CODE;
    data = (u8 *)PROG_DATA;
    cpos = dpos = 0;
    memset(data, 0, PROG_DATA_MAX);
    L.p = src;
    L.line = 1;

    if (k_setjmp(cc_jb)) {
        cleanup();
        return -1;
    }

    for (int i = 0; cc_builtins[i].name; i++) {
        sym_t *s = add_global(cc_builtins[i].name, C_BUILTIN);
        s->val = i;
        s->nparams = (i16)cc_builtins[i].nargs;
        s->type = cc_builtins[i].rettype;
        s->defined = 1;
        s->predef = 1;
    }
    for (int i = 0; predef_consts[i].name; i++) {
        sym_t *s = add_global(predef_consts[i].name, C_CONST);
        s->val = predef_consts[i].val;
        s->type = TY_INT;
        s->predef = 1;
    }

    data_alloc(4, 4);       /* keep offset 0 unused so no object sits at PROG_DATA */

    next();
    while (L.tok != TK_EOF) {
        if (!is_type_tok(L.tok) && L.tok != TK_ID)
            cc_error("declaration expected but found %s", tokdesc(L.tok));
        if (L.tok == TK_ID) {
            /* "main() { }" - old style implicit int function */
            if (peek() != '(') cc_error("unknown type name '%s'", tokname);
            char name[32];
            strcpy(name, tokname);
            next();
            function(name, TY_INT);
            continue;
        }
        global_decl();
    }

    for (int i = 0; i < nfix; i++) {
        sym_t *s = &gsyms[fixes[i].sym];
        if (s->cls != C_FUNC || !s->defined) {
            L.line = fixes[i].line;
            cc_error("undefined function '%s'", s->name);
        }
        patch_to(fixes[i].pos, (u32)s->val);
    }

    sym_t *m = find_global("main");
    if (!m || m->cls != C_FUNC || !m->defined) {
        L.line = 0;
        cc_error("no main() function");
    }

    out->entry = (u32)m->val;
    out->code_size = cpos;
    out->data_size = dpos;
    u32 init = dpos;
    while (init > 0 && data[init - 1] == 0) init--;
    out->data_init = init;
    cleanup();
    return 0;
}
