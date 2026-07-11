// Reflex Dig - a whack-a-mole reaction game for 3DS.
//
// Mechanic: a 4x3 grid of holes. One hole at a time pops up a mole for a
// shrinking time window. Move the cursor with the D-Pad/Circle Pad onto the
// lit hole and press A before the timer runs out to score. Miss (timer
// expires or wrong hole) costs a life. Reach the target score to win before
// 3 lives run out. Reaction/timing gameplay against a static random target,
// mechanically distinct from every other game in this repo: simon-sez
// (memory pattern), block-drop (falling-block puzzle), meteor-dash
// (dodge/survival), vault-run (ordered-collection maze), sky-hopper
// (vertical climbing jumper), asteroid-field (rotate/thrust shooter),
// gem-swap (tile-swap match-3), and the NDS titles (chase, snake,
// memory-matching card-flip, breakout, shooter, endless runner, rhythm gate).

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_W 4
#define GRID_H 3
#define START_LIVES 3
#define TARGET_SCORE 15
#define MAX_TICKS 90   // ~1.5s at 60fps for the first mole
#define MIN_TICKS 24   // ~0.4s floor as difficulty ramps

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE
} GameState;

static int cursorX, cursorY;
static int moleX, moleY;
static int moleActive;
static int moleTicks;
static int moleWindow;
static int score;
static int lives;
static int best;

static void spawnMole(void) {
    moleX = rand() % GRID_W;
    moleY = rand() % GRID_H;
    moleActive = 1;

    moleWindow = MAX_TICKS - score * 4;
    if (moleWindow < MIN_TICKS) moleWindow = MIN_TICKS;
    moleTicks = moleWindow;
}

static void resetGame(void) {
    cursorX = 0;
    cursorY = 0;
    score = 0;
    lives = START_LIVES;
    moleActive = 0;
    spawnMole();
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
                if (kDown & (KEY_UP | KEY_DUP)) cursorY = (cursorY - 1 + GRID_H) % GRID_H;
                else if (kDown & (KEY_DOWN | KEY_DDOWN)) cursorY = (cursorY + 1) % GRID_H;
                else if (kDown & (KEY_LEFT | KEY_DLEFT)) cursorX = (cursorX - 1 + GRID_W) % GRID_W;
                else if (kDown & (KEY_RIGHT | KEY_DRIGHT)) cursorX = (cursorX + 1) % GRID_W;

                if (kDown & KEY_A) {
                    if (moleActive && cursorX == moleX && cursorY == moleY) {
                        score++;
                        moleActive = 0;
                    } else {
                        lives--;
                        moleActive = 0;
                    }
                }

                if (moleActive) {
                    moleTicks--;
                    if (moleTicks <= 0) {
                        lives--;
                        moleActive = 0;
                    }
                }

                if (score >= TARGET_SCORE) {
                    if (score > best) best = score;
                    state = STATE_WIN;
                } else if (lives <= 0) {
                    if (score > best) best = score;
                    state = STATE_LOSE;
                } else if (!moleActive) {
                    spawnMole();
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
        printf("\x1b[0;0HREFLEX DIG\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Whack the mole before time runs out.\n");
                printf("\x1b[4;0H  Score %d hits before %d misses.\n", TARGET_SCORE, START_LIVES);
                printf("\x1b[6;0H  D-Pad/Circle Pad = move cursor\n");
                printf("\x1b[7;0H  A = dig (whack the lit hole)\n");
                printf("\x1b[9;0H  Wrong hole or a timeout costs\n");
                printf("\x1b[10;0H  a life. It gets faster as you go.\n");
                if (best > 0) {
                    printf("\x1b[13;0H  Best score: %d\n", best);
                }
                printf("\x1b[16;0H  Press A to start.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_PLAY: {
                for (int y = 0; y < GRID_H; y++) {
                    printf("\x1b[%d;0H  ", 3 + y * 2);
                    for (int x = 0; x < GRID_W; x++) {
                        int isCursor = (x == cursorX && y == cursorY);
                        int isMole = moleActive && (x == moleX && y == moleY);
                        if (isCursor) putchar('[');
                        else putchar(' ');
                        putchar(isMole ? '@' : 'o');
                        if (isCursor) putchar(']');
                        else putchar(' ');
                        putchar(' ');
                    }
                    putchar('\n');
                }

                int barY = 3 + GRID_H * 2 + 1;
                printf("\x1b[%d;0HLives: ", barY);
                for (int i = 0; i < lives; i++) putchar('*');
                for (int i = lives; i < START_LIVES; i++) putchar('.');
                printf("   Score: %d / %d\n", score, TARGET_SCORE);

                if (moleActive) {
                    int barLen = 20;
                    int filled = (moleTicks * barLen) / (moleWindow > 0 ? moleWindow : 1);
                    printf("\x1b[%d;0HTimer: [", barY + 1);
                    for (int i = 0; i < barLen; i++) putchar(i < filled ? '=' : ' ');
                    printf("]\n");
                }
                break;
            }

            case STATE_WIN:
                printf("\x1b[4;0H  ALL MOLES DEALT WITH!\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[10;0H  Press A to return to title.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_LOSE:
                printf("\x1b[4;0H  OUT OF LIVES\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[8;0H  Needed %d, got %d.\n", TARGET_SCORE, score);
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
