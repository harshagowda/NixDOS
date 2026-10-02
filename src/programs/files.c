// files.c - programs can read and write files too
int main()
{
    char *msg = "written by a NixC program\n";
    char buf[128];

    if (writefile("out.txt", msg, strlen(msg)) != 0) {
        printf("write failed\n");
        return 1;
    }
    int n = readfile("out.txt", buf, 127);
    buf[n] = 0;
    printf("read back: %s", buf);
    printf("(%d bytes) - try 'cat out.txt'\n", n);
    return 0;
}
