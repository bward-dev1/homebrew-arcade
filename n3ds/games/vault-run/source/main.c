// Vault Run - a top-down maze vault-cracking game for 3DS.
//
// Mechanic: navigate a fixed maze of corridors and walls, collecting
// numbered tumblers (1, 2, 3, ...) IN ORDER before the timer runs out.
// Picking up a tumbler out of order does nothing (it stays put); picking
// them up in order unlocks the vault door. This is a top-down maze
// navigation + ordered-collection game against a clock, mechanically
// distinct from simon-sez (memory/reaction pattern), block-drop
// (falling-block puzzle), and meteor-dash (dodge/survival) -- and from
// key-quest (GBA maze) and maze-muncher (NDS ghost-chase), since there
// is no enemy AI here and the puzzle is about correct pickup ORDER.

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAZE_W 19
#define MAZE_H 11
#define NUM_TUMBLERS 6
#define START_TIME (60 * 30) // 30 seconds at 60fps

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE
} GameState;

// '#' = wall, '.' = floor. Border is always wall.
static char maze[MAZE_H][MAZE_W + 1] = {
    "###################",
    "#S....#.....#.....#",
    "#.###.#.###.#.###.#",
    "#.#...#...#...#...#",
    "#.#.#####.#.#####.#",
    "#...#.....#.......#",
    "###.#.###.#.#######",
    "#...#.#...#.......#",
    "#.###.#.#####.###.#",
    "#.................#",
    "###################",
};

typedef struct {
    int x, y;
    int order;   // 1..NUM_TUMBLERS
    int taken;
} Tumbler;

static Tumbler tumblers[NUM_TUMBLERS];
static int playerX, playerY;
static int nextNeeded;
static int timer;
static int score;
static int best;

static int isFloor(int x, int y) {
    if (x < 0 || y < 0 || y >= MAZE_H || x >= MAZE_W) return 0;
    return maze[y][x] != '#';
}

static void placeTumblers(void) {
    // Fixed, hand-picked floor tiles spread around the maze so the
    // required order forces real back-and-forth navigation.
    int spots[NUM_TUMBLERS][2] = {
        {4, 1}, {17, 3}, {2, 7}, {13, 5}, {9, 9}, {17, 8}
    };
    for (int i = 0; i < NUM_TUMBLERS; i++) {
        tumblers[i].x = spots[i][0];
        tumblers[i].y = spots[i][1];
        tumblers[i].order = i + 1;
        tumblers[i].taken = 0;
    }
}

static void resetGame(void) {
    playerX = 1;
    playerY = 1;
    nextNeeded = 1;
    timer = START_TIME;
    score = 0;
    placeTumblers();
}

static void tryMove(int dx, int dy) {
    int nx = playerX + dx;
    int ny = playerY + dy;
    if (isFloor(nx, ny)) {
        playerX = nx;
        playerY = ny;
    }
}

static char tileAt(int x, int y) {
    if (x == playerX && y == playerY) return '@';
    for (int i = 0; i < NUM_TUMBLERS; i++) {
        if (!tumblers[i].taken && tumblers[i].x == x && tumblers[i].y == y) {
            return '0' + tumblers[i].order;
        }
    }
    return maze[y][x];
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
                if (kDown & (KEY_UP | KEY_DUP)) tryMove(0, -1);
                else if (kDown & (KEY_DOWN | KEY_DDOWN)) tryMove(0, 1);
                else if (kDown & (KEY_LEFT | KEY_DLEFT)) tryMove(-1, 0);
                else if (kDown & (KEY_RIGHT | KEY_DRIGHT)) tryMove(1, 0);

                for (int i = 0; i < NUM_TUMBLERS; i++) {
                    if (!tumblers[i].taken &&
                        tumblers[i].x == playerX && tumblers[i].y == playerY &&
                        tumblers[i].order == nextNeeded) {
                        tumblers[i].taken = 1;
                        nextNeeded++;
                        score += 100 + (timer / 10);
                    }
                }

                if (nextNeeded > NUM_TUMBLERS) {
                    if (score > best) best = score;
                    state = STATE_WIN;
                    break;
                }

                timer--;
                if (timer <= 0) {
                    if (score > best) best = score;
                    state = STATE_LOSE;
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
        printf("\x1b[0;0HVAULT RUN\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Crack the vault: collect tumblers\n");
                printf("\x1b[4;0H  1-%d IN ORDER before time runs out.\n", NUM_TUMBLERS);
                printf("\x1b[6;0H  D-Pad/Circle Pad = move\n");
                printf("\x1b[7;0H  A = start / continue\n");
                printf("\x1b[9;0H  Grabbing out of order does nothing --\n");
                printf("\x1b[10;0H  come back for it later!\n");
                if (best > 0) {
                    printf("\x1b[13;0H  Best score: %d\n", best);
                }
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_PLAY: {
                for (int y = 0; y < MAZE_H; y++) {
                    printf("\x1b[%d;0H", 3 + y);
                    for (int x = 0; x < MAZE_W; x++) {
                        putchar(tileAt(x, y));
                    }
                }
                printf("\x1b[%d;0HNext needed: %d/%d   Time: %d   Score: %d\n",
                       4 + MAZE_H, nextNeeded, NUM_TUMBLERS,
                       timer / 30, score);
                break;
            }

            case STATE_WIN:
                printf("\x1b[4;0H  VAULT CRACKED!\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[10;0H  Press A to return to title.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_LOSE:
                printf("\x1b[4;0H  TIME'S UP -- VAULT LOCKED\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[8;0H  Got %d of %d tumblers.\n", nextNeeded - 1, NUM_TUMBLERS);
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
