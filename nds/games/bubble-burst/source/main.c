// Bubble Burst - a color-matching bubble shooter for the DS
//
// Mechanic: a grid of colored bubbles sits at the top of the screen. A
// cannon at the bottom holds one loaded bubble color; the player moves
// the cannon left/right with the D-Pad and fires straight up with A.
// The fired bubble flies up its column and lodges in the first empty
// slot below an occupied one (or the top row if the column is empty).
// If it forms a group of 3+ same-colored bubbles (orthogonally
// connected), the whole group pops and score goes up. Any bubbles left
// floating with no path back to the top row also fall and are cleared
// for bonus points. Clear the whole grid to win the round and advance
// to a harder one. If a column stacks bubbles all the way down to the
// cannon row, it's game over. START restarts.
//
// This is deliberately a different genre from every other game in the
// arcade: not a chase, not a snake, not a memory-match, not a paddle
// game, not a top-down shooter, not whack-a-mole, not an auto-scroller,
// not a rhythm game, and not a Tetris/match-3/Asteroids/climbing
// clone either - it is an aim-and-match projectile puzzle, rendered
// entirely with libnds' text console (no tile/graphics engine needed).

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GRID_W 8
#define GRID_H 8
#define NUM_COLORS 4
#define START_LIVES 3

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_ROUNDCLEAR, STATE_GAMEOVER } GameState;

static const char COLOR_GLYPH[NUM_COLORS] = { 'R', 'G', 'B', 'Y' };

static int grid[GRID_H][GRID_W]; // -1 empty, else color index
static int cannonX;
static int loaded;
static int nextColor;
static int score;
static int lives;
static int round_;
static bool visited[GRID_H][GRID_W];

static int cellAt(int x, int y) {
    if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return -1;
    return grid[y][x];
}

static void seedRows(int rows) {
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            grid[y][x] = -1;

    int colors = NUM_COLORS - (round_ > 2 ? 0 : 1);
    if (colors < 2) colors = 2;
    if (colors > NUM_COLORS) colors = NUM_COLORS;

    for (int y = 0; y < rows; y++)
        for (int x = 0; x < GRID_W; x++)
            grid[y][x] = rand() % colors;
}

static void newGame(void) {
    score = 0;
    lives = START_LIVES;
    round_ = 1;
    seedRows(3);
    cannonX = GRID_W / 2;
    loaded = rand() % NUM_COLORS;
    nextColor = rand() % NUM_COLORS;
}

static void newRound(void) {
    round_++;
    int rows = 3 + (round_ - 1);
    if (rows > GRID_H - 2) rows = GRID_H - 2;
    seedRows(rows);
    cannonX = GRID_W / 2;
    loaded = rand() % NUM_COLORS;
    nextColor = rand() % NUM_COLORS;
}

// Flood fill same-color group starting at (x,y). Returns count and
// marks visited[] for the group.
static int floodGroup(int x, int y, int color, bool mark[GRID_H][GRID_W]) {
    if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return 0;
    if (mark[y][x]) return 0;
    if (grid[y][x] != color) return 0;
    mark[y][x] = true;
    int count = 1;
    count += floodGroup(x + 1, y, color, mark);
    count += floodGroup(x - 1, y, color, mark);
    count += floodGroup(x, y + 1, color, mark);
    count += floodGroup(x, y - 1, color, mark);
    return count;
}

static bool isGridEmpty(void) {
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            if (grid[y][x] != -1) return false;
    return true;
}

// Drop any bubble not connected (directly or via a chain) to the top
// row - simulates gravity once support is removed by a pop.
static int dropFloaters(void) {
    memset(visited, 0, sizeof(visited));
    // Mark everything reachable from row 0 as anchored.
    // Simple BFS using a stack array (grid is small).
    int stackX[GRID_W * GRID_H];
    int stackY[GRID_W * GRID_H];
    int sp = 0;
    for (int x = 0; x < GRID_W; x++) {
        if (grid[0][x] != -1 && !visited[0][x]) {
            visited[0][x] = true;
            stackX[sp] = x;
            stackY[sp] = 0;
            sp++;
        }
    }
    while (sp > 0) {
        sp--;
        int cx = stackX[sp];
        int cy = stackY[sp];
        int dx4[4] = {1, -1, 0, 0};
        int dy4[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx4[d];
            int ny = cy + dy4[d];
            if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) continue;
            if (visited[ny][nx]) continue;
            if (grid[ny][nx] == -1) continue;
            visited[ny][nx] = true;
            stackX[sp] = nx;
            stackY[sp] = ny;
            sp++;
        }
    }
    int dropped = 0;
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            if (grid[y][x] != -1 && !visited[y][x]) {
                grid[y][x] = -1;
                dropped++;
            }
    return dropped;
}

// Fire the loaded bubble up column cannonX. It lands in the lowest
// empty cell above any occupied cell (or the top row if the whole
// column is empty).
static void fireBubble(void) {
    int landRow = -1;
    for (int y = 0; y < GRID_H; y++) {
        if (grid[y][cannonX] != -1) {
            landRow = y - 1;
            break;
        }
    }
    if (landRow == -1) landRow = GRID_H - 1; // whole column empty
    if (landRow < 0) {
        // Column already full to the top - stack overflow, lose a life.
        lives--;
        loaded = nextColor;
        nextColor = rand() % NUM_COLORS;
        return;
    }

    grid[landRow][cannonX] = loaded;

    memset(visited, 0, sizeof(visited));
    int groupCount = floodGroup(cannonX, landRow, loaded, visited);
    if (groupCount >= 3) {
        for (int y = 0; y < GRID_H; y++)
            for (int x = 0; x < GRID_W; x++)
                if (visited[y][x]) grid[y][x] = -1;
        score += groupCount * 10;
        int dropped = dropFloaters();
        score += dropped * 15;
    }

    loaded = nextColor;
    nextColor = rand() % NUM_COLORS;
}

static bool columnBlocked(void) {
    // Game over if any column has a bubble in the bottom-most playable
    // row (one row above the cannon), i.e. stack reached the bottom.
    for (int x = 0; x < GRID_W; x++)
        if (grid[GRID_H - 1][x] != -1) return true;
    return false;
}

static void drawBoard(void) {
    consoleClear();
    iprintf("  BUBBLE BURST   Rnd %d  Score %4d\n\n", round_, score);
    for (int y = 0; y < GRID_H; y++) {
        iprintf(" ");
        for (int x = 0; x < GRID_W; x++) {
            int c = grid[y][x];
            iprintf(" %c", c == -1 ? '.' : COLOR_GLYPH[c]);
        }
        iprintf("\n");
    }
    iprintf(" ");
    for (int x = 0; x < GRID_W; x++)
        iprintf("%s", x == cannonX ? " ^" : "  ");
    iprintf("\n");
    iprintf("\n Loaded:[%c]  Next:[%c]  Lives:%d\n", COLOR_GLYPH[loaded], COLOR_GLYPH[nextColor], lives);
    iprintf("\n L/R move  A fire  START restart\n");
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        BUBBLE BURST\n\n");
    iprintf("   Match 3+ same-color bubbles\n");
    iprintf("   to pop them!\n\n");
    iprintf("   D-Pad L/R : move cannon\n");
    iprintf("   A         : fire bubble\n");
    iprintf("   START     : begin / restart\n\n");
    iprintf("   Clear the grid to advance a\n");
    iprintf("   round. Stack to the bottom\n");
    iprintf("   and you lose a life. Three\n");
    iprintf("   strikes and it's over.\n\n");
    iprintf("        Press START\n");
}

static void drawRoundClear(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        ROUND %d CLEAR!\n\n", round_);
    iprintf("        Score: %d\n\n", score);
    iprintf("        Press START for next round\n");
}

static void drawGameOver(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        GAME OVER\n\n");
    iprintf("        Final score: %d\n\n", score);
    iprintf("        Press START to play again\n");
}

int main(void) {
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleDemoInit();

    srand(cpuGetTiming());

    GameState state = STATE_TITLE;
    drawTitle();

    while (1) {
        scanKeys();
        int down = keysDown();

        switch (state) {
            case STATE_TITLE:
                if (down & KEY_START) {
                    newGame();
                    state = STATE_PLAYING;
                    drawBoard();
                }
                break;

            case STATE_PLAYING: {
                bool redraw = false;

                if (down & KEY_LEFT)  { if (cannonX > 0) cannonX--; redraw = true; }
                if (down & KEY_RIGHT) { if (cannonX < GRID_W - 1) cannonX++; redraw = true; }

                if (down & KEY_A) {
                    fireBubble();
                    redraw = true;

                    if (lives <= 0) {
                        state = STATE_GAMEOVER;
                        drawGameOver();
                        break;
                    }
                    if (columnBlocked()) {
                        lives--;
                        if (lives <= 0) {
                            state = STATE_GAMEOVER;
                            drawGameOver();
                            break;
                        }
                        // Clear a bottom-heavy board so play can continue.
                        seedRows(3);
                    }
                    if (isGridEmpty()) {
                        state = STATE_ROUNDCLEAR;
                        drawRoundClear();
                        break;
                    }
                }

                if (redraw) drawBoard();
                break;
            }

            case STATE_ROUNDCLEAR:
                if (down & KEY_START) {
                    newRound();
                    state = STATE_PLAYING;
                    drawBoard();
                }
                break;

            case STATE_GAMEOVER:
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
