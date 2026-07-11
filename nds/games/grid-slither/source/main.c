// Grid Slither - a classic snake game for the Nintendo DS
//
// Mechanic: guide a growing snake around a bounded grid to eat apples
// ('*'). Each apple grows the snake by one segment and adds to score.
// The game ends if the snake runs into a wall or into itself. START
// restarts from the title screen at any time.
//
// Rendered entirely with libnds' text console (no tile/graphics engine
// needed) - same low-risk approach as maze-muncher, but a genuinely
// different mechanic: continuous self-collision-avoiding growth rather
// than a fixed maze with a chasing enemy.

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GRID_W 30
#define GRID_H 20
#define MAX_LEN (GRID_W * GRID_H)
#define MOVE_PERIOD 6 // frames per snake step; lower = faster

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_LOSE } GameState;
typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Direction;

static int snakeX[MAX_LEN];
static int snakeY[MAX_LEN];
static int snakeLen;
static Direction dir;
static int foodX, foodY;
static int score;
static int moveCounter;

static bool occupied(int x, int y) {
    for (int i = 0; i < snakeLen; i++)
        if (snakeX[i] == x && snakeY[i] == y) return true;
    return false;
}

static void placeFood(void) {
    // grid is small relative to snake length for most of the game, so a
    // simple rejection sample is fine and keeps the logic obvious.
    int x, y;
    do {
        x = rand() % GRID_W;
        y = rand() % GRID_H;
    } while (occupied(x, y));
    foodX = x;
    foodY = y;
}

static void loadLevel(void) {
    snakeLen = 4;
    dir = DIR_RIGHT;
    score = 0;
    moveCounter = 0;
    int startX = GRID_W / 4;
    int startY = GRID_H / 2;
    for (int i = 0; i < snakeLen; i++) {
        snakeX[i] = startX - i;
        snakeY[i] = startY;
    }
    placeFood();
}

static void drawPlaying(void) {
    consoleClear();
    char grid[GRID_H][GRID_W + 1];
    for (int y = 0; y < GRID_H; y++) {
        memset(grid[y], ' ', GRID_W);
        grid[y][GRID_W] = '\0';
    }
    grid[foodY][foodX] = '*';
    for (int i = 1; i < snakeLen; i++)
        grid[snakeY[i]][snakeX[i]] = 'o';
    grid[snakeY[0]][snakeX[0]] = '@';

    iprintf("+");
    for (int x = 0; x < GRID_W; x++) iprintf("-");
    iprintf("+\n");
    for (int y = 0; y < GRID_H; y++)
        iprintf("|%s|\n", grid[y]);
    iprintf("+");
    for (int x = 0; x < GRID_W; x++) iprintf("-");
    iprintf("+\n");
    iprintf("Score:%4d   Length:%3d\n", score, snakeLen);
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        GRID SLITHER\n\n");
    iprintf("   Eat apples ('*') to grow.\n");
    iprintf("   Don't hit walls or yourself!\n\n");
    iprintf("   D-Pad : steer\n");
    iprintf("   START : begin / restart\n\n");
    iprintf("        Press START\n");
}

static void drawLose(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        GAME OVER\n\n");
    iprintf("        Final score: %d\n", score);
    iprintf("        Final length: %d\n\n", snakeLen);
    iprintf("        Press START to try again\n");
}

int main(void) {
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleDemoInit(); // sets up main-screen text console by default

    GameState state = STATE_TITLE;
    drawTitle();

    while (1) {
        scanKeys();
        int down = keysDown();

        switch (state) {
            case STATE_TITLE:
                if (down & KEY_START) {
                    loadLevel();
                    state = STATE_PLAYING;
                    drawPlaying();
                }
                break;

            case STATE_PLAYING: {
                // buffer direction changes immediately for responsiveness,
                // but reject 180-degree reversals into the snake's own neck
                if ((down & KEY_UP) && dir != DIR_DOWN) dir = DIR_UP;
                else if ((down & KEY_DOWN) && dir != DIR_UP) dir = DIR_DOWN;
                else if ((down & KEY_LEFT) && dir != DIR_RIGHT) dir = DIR_LEFT;
                else if ((down & KEY_RIGHT) && dir != DIR_LEFT) dir = DIR_RIGHT;

                moveCounter++;
                if (moveCounter >= MOVE_PERIOD) {
                    moveCounter = 0;

                    int nx = snakeX[0];
                    int ny = snakeY[0];
                    switch (dir) {
                        case DIR_UP:    ny--; break;
                        case DIR_DOWN:  ny++; break;
                        case DIR_LEFT:  nx--; break;
                        case DIR_RIGHT: nx++; break;
                    }

                    bool hitWall = (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H);
                    bool ateFood = (nx == foodX && ny == foodY);
                    // the tail cell vacates this step unless we're eating,
                    // so moving into the current tail position is legal
                    bool hitSelf = false;
                    int checkLen = ateFood ? snakeLen : snakeLen - 1;
                    for (int i = 0; i < checkLen; i++) {
                        if (snakeX[i] == nx && snakeY[i] == ny) { hitSelf = true; break; }
                    }

                    if (hitWall || hitSelf) {
                        state = STATE_LOSE;
                        drawLose();
                        break;
                    }

                    if (ateFood) {
                        snakeLen++;
                        score += 10;
                    }
                    for (int i = snakeLen - 1; i > 0; i--) {
                        snakeX[i] = snakeX[i - 1];
                        snakeY[i] = snakeY[i - 1];
                    }
                    snakeX[0] = nx;
                    snakeY[0] = ny;

                    if (ateFood) placeFood();

                    drawPlaying();
                }

                if (down & KEY_START) {
                    state = STATE_TITLE;
                    drawTitle();
                }
                break;
            }

            case STATE_LOSE:
                if (down & KEY_START) {
                    state = STATE_TITLE;
                    drawTitle();
                }
                break;
        }

        swiWaitForVBlank();
    }

    return 0;
}
