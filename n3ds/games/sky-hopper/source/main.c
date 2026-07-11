// Sky Hopper - a vertical climbing platform jumper for 3DS.
//
// Mechanic: a tower of randomly-generated platforms extends upward
// forever. The player moves left/right and presses A to hop up one row
// onto whatever platform is currently under them; the camera scrolls
// upward on a clock that speeds up over time, and if the player's row
// falls too far below the bottom of the visible window they fall off
// the tower and lose. Height climbed is the score. This is a vertical
// climb-under-time-pressure game with an explicit jump button, distinct
// from every other game in the set: not a memory/pattern game
// (simon-sez), not a falling-block puzzle (block-drop), not a
// dodge/survival field (meteor-dash), not an ordered-collection maze
// (vault-run), and not a side-scrolling run/jump against gravity
// (pixel-jumper on GBA, which scrolls horizontally) -- here the tower is
// static and only the camera and player move vertically.

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_W 30
#define VISIBLE_ROWS 16
#define HISTORY 1024

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_LOSE
} GameState;

static short histStart[HISTORY];
static short histWidth[HISTORY];
static char  histValid[HISTORY];

static int playerCol;
static int playerRow;      // world row player stands on; row 0 = ground, decreases upward
static int cameraTop;      // world row shown at the top of the grid
static int lowestGenerated;

static int score;
static int best;
static int tickCount;
static int scrollTick;
static int scrollInterval; // frames between forced camera advance (difficulty)

static int rngRange(int lo, int hi) {
    return lo + (rand() % (hi - lo + 1));
}

static int histIdx(int row) {
    return ((row % HISTORY) + HISTORY) % HISTORY;
}

static void genRow(int row) {
    int idx = histIdx(row);
    int w = rngRange(4, 9);
    int s = rngRange(0, GRID_W - w);
    histStart[idx] = s;
    histWidth[idx] = w;
    histValid[idx] = 1;
}

// Is there solid ground at this world row/col?
static int solidAt(int row, int col) {
    if (row >= 0) return (col >= 0 && col < GRID_W); // ground level and below: solid floor
    int idx = histIdx(row);
    if (!histValid[idx]) return 0;
    return col >= histStart[idx] && col < histStart[idx] + histWidth[idx];
}

static void ensureGenerated(int throughRow) {
    while (lowestGenerated > throughRow) {
        lowestGenerated--;
        genRow(lowestGenerated);
    }
}

static void resetGame(void) {
    playerCol = GRID_W / 2;
    playerRow = 0;
    cameraTop = -(VISIBLE_ROWS - 3);
    score = 0;
    tickCount = 0;
    scrollTick = 0;
    scrollInterval = 45;

    memset(histValid, 0, sizeof(histValid));
    lowestGenerated = 1;
    ensureGenerated(cameraTop - VISIBLE_ROWS);
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
                    if (playerCol > 0) playerCol--;
                } else if (kHeld & (KEY_RIGHT | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                    if (playerCol < GRID_W - 1) playerCol++;
                }

                if (kDown & KEY_A) {
                    // hop up one row if there is a platform beneath the
                    // player's feet at the row above (must land on solid
                    // ground, otherwise the hop fails and you stay put)
                    if (solidAt(playerRow - 1, playerCol)) {
                        playerRow--;
                        int height = -playerRow;
                        if (height > score) score = height;
                    }
                }

                tickCount++;
                if (tickCount % 240 == 0 && scrollInterval > 14) {
                    scrollInterval -= 2;
                }

                scrollTick++;
                if (scrollTick >= scrollInterval) {
                    scrollTick = 0;
                    cameraTop--;
                    ensureGenerated(cameraTop - VISIBLE_ROWS);
                }

                // lose if the player has fallen below the bottom of the
                // visible window (the rising camera left them behind)
                if (playerRow > cameraTop + VISIBLE_ROWS - 1) {
                    if (score > best) best = score;
                    state = STATE_LOSE;
                }

                break;
            }

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
        printf("\x1b[0;0HSKY HOPPER\n");
        printf("\x1b[1;0H------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Climb the tower as high as\n");
                printf("\x1b[4;0H  you can before it outpaces you!\n");
                printf("\x1b[6;0H  Left/Right (D-Pad/Circle Pad)\n");
                printf("\x1b[7;0H  = move along a platform\n");
                printf("\x1b[8;0H  A = hop up to the platform\n");
                printf("\x1b[9;0H       directly above you\n");
                printf("\x1b[11;0H  A = start / continue\n");
                if (best > 0) {
                    printf("\x1b[14;0H  Best height: %d\n", best);
                }
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_PLAY: {
                for (int y = 0; y < VISIBLE_ROWS; y++) {
                    int worldRow = cameraTop + y;
                    printf("\x1b[%d;0H", 3 + y);
                    for (int x = 0; x < GRID_W; x++) {
                        char c = ' ';
                        if (solidAt(worldRow, x)) c = '=';
                        if (worldRow == playerRow && x == playerCol) c = '@';
                        putchar(c);
                    }
                }
                printf("\x1b[%d;0HHeight: %d   Speed: %d\n",
                       4 + VISIBLE_ROWS, score, 60 - scrollInterval);
                break;
            }

            case STATE_LOSE:
                printf("\x1b[4;0H  YOU FELL OFF THE TOWER!\n");
                printf("\x1b[6;0H  Height reached: %d\n", score);
                printf("\x1b[7;0H  Best:           %d\n", best);
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
