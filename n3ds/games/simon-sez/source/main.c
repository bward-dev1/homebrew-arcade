// Simon Sez - a button-pattern memory/reaction game for 3DS.
//
// Mechanic: the game flashes a growing sequence of face buttons
// (A / B / X / Y). The player must repeat the sequence back in order.
// Get it right -> the sequence grows by one and speeds up slightly.
// Get it wrong -> game over, showing the level reached (score).
// This is a memory/reaction game, mechanically distinct from
// pixel-jumper (GBA platformer) and maze-muncher (NDS maze/chase game).

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATTERN 64

typedef enum {
    STATE_TITLE,
    STATE_SHOW,
    STATE_INPUT,
    STATE_GAMEOVER
} GameState;

static const char *BTN_NAME[4] = { "A", "B", "X", "Y" };
static const u32   BTN_KEY[4]  = { KEY_A, KEY_B, KEY_X, KEY_Y };

static u8 pattern[MAX_PATTERN];
static int patternLen;
static int showIndex;
static int inputIndex;
static int level;
static int best;
static int flashTimer;
static int flashOn;

static void resetGame(void) {
    patternLen = 0;
    showIndex = 0;
    inputIndex = 0;
    level = 0;
    flashTimer = 0;
    flashOn = 0;
}

static void growPattern(void) {
    pattern[patternLen] = (u8)(rand() % 4);
    patternLen++;
    level = patternLen;
    showIndex = 0;
    flashTimer = 0;
    flashOn = 0;
}

// Frames to hold each flash on/off; sequence speeds up (min-capped) as it grows.
static int flashSpeed(void) {
    int speed = 34 - level; // frames
    if (speed < 14) speed = 14;
    return speed;
}

int main(int argc, char **argv) {
    gfxInitDefault();
    PrintConsole topScreen;
    consoleInit(GFX_TOP, &topScreen);

    // Seed RNG from the hardware tick counter so each run differs.
    srand((unsigned int)svcGetSystemTick());

    GameState state = STATE_TITLE;
    resetGame();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        switch (state) {
            case STATE_TITLE: {
                if (kDown & (KEY_A | KEY_B | KEY_X | KEY_Y)) {
                    resetGame();
                    growPattern();
                    state = STATE_SHOW;
                }
                break;
            }

            case STATE_SHOW: {
                flashTimer++;
                int speed = flashSpeed();
                if (flashTimer >= speed) {
                    flashTimer = 0;
                    if (flashOn) {
                        flashOn = 0;
                        showIndex++;
                        if (showIndex >= patternLen) {
                            inputIndex = 0;
                            state = STATE_INPUT;
                        }
                    } else {
                        flashOn = 1;
                    }
                }
                break;
            }

            case STATE_INPUT: {
                for (int i = 0; i < 4; i++) {
                    if (kDown & BTN_KEY[i]) {
                        if (i == pattern[inputIndex]) {
                            inputIndex++;
                            if (inputIndex >= patternLen) {
                                // Full sequence repeated correctly - advance level.
                                growPattern();
                                state = STATE_SHOW;
                            }
                        } else {
                            if (level > best) best = level;
                            state = STATE_GAMEOVER;
                        }
                        break;
                    }
                }
                break;
            }

            case STATE_GAMEOVER: {
                if (kDown & (KEY_A | KEY_B | KEY_X | KEY_Y)) {
                    resetGame();
                    state = STATE_TITLE;
                }
                break;
            }
        }

        // ---- render ----
        consoleSelect(&topScreen);
        consoleClear();
        printf("\x1b[0;0HSIMON SEZ\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[4;0H  Repeat the button pattern back.\n");
                printf("\x1b[6;0H  Press A, B, X, or Y to start.\n");
                printf("\x1b[8;0H  Sequence grows and speeds up\n");
                printf("\x1b[9;0H  every round you get right.\n");
                if (best > 0) {
                    printf("\x1b[12;0H  Best level: %d\n", best);
                }
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_SHOW: {
                printf("\x1b[4;0H  Watch closely...   Level %d\n", level);
                printf("\x1b[8;0H");
                if (flashOn) {
                    int b = pattern[showIndex];
                    printf("           [ %s ]\n", BTN_NAME[b]);
                } else {
                    printf("\n");
                }
                printf("\x1b[12;0H  Step %d / %d\n", showIndex + 1, patternLen);
                break;
            }

            case STATE_INPUT: {
                printf("\x1b[4;0H  Your turn!         Level %d\n", level);
                printf("\x1b[8;0H  A=%s  B=%s  X=%s  Y=%s\n",
                       "A", "B", "X", "Y");
                printf("\x1b[10;0H  Progress: %d / %d\n", inputIndex, patternLen);
                break;
            }

            case STATE_GAMEOVER:
                printf("\x1b[4;0H  GAME OVER\n");
                printf("\x1b[6;0H  You reached level %d\n", level);
                printf("\x1b[7;0H  Best level: %d\n", best);
                printf("\x1b[10;0H  Press A, B, X, or Y to play again.\n");
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
