// Gem Swap - a match-3 tile-swap puzzle for 3DS.
//
// Mechanic: an 8x6 grid of gems (A-F). Move a cursor, select a gem, then
// select an orthogonally adjacent gem to swap with it. If the swap creates
// a line of 3+ same gems (row or column) they clear, score goes up,
// columns drop to fill the gap, and new random gems spawn at the top.
// If a swap creates no match it's undone. Limited move count; reach the
// target score before moves run out to win. This is grid tile-swap match
// puzzle, mechanically distinct from every other game in this repo:
// simon-sez (memory/reaction pattern), block-drop (falling-block puzzle),
// meteor-dash (dodge/survival), vault-run (ordered-collection maze),
// sky-hopper (vertical climbing jumper), asteroid-field (rotate/thrust
// shooter), and from the NDS titles (chase, snake, memory-matching
// card-flip, breakout, shooter, whack-a-mole, endless runner).

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_W 8
#define GRID_H 6
#define NUM_KINDS 5
#define START_MOVES 20
#define TARGET_SCORE 800

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE
} GameState;

static char grid[GRID_H][GRID_W];
static int cursorX, cursorY;
static int selX, selY;
static int hasSelection;
static int moves;
static int score;
static int best;

static char gemChar(int kind) {
    static const char kinds[NUM_KINDS] = { 'A', 'B', 'C', 'D', 'E' };
    return kinds[kind % NUM_KINDS];
}

static void fillRandom(void) {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            grid[y][x] = gemChar(rand() % NUM_KINDS);
        }
    }
}

// Returns 1 if any match-of-3+ exists anywhere on the board.
static int hasAnyMatch(void) {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W - 2; x++) {
            if (grid[y][x] && grid[y][x] == grid[y][x + 1] && grid[y][x] == grid[y][x + 2])
                return 1;
        }
    }
    for (int x = 0; x < GRID_W; x++) {
        for (int y = 0; y < GRID_H - 2; y++) {
            if (grid[y][x] && grid[y][x] == grid[y + 1][x] && grid[y][x] == grid[y + 2][x])
                return 1;
        }
    }
    return 0;
}

static void resetGame(void) {
    cursorX = 0;
    cursorY = 0;
    hasSelection = 0;
    moves = START_MOVES;
    score = 0;
    do {
        fillRandom();
    } while (hasAnyMatch()); // start on a calm board
}

// Marks matched cells with 0 in `marked`; returns count cleared.
static int markMatches(char marked[GRID_H][GRID_W]) {
    memset(marked, 0, GRID_H * GRID_W);
    int cleared = 0;

    for (int y = 0; y < GRID_H; y++) {
        int runStart = 0;
        for (int x = 1; x <= GRID_W; x++) {
            int broke = (x == GRID_W) || grid[y][x] != grid[y][runStart];
            if (broke) {
                int len = x - runStart;
                if (len >= 3) {
                    for (int i = runStart; i < x; i++) marked[y][i] = 1;
                }
                runStart = x;
            }
        }
    }
    for (int x = 0; x < GRID_W; x++) {
        int runStart = 0;
        for (int y = 1; y <= GRID_H; y++) {
            int broke = (y == GRID_H) || grid[y][x] != grid[runStart][x];
            if (broke) {
                int len = y - runStart;
                if (len >= 3) {
                    for (int i = runStart; i < y; i++) marked[i][x] = 1;
                }
                runStart = y;
            }
        }
    }

    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            if (marked[y][x]) cleared++;

    return cleared;
}

static void clearAndDrop(char marked[GRID_H][GRID_W]) {
    for (int x = 0; x < GRID_W; x++) {
        int writeY = GRID_H - 1;
        for (int y = GRID_H - 1; y >= 0; y--) {
            if (!marked[y][x]) {
                grid[writeY][x] = grid[y][x];
                writeY--;
            }
        }
        for (int y = writeY; y >= 0; y--) {
            grid[y][x] = gemChar(rand() % NUM_KINDS);
        }
    }
}

// Resolves cascades from the current board state; returns total score gained.
static int resolveBoard(void) {
    int totalScore = 0;
    char marked[GRID_H][GRID_W];
    int cleared;
    int chain = 0;
    while ((cleared = markMatches(marked)) > 0) {
        chain++;
        totalScore += cleared * 10 * chain;
        clearAndDrop(marked);
    }
    return totalScore;
}

static void trySwap(int x1, int y1, int x2, int y2) {
    char tmp = grid[y1][x1];
    grid[y1][x1] = grid[y2][x2];
    grid[y2][x2] = tmp;

    if (hasAnyMatch()) {
        score += resolveBoard();
        moves--;
    } else {
        // undo, no valid match
        tmp = grid[y1][x1];
        grid[y1][x1] = grid[y2][x2];
        grid[y2][x2] = tmp;
    }
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

                if (kDown & KEY_B) {
                    hasSelection = 0;
                }

                if (kDown & KEY_A) {
                    if (!hasSelection) {
                        selX = cursorX;
                        selY = cursorY;
                        hasSelection = 1;
                    } else {
                        int dx = abs(cursorX - selX);
                        int dy = abs(cursorY - selY);
                        if ((dx == 1 && dy == 0) || (dx == 0 && dy == 1)) {
                            trySwap(selX, selY, cursorX, cursorY);
                        }
                        hasSelection = 0;
                    }
                }

                if (score >= TARGET_SCORE) {
                    if (score > best) best = score;
                    state = STATE_WIN;
                } else if (moves <= 0) {
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
        printf("\x1b[0;0HGEM SWAP\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Match 3+ gems in a row or column.\n");
                printf("\x1b[4;0H  Reach %d points in %d moves.\n", TARGET_SCORE, START_MOVES);
                printf("\x1b[6;0H  D-Pad/Circle Pad = move cursor\n");
                printf("\x1b[7;0H  A = select gem, then A on a\n");
                printf("\x1b[8;0H      neighbor to swap\n");
                printf("\x1b[9;0H  B = cancel selection\n");
                printf("\x1b[11;0H  Bad swaps (no match) are undone\n");
                printf("\x1b[12;0H  and don't cost a move.\n");
                if (best > 0) {
                    printf("\x1b[15;0H  Best score: %d\n", best);
                }
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_PLAY: {
                for (int y = 0; y < GRID_H; y++) {
                    printf("\x1b[%d;0H", 3 + y);
                    for (int x = 0; x < GRID_W; x++) {
                        int isCursor = (x == cursorX && y == cursorY);
                        int isSel = hasSelection && (x == selX && y == selY);
                        if (isSel) putchar('[');
                        else if (isCursor) putchar('>');
                        else putchar(' ');
                        putchar(grid[y][x]);
                    }
                    putchar('\n');
                }
                printf("\x1b[%d;0HMoves: %d   Score: %d / %d\n",
                       4 + GRID_H, moves, score, TARGET_SCORE);
                if (hasSelection) {
                    printf("\x1b[%d;0HSelected (%d,%d) -- pick a neighbor.\n",
                           5 + GRID_H, selX, selY);
                }
                break;
            }

            case STATE_WIN:
                printf("\x1b[4;0H  TARGET REACHED!\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[10;0H  Press A to return to title.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_LOSE:
                printf("\x1b[4;0H  OUT OF MOVES\n");
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
