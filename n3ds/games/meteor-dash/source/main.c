// Meteor Dash - a top-down dodge-the-falling-obstacles game for 3DS.
//
// Mechanic: the player controls a ship on the bottom row of a text
// grid and slides left/right (Circle Pad or D-Pad) to dodge meteors
// ('*') that fall from the top of the screen. Surviving longer raises
// the fall speed and spawn rate. Touching a meteor ends the run and
// shows the survival score. This is a real-time dodging/reflex game -
// mechanically distinct from simon-sez (memory/pattern recall) and
// block-drop (Tetris-style stacking), and from every GBA/NDS game in
// the arcade (no jumping, no maze, no breakout paddle, no snake, no
// card matching).

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_W   28
#define GRID_H   18
#define MAX_METEORS 64

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_GAMEOVER
} GameState;

typedef struct {
    int active;
    int col;
    int row;      // fixed-point-ish: we advance row every N frames
    int subframe;
} Meteor;

static Meteor meteors[MAX_METEORS];
static int playerCol;
static int score;         // frames survived, shown scaled
static int best;
static int frameCount;
static int spawnTimer;
static int spawnInterval;
static int fallSpeed;     // frames per row-drop

static void resetGame(void) {
    memset(meteors, 0, sizeof(meteors));
    playerCol = GRID_W / 2;
    score = 0;
    frameCount = 0;
    spawnTimer = 0;
    spawnInterval = 40;
    fallSpeed = 10;
}

static void spawnMeteor(void) {
    for (int i = 0; i < MAX_METEORS; i++) {
        if (!meteors[i].active) {
            meteors[i].active = 1;
            meteors[i].col = rand() % GRID_W;
            meteors[i].row = 0;
            meteors[i].subframe = 0;
            return;
        }
    }
}

// Returns 1 if a meteor collided with the player.
static int updateMeteors(void) {
    for (int i = 0; i < MAX_METEORS; i++) {
        if (!meteors[i].active) continue;
        meteors[i].subframe++;
        if (meteors[i].subframe >= fallSpeed) {
            meteors[i].subframe = 0;
            meteors[i].row++;
            if (meteors[i].row >= GRID_H - 1) {
                if (meteors[i].col == playerCol) {
                    return 1;
                }
                meteors[i].active = 0;
            }
        }
    }
    return 0;
}

static void drawFrame(PrintConsole *console) {
    consoleSelect(console);
    consoleClear();

    char grid[GRID_H][GRID_W + 1];
    for (int r = 0; r < GRID_H; r++) {
        memset(grid[r], ' ', GRID_W);
        grid[r][GRID_W] = '\0';
    }

    for (int i = 0; i < MAX_METEORS; i++) {
        if (!meteors[i].active) continue;
        if (meteors[i].row >= 0 && meteors[i].row < GRID_H - 1) {
            grid[meteors[i].row][meteors[i].col] = '*';
        }
    }
    grid[GRID_H - 1][playerCol] = 'A';

    printf("\x1b[0;0H");
    printf("METEOR DASH   score:%4d   best:%4d\n", score / 6, best / 6);
    for (int r = 0; r < GRID_H; r++) {
        printf("%s\n", grid[r]);
    }
    printf("Circle Pad / D-Pad: dodge   START: quit\n");
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
                consoleSelect(&topScreen);
                consoleClear();
                printf("\x1b[0;0H");
                printf("\n\n");
                printf("        METEOR DASH\n\n");
                printf("  Dodge the falling meteors!\n");
                printf("  Move with Circle Pad or D-Pad.\n");
                printf("  Survive as long as you can.\n\n");
                printf("  Press A to start.\n");
                printf("  Press START to quit.\n");
                if (kDown & KEY_A) {
                    resetGame();
                    state = STATE_PLAY;
                }
                break;
            }

            case STATE_PLAY: {
                circlePosition cp;
                hidCircleRead(&cp);

                if ((kHeld & KEY_LEFT) || cp.dx < -30) {
                    if (frameCount % 3 == 0 && playerCol > 0) playerCol--;
                } else if ((kHeld & KEY_RIGHT) || cp.dx > 30) {
                    if (frameCount % 3 == 0 && playerCol < GRID_W - 1) playerCol++;
                }

                spawnTimer++;
                if (spawnTimer >= spawnInterval) {
                    spawnTimer = 0;
                    spawnMeteor();
                }

                if (updateMeteors()) {
                    state = STATE_GAMEOVER;
                    if (score > best) best = score;
                    break;
                }

                score++;
                frameCount++;

                // Ramp difficulty over time.
                if (score % 300 == 0 && spawnInterval > 14) spawnInterval -= 2;
                if (score % 400 == 0 && fallSpeed > 4) fallSpeed -= 1;

                drawFrame(&topScreen);
                break;
            }

            case STATE_GAMEOVER: {
                consoleSelect(&topScreen);
                consoleClear();
                printf("\x1b[0;0H");
                printf("\n\n");
                printf("        GAME OVER\n\n");
                printf("  You survived: %d\n", score / 6);
                printf("  Best run:     %d\n\n", best / 6);
                printf("  Press A to play again.\n");
                printf("  Press START to quit.\n");
                if (kDown & KEY_A) {
                    resetGame();
                    state = STATE_PLAY;
                }
                break;
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
