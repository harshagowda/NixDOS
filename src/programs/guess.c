// guess.c - guess the number game
int main()
{
    char line[16];
    int secret, guess, tries = 0;
    srand(time());
    secret = rand() % 100 + 1;
    printf("I'm thinking of a number between 1 and 100.\n");
    for (;;) {
        printf("Your guess: ");
        gets(line, 16);
        guess = atoi(line);
        tries++;
        if (guess < secret) printf("Too low!\n");
        else if (guess > secret) printf("Too high!\n");
        else break;
    }
    setcolor(LIGHTGREEN, BLACK);
    printf("Correct! You got it in %d tries.\n", tries);
    beep(880, 200);
    return 0;
}
