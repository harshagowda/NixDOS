// hello.c - your first NixC program
// Compile and run it with:   cc hello.c
// Or build an executable:    cc hello.c -o hello.nxe   then type: hello

int main(int argc, char **argv)
{
    printf("Hello from NixDOS!\n");
    printf("This program was compiled by the NixC compiler, inside NixDOS.\n");
    if (argc > 1) {
        int i;
        printf("You passed %d argument(s):\n", argc - 1);
        for (i = 1; i < argc; i++)
            printf("  argv[%d] = %s\n", i, argv[i]);
    }
    return 0;
}
