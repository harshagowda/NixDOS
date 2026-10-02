// snake.c - full-screen snake game (arrow keys to steer, Esc to quit)
#define W 80
#define H 24
#define MAXLEN 400

int sx[MAXLEN], sy[MAXLEN];
int len, dir, foodx, foody, score;

void place_food()
{
    int ok = 0, i;
    while (!ok) {
        foodx = rand() % (W - 2) + 1;
        foody = rand() % (H - 3) + 2;
        ok = 1;
        for (i = 0; i < len; i++)
            if (sx[i] == foodx && sy[i] == foody) ok = 0;
    }
    putat(foodx, foody, '*', LIGHTRED);
}

void draw_border()
{
    int i;
    for (i = 0; i < W; i++) {
        putat(i, 1, '#', DARKGRAY);
        putat(i, H - 1, '#', DARKGRAY);
    }
    for (i = 1; i < H; i++) {
        putat(0, i, '#', DARKGRAY);
        putat(W - 1, i, '#', DARKGRAY);
    }
}

void status()
{
    char line[80];
    int i;
    sprintf(line, " SNAKE   score: %d   arrows steer, Esc quits", score);
    for (i = 0; i < W; i++) putat(i, 0, ' ', 0x70);
    for (i = 0; line[i]; i++) putat(i, 0, line[i], 0x70);
}

int main()
{
    int i, dx, dy, nx, ny, k;
    srand(time());
    cls();
    draw_border();
    len = 4;
    for (i = 0; i < len; i++) {
        sx[i] = 40 - i;
        sy[i] = 12;
        putat(sx[i], sy[i], i == 0 ? '@' : 'o', LIGHTGREEN);
    }
    dir = KEY_RIGHT;
    score = 0;
    place_food();
    status();

    for (;;) {
        while (kbhit()) {
            k = getkey();
            if (k == KEY_ESC) { cls(); printf("Bye! Final score: %d\n", score); return 0; }
            if ((k == KEY_UP && dir != KEY_DOWN) || (k == KEY_DOWN && dir != KEY_UP) ||
                (k == KEY_LEFT && dir != KEY_RIGHT) || (k == KEY_RIGHT && dir != KEY_LEFT))
                dir = k;
        }
        dx = 0; dy = 0;
        switch (dir) {
        case KEY_UP: dy = -1; break;
        case KEY_DOWN: dy = 1; break;
        case KEY_LEFT: dx = -1; break;
        case KEY_RIGHT: dx = 1; break;
        }
        nx = sx[0] + dx;
        ny = sy[0] + dy;
        int dead = nx <= 0 || nx >= W - 1 || ny <= 1 || ny >= H - 1;
        for (i = 0; i < len - 1; i++)
            if (sx[i] == nx && sy[i] == ny) dead = 1;
        if (dead) break;

        int grow = nx == foodx && ny == foody;
        if (!grow) putat(sx[len - 1], sy[len - 1], ' ', LIGHTGRAY);
        else if (len < MAXLEN) { len++; score += 10; beep(1200, 20); }
        for (i = len - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
        sx[0] = nx;
        sy[0] = ny;
        putat(sx[1], sy[1], 'o', LIGHTGREEN);
        putat(nx, ny, '@', YELLOW);
        if (grow) { place_food(); status(); }
        sleep(dy ? 120 : 80);
    }
    gotoxy(30, 12);
    setcolor(WHITE, RED);
    printf(" GAME OVER - score %d ", score);
    setcolor(LIGHTGRAY, BLACK);
    gotoxy(0, H);
    printf("\n");
    return 0;
}
