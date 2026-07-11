// Brick Smash - a Breakout/paddle-ball game for 3DS.
//
// Mechanic: a paddle on the bottom row bounces a ball into a grid of bricks
// above. Move the paddle left/right, keep the ball alive, and clear every
// brick to win; let the ball fall past the paddle too many times and it's
// game over. Physics/reflection gameplay against a fixed brick field,
// mechanically distinct from every other game in this repo: simon-sez
// (memory pattern), block-drop (falling-block puzzle), meteor-dash
// (dodge/survival), vault-run (ordered-collection maze), sky-hopper
// (vertical climbing jumper), asteroid-field (rotate/thrust shooter),
// gem-swap (tile-swap match-3), reflex-dig (whack-a-mole reaction), and the
// NDS titles (chase, snake, memory-matching card-flip, top-down shooter,
// endless runner, rhythm gate, bubble shooter).

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIELD_W 40
#define FIELD_H 22

#define BRICK_COLS 10
#define BRICK_ROWS 5
#define BRICK_TOP 2

#define PADDLE_ROW (FIELD_H - 2)
#define PADDLE_W 6

#define START_LIVES 3

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE
} GameState;

static int bricks[BRICK_ROWS][BRICK_COLS];
static int bricksLeft;

static int paddleX;
static float ballX, ballY;
static float ballVX, ballVY;
static int lives;
static int score;
static int best;

static void resetBall(void) {
    ballX = (float)(FIELD_W / 2);
    ballY = (float)(PADDLE_ROW - 1);
    ballVX = ((rand() % 2) ? 1.0f : -1.0f) * 0.5f;
    ballVY = -0.6f;
}

static void resetGame(void) {
    for (int r = 0; r < BRICK_ROWS; r++)
        for (int c = 0; c < BRICK_COLS; c++)
            bricks[r][c] = 1;
    bricksLeft = BRICK_ROWS * BRICK_COLS;

    paddleX = FIELD_W / 2 - PADDLE_W / 2;
    lives = START_LIVES;
    score = 0;
    resetBall();
}

static int brickColWidth(void) {
    return FIELD_W / BRICK_COLS;
}

int main(int argc, char **argv) {
    gfxInitDefault();
    PrintConsole topScreen;
    consoleInit(GFX_TOP, &topScreen);

    srand((unsigned int)svcGetSystemTick());

    GameState state = STATE_TITLE;
    resetGame();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        switch (state) {
            case STATE_TITLE: {
                if (kDown & KEY_A) {
                    resetGame();
                    state = STATE_PLAY;
                }
                break;
            }

            case STATE_PLAY: {
                if (kHeld & (KEY_LEFT | KEY_DLEFT | KEY_CPAD_LEFT)) {
                    paddleX -= 1;
                    if (paddleX < 0) paddleX = 0;
                }
                if (kHeld & (KEY_RIGHT | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                    paddleX += 1;
                    if (paddleX > FIELD_W - PADDLE_W) paddleX = FIELD_W - PADDLE_W;
                }

                ballX += ballVX;
                ballY += ballVY;

                // wall bounces
                if (ballX <= 0) { ballX = 0; ballVX = -ballVX; }
                if (ballX >= FIELD_W - 1) { ballX = FIELD_W - 1; ballVX = -ballVX; }
                if (ballY <= 0) { ballY = 0; ballVY = -ballVY; }

                // brick collision
                int by = (int)(ballY + 0.5f);
                int bx = (int)(ballX + 0.5f);
                if (by >= BRICK_TOP && by < BRICK_TOP + BRICK_ROWS) {
                    int col = bx / brickColWidth();
                    int row = by - BRICK_TOP;
                    if (col >= 0 && col < BRICK_COLS && row >= 0 && row < BRICK_ROWS &&
                        bricks[row][col]) {
                        bricks[row][col] = 0;
                        bricksLeft--;
                        score += 10;
                        ballVY = -ballVY;
                    }
                }

                // paddle collision
                int py = (int)(ballY + 0.5f);
                if (py == PADDLE_ROW) {
                    int px = (int)(ballX + 0.5f);
                    if (px >= paddleX && px < paddleX + PADDLE_W && ballVY > 0) {
                        ballVY = -ballVY;
                        // add spin based on where it hit the paddle
                        float hitPos = (ballX - (paddleX + PADDLE_W / 2.0f)) / (PADDLE_W / 2.0f);
                        ballVX += hitPos * 0.4f;
                        if (ballVX > 0.9f) ballVX = 0.9f;
                        if (ballVX < -0.9f) ballVX = -0.9f;
                    }
                }

                // ball falls past paddle
                if (ballY >= FIELD_H - 1) {
                    lives--;
                    if (lives <= 0) {
                        if (score > best) best = score;
                        state = STATE_LOSE;
                    } else {
                        resetBall();
                    }
                }

                if (bricksLeft <= 0) {
                    if (score > best) best = score;
                    state = STATE_WIN;
                }
                break;
            }

            case STATE_WIN:
            case STATE_LOSE: {
                if (kDown & KEY_A) {
                    resetGame();
                    state = STATE_TITLE;
                }
                break;
            }
        }

        // ---- render ----
        consoleSelect(&topScreen);
        consoleClear();
        printf("\x1b[0;0HBRICK SMASH\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Bounce the ball, clear every brick.\n");
                printf("\x1b[5;0H  Circle Pad/D-Pad Left-Right = paddle\n");
                printf("\x1b[7;0H  Let the ball fall %d times and\n", START_LIVES);
                printf("\x1b[8;0H  it's game over.\n");
                if (best > 0) {
                    printf("\x1b[11;0H  Best score: %d\n", best);
                }
                printf("\x1b[14;0H  Press A to start.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_PLAY: {
                // bricks
                for (int r = 0; r < BRICK_ROWS; r++) {
                    printf("\x1b[%d;0H", BRICK_TOP + r);
                    for (int c = 0; c < BRICK_COLS; c++) {
                        int w = brickColWidth();
                        for (int i = 0; i < w; i++) {
                            putchar(bricks[r][c] ? '#' : ' ');
                        }
                    }
                    putchar('\n');
                }

                // ball
                int bxp = (int)(ballX + 0.5f);
                int byp = (int)(ballY + 0.5f);
                if (byp >= 0 && byp < FIELD_H) {
                    printf("\x1b[%d;%dHO\n", byp, bxp);
                }

                // paddle
                printf("\x1b[%d;0H", PADDLE_ROW);
                for (int i = 0; i < FIELD_W; i++) {
                    if (i >= paddleX && i < paddleX + PADDLE_W) putchar('=');
                    else putchar(' ');
                }
                putchar('\n');

                printf("\x1b[%d;0HLives: ", FIELD_H + 1);
                for (int i = 0; i < lives; i++) putchar('*');
                for (int i = lives; i < START_LIVES; i++) putchar('.');
                printf("   Score: %d   Bricks left: %d\n", score, bricksLeft);
                break;
            }

            case STATE_WIN:
                printf("\x1b[4;0H  ALL BRICKS SMASHED!\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[10;0H  Press A to return to title.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_LOSE:
                printf("\x1b[4;0H  OUT OF LIVES\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[8;0H  %d bricks left.\n", bricksLeft);
                printf("\x1b[10;0H  Press A to return to title.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
