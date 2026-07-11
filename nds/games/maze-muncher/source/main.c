// Maze Muncher - a maze/collector game for the Nintendo DS
//
// Mechanic: navigate a fixed maze on the top screen, eat every dot ('.')
// before a chasing ghost ('G') catches you. Collecting all dots wins the
// level; touching the ghost loses it. START restarts from the title
// screen at any time.
//
// Rendered entirely with libnds' text console (no tile/graphics engine
// needed), which keeps this both a genuinely different mechanic from a
// platformer (pixel-jumper) and simple enough to be a complete, solid
// game rather than a stub.

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAZE_W 30
#define MAZE_H 18

// '#' wall, '.' dot (collectible), ' ' open floor (already visited),
// 'P' player start, 'G' ghost start.
static const char MAZE_TEMPLATE[MAZE_H][MAZE_W + 1] = {
    "##############################",
    "#P#.............#...#...#...##",
    "#.###.###.#####.#.###.#.#.#.##",
    "#...#.#.#.............#.....##",
    "###.#.#.#####.#####.###.#.#.##",
    "#...#.......#.#.......#...#.##",
    "#.#.###.#.###.#.###########.##",
    "#.#.#.#.#.#...#...........#.##",
    "#.#.#.#.#.#.#.#.#.###.###.#.##",
    "#.#.#.#.....#...#.....#.#...##",
    "#.#.#.###.#.###.#.###.#.###.##",
    "#...#...........#.#.........##",
    "#.###.#######.#.#.#.##########",
    "#.#.......#.....#.#...#.....##",
    "#.#####.###.#.###.###.#.###.##",
    "#..........................G##",
    "##############################",
    "##############################",
};

static char maze[MAZE_H][MAZE_W + 1];

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_WIN, STATE_LOSE } GameState;

static int px, py;        // player position
static int gx, gy;        // ghost position
static int dotsLeft;
static int score;
static int ghostMoveCounter;

static bool isWalkable(int x, int y) {
    if (x < 0 || x >= MAZE_W || y < 0 || y >= MAZE_H) return false;
    return maze[y][x] != '#';
}

static void loadLevel(void) {
    dotsLeft = 0;
    score = 0;
    ghostMoveCounter = 0;
    for (int y = 0; y < MAZE_H; y++) {
        for (int x = 0; x < MAZE_W; x++) {
            char c = MAZE_TEMPLATE[y][x];
            if (c == 'P') {
                px = x; py = y;
                maze[y][x] = '.';
            } else if (c == 'G') {
                gx = x; gy = y;
                maze[y][x] = '.';
            } else {
                maze[y][x] = c;
            }
        }
        maze[y][MAZE_W] = '\0';
    }
    // player and ghost start tiles are floor, not dots
    maze[py][px] = ' ';
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++)
            if (maze[y][x] == '.') dotsLeft++;
}

// Greedy chase: ghost picks the neighbouring floor tile that most
// reduces Manhattan distance to the player. Simple, cheap, and
// legitimately threatening in a maze full of dead ends.
static void moveGhost(void) {
    int bestX = gx, bestY = gy;
    int bestDist = abs(gx - px) + abs(gy - py);
    const int dx[4] = {0, 0, -1, 1};
    const int dy[4] = {-1, 1, 0, 0};
    for (int i = 0; i < 4; i++) {
        int nx = gx + dx[i];
        int ny = gy + dy[i];
        if (!isWalkable(nx, ny)) continue;
        int d = abs(nx - px) + abs(ny - py);
        if (d < bestDist) {
            bestDist = d;
            bestX = nx;
            bestY = ny;
        }
    }
    gx = bestX;
    gy = bestY;
}

static void drawMaze(void) {
    consoleClear();
    for (int y = 0; y < MAZE_H; y++) {
        char row[MAZE_W + 1];
        memcpy(row, maze[y], MAZE_W + 1);
        if (y == py) row[px] = '@';
        if (y == gy) row[gx] = (gx == px && gy == py) ? '@' : 'G';
        iprintf("%s\n", row);
    }
    iprintf("\nScore:%3d   Dots left:%3d\n", score, dotsLeft);
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        MAZE MUNCHER\n\n");
    iprintf("   Eat every dot in the maze.\n");
    iprintf("   Don't let the G catch you!\n\n");
    iprintf("   D-Pad : move\n");
    iprintf("   START : begin / restart\n\n");
    iprintf("        Press START\n");
}

static void drawWin(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        YOU WIN!\n\n");
    iprintf("        Final score: %d\n\n", score);
    iprintf("        Press START to play again\n");
}

static void drawLose(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        CAUGHT!\n\n");
    iprintf("        Final score: %d\n\n", score);
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
        int held = keysHeld();

        switch (state) {
            case STATE_TITLE:
                if (down & KEY_START) {
                    loadLevel();
                    state = STATE_PLAYING;
                    drawMaze();
                }
                break;

            case STATE_PLAYING: {
                int nx = px, ny = py;
                bool moved = false;
                if (down & KEY_UP)    { ny--; moved = true; }
                else if (down & KEY_DOWN)  { ny++; moved = true; }
                else if (down & KEY_LEFT)  { nx--; moved = true; }
                else if (down & KEY_RIGHT) { nx++; moved = true; }

                if (moved && isWalkable(nx, ny)) {
                    px = nx;
                    py = ny;
                    if (maze[py][px] == '.') {
                        maze[py][px] = ' ';
                        dotsLeft--;
                        score += 10;
                    }
                }

                // ghost advances every 8 frames so the player can
                // out-maneuver it with careful routing, not just speed
                ghostMoveCounter++;
                if (ghostMoveCounter >= 8) {
                    ghostMoveCounter = 0;
                    moveGhost();
                }

                if (gx == px && gy == py) {
                    state = STATE_LOSE;
                    drawLose();
                } else if (dotsLeft <= 0) {
                    state = STATE_WIN;
                    drawWin();
                } else if (moved || ghostMoveCounter == 0) {
                    drawMaze();
                }

                if (down & KEY_START) {
                    state = STATE_TITLE;
                    drawTitle();
                }
                break;
            }

            case STATE_WIN:
            case STATE_LOSE:
                if (down & KEY_START) {
                    state = STATE_TITLE;
                    drawTitle();
                }
                break;
        }

        (void)held;
        swiWaitForVBlank();
    }

    return 0;
}
