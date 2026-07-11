// Paddle Bounce - a Breakout/paddle-and-ball game for the Nintendo DS
//
// Mechanic: a horizontal paddle at the bottom of the screen deflects a
// bouncing ball up into a wall of bricks. Clear every brick to win the
// level (score bonus + next level, faster ball); miss the ball with the
// paddle three times and it's game over. Genuinely distinct from every
// other game in the arcade so far: none of them involve real-time
// continuous ball physics against a paddle.
//
// Rendered entirely with libnds' text console (no tile/graphics engine
// needed) - same low-risk approach as maze-muncher/grid-slither/card-flip.

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FIELD_W 32
#define FIELD_H 22
#define PADDLE_W 5
#define BRICK_ROWS 5
#define BRICK_COLS 14
#define BRICK_TOP 2
#define STARTING_LIVES 3

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_WIN_LEVEL, STATE_LOSE } GameState;

static int paddleX;
static int ballX16, ballY16;  // fixed point, 16 sub-units per cell
static int ballDX16, ballDY16;
static bool bricks[BRICK_ROWS][BRICK_COLS];
static int bricksLeft;
static int score;
static int lives;
static int level;
static int ballSpeedBase;

static void placeBricks(void) {
    for (int r = 0; r < BRICK_ROWS; r++)
        for (int c = 0; c < BRICK_COLS; c++)
            bricks[r][c] = true;
    bricksLeft = BRICK_ROWS * BRICK_COLS;
}

static void serveBall(void) {
    ballX16 = (paddleX + PADDLE_W / 2) * 16;
    ballY16 = (FIELD_H - 3) * 16;
    int speed = ballSpeedBase;
    ballDX16 = (rand() % 2 == 0) ? speed : -speed;
    ballDY16 = -speed;
}

static void loadLevel(int newLevel) {
    level = newLevel;
    ballSpeedBase = 6 + (level - 1) * 2;
    if (ballSpeedBase > 14) ballSpeedBase = 14;
    placeBricks();
    paddleX = (FIELD_W - PADDLE_W) / 2;
    serveBall();
}

static void newGame(void) {
    score = 0;
    lives = STARTING_LIVES;
    loadLevel(1);
}

static void drawPlaying(void) {
    consoleClear();

    char grid[FIELD_H][FIELD_W + 1];
    for (int y = 0; y < FIELD_H; y++) {
        memset(grid[y], ' ', FIELD_W);
        grid[y][FIELD_W] = '\0';
    }

    // bricks
    int brickAreaW = BRICK_COLS * 2;
    int brickOffsetX = (FIELD_W - brickAreaW) / 2;
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!bricks[r][c]) continue;
            int x = brickOffsetX + c * 2;
            int y = BRICK_TOP + r;
            if (x >= 0 && x < FIELD_W) grid[y][x] = '=';
            if (x + 1 >= 0 && x + 1 < FIELD_W) grid[y][x + 1] = '=';
        }
    }

    // paddle
    int py = FIELD_H - 1;
    for (int i = 0; i < PADDLE_W; i++) {
        int x = paddleX + i;
        if (x >= 0 && x < FIELD_W) grid[py][x] = '#';
    }

    // ball
    int bx = ballX16 / 16;
    int by = ballY16 / 16;
    if (bx >= 0 && bx < FIELD_W && by >= 0 && by < FIELD_H) grid[by][bx] = 'o';

    iprintf("+");
    for (int x = 0; x < FIELD_W; x++) iprintf("-");
    iprintf("+\n");
    for (int y = 0; y < FIELD_H; y++)
        iprintf("|%s|\n", grid[y]);
    iprintf("+");
    for (int x = 0; x < FIELD_W; x++) iprintf("-");
    iprintf("+\n");
    iprintf("Score:%5d  Lives:%d  Level:%d\n", score, lives, level);
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        PADDLE BOUNCE\n\n");
    iprintf("   Bounce the ball into the\n");
    iprintf("   bricks to clear the level.\n\n");
    iprintf("   D-Pad Left/Right : move paddle\n");
    iprintf("   START : begin / restart\n\n");
    iprintf("        Press START\n");
}

static void drawWinLevel(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        LEVEL %d CLEAR!\n\n", level);
    iprintf("        Score: %d\n\n", score);
    iprintf("        Press START for level %d\n", level + 1);
}

static void drawLose(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        GAME OVER\n\n");
    iprintf("        Final score: %d\n", score);
    iprintf("        Reached level: %d\n\n", level);
    iprintf("        Press START to try again\n");
}

static void updatePhysics(void) {
    int nx = ballX16 + ballDX16;
    int ny = ballY16 + ballDY16;

    int cellX = nx / 16;
    int cellY = ny / 16;

    // wall bounces
    if (cellX <= 0) {
        cellX = 0;
        nx = 0;
        ballDX16 = -ballDX16;
    } else if (cellX >= FIELD_W - 1) {
        cellX = FIELD_W - 1;
        nx = cellX * 16;
        ballDX16 = -ballDX16;
    }
    if (cellY <= 0) {
        cellY = 0;
        ny = 0;
        ballDY16 = -ballDY16;
    }

    // brick collision (check the cell the ball is entering)
    int brickAreaW = BRICK_COLS * 2;
    int brickOffsetX = (FIELD_W - brickAreaW) / 2;
    if (cellY >= BRICK_TOP && cellY < BRICK_TOP + BRICK_ROWS) {
        int col = (cellX - brickOffsetX) / 2;
        int row = cellY - BRICK_TOP;
        if (col >= 0 && col < BRICK_COLS && bricks[row][col]) {
            bricks[row][col] = false;
            bricksLeft--;
            score += 10;
            ballDY16 = -ballDY16;
        }
    }

    // paddle collision
    int py = FIELD_H - 1;
    if (cellY >= py) {
        if (cellX >= paddleX && cellX < paddleX + PADDLE_W) {
            // reflect and add spin based on where it hit the paddle
            int hitOffset = cellX - (paddleX + PADDLE_W / 2);
            ballDX16 = hitOffset * 3;
            if (ballDX16 == 0) ballDX16 = (rand() % 2 == 0) ? 3 : -3;
            ballDY16 = -abs(ballDY16);
            ny = py * 16;
            cellY = py;
        } else {
            // missed - lose a life
            lives--;
            if (lives <= 0) {
                return; // caller checks lives<=0 -> lose state
            }
            serveBall();
            return;
        }
    }

    ballX16 = nx;
    ballY16 = ny;
}

int main(void) {
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleDemoInit(); // sets up main-screen text console by default

    GameState state = STATE_TITLE;
    drawTitle();

    int frame = 0;

    while (1) {
        scanKeys();
        int down = keysDown();
        int held = keysHeld();

        switch (state) {
            case STATE_TITLE:
                if (down & KEY_START) {
                    newGame();
                    state = STATE_PLAYING;
                    drawPlaying();
                }
                break;

            case STATE_PLAYING: {
                if (held & KEY_LEFT) paddleX -= 1;
                if (held & KEY_RIGHT) paddleX += 1;
                if (paddleX < 0) paddleX = 0;
                if (paddleX > FIELD_W - PADDLE_W) paddleX = FIELD_W - PADDLE_W;

                frame++;
                if (frame % 3 == 0) {
                    updatePhysics();
                    if (lives <= 0) {
                        state = STATE_LOSE;
                        drawLose();
                        break;
                    }
                    if (bricksLeft <= 0) {
                        state = STATE_WIN_LEVEL;
                        drawWinLevel();
                        break;
                    }
                }
                drawPlaying();
                break;
            }

            case STATE_WIN_LEVEL:
                if (down & KEY_START) {
                    loadLevel(level + 1);
                    state = STATE_PLAYING;
                    drawPlaying();
                }
                break;

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
