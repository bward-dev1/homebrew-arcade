// Block Drop - a falling-block puzzle (Tetris-like) for 3DS.
//
// Mechanic: tetromino pieces fall down a 10x16 well one row at a time.
// Player moves left/right, rotates, and soft-drops with the D-Pad,
// A rotates, B soft-drops. Completed rows clear and score points;
// speed increases as more lines are cleared. Game ends when a new
// piece can't spawn (well tops out). This is a puzzle/stacking game,
// mechanically distinct from pixel-jumper (platformer), brick-blaster
// (paddle/ball), maze-muncher (maze/chase), grid-slither (snake), and
// simon-sez (memory/reaction).

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOARD_W 10
#define BOARD_H 16

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_GAMEOVER
} GameState;

// Each tetromino is defined as 4 rotations, each a 4x4 bitmask (16 bits,
// row-major, bit (r*4+c) set means filled).
typedef struct {
    u16 rot[4];
    char ch;
} Piece;

static const Piece PIECES[7] = {
    // I
    { { 0x0F00, 0x2222, 0x00F0, 0x4444 }, 'I' },
    // O
    { { 0x0660, 0x0660, 0x0660, 0x0660 }, 'O' },
    // T
    { { 0x0E40, 0x4C40, 0x4E00, 0x4640 }, 'T' },
    // S
    { { 0x06C0, 0x8C40, 0x06C0, 0x8C40 }, 'S' },
    // Z
    { { 0x0C60, 0x4C80, 0x0C60, 0x4C80 }, 'Z' },
    // J
    { { 0x44C0, 0x8E00, 0xC880, 0x0E20 }, 'J' },
    // L
    { { 0x4460, 0x0E80, 0xC440, 0x2E00 }, 'L' },
};

static char board[BOARD_H][BOARD_W];

static int curType;
static int curRot;
static int curX, curY;

static int score;
static int lines;
static int level;
static int best;
static int gravityTimer;
static int softDrop;

static int cellFilled(int type, int rot, int r, int c) {
    u16 mask = PIECES[type].rot[rot];
    int bit = r * 4 + c;
    return (mask >> (15 - bit)) & 1;
}

static int collides(int type, int rot, int x, int y) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!cellFilled(type, rot, r, c)) continue;
            int bx = x + c;
            int by = y + r;
            if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return 1;
            if (by >= 0 && board[by][bx] != 0) return 1;
        }
    }
    return 0;
}

static void spawnPiece(void) {
    curType = rand() % 7;
    curRot = 0;
    curX = (BOARD_W - 4) / 2;
    curY = -1;
}

static void resetGame(void) {
    memset(board, 0, sizeof(board));
    score = 0;
    lines = 0;
    level = 1;
    gravityTimer = 0;
    softDrop = 0;
    spawnPiece();
}

// Frames per gravity step at the current level (speeds up as level rises).
static int gravitySpeed(void) {
    int speed = 40 - (level - 1) * 3;
    if (speed < 8) speed = 8;
    return speed;
}

static void lockPiece(void) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!cellFilled(curType, curRot, r, c)) continue;
            int bx = curX + c;
            int by = curY + r;
            if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
                board[by][bx] = PIECES[curType].ch;
            }
        }
    }

    // Clear completed rows.
    int cleared = 0;
    for (int r = BOARD_H - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < BOARD_W; c++) {
            if (board[r][c] == 0) { full = 0; break; }
        }
        if (full) {
            cleared++;
            for (int rr = r; rr > 0; rr--) {
                memcpy(board[rr], board[rr - 1], BOARD_W);
            }
            memset(board[0], 0, BOARD_W);
            r++; // recheck same row index after shifting down
        }
    }

    if (cleared > 0) {
        static const int lineScore[5] = { 0, 100, 300, 500, 800 };
        score += lineScore[cleared] * level;
        lines += cleared;
        level = 1 + lines / 10;
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
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        switch (state) {
            case STATE_TITLE: {
                if (kDown & (KEY_A | KEY_B)) {
                    resetGame();
                    state = STATE_PLAY;
                }
                break;
            }

            case STATE_PLAY: {
                if (kDown & KEY_LEFT) {
                    if (!collides(curType, curRot, curX - 1, curY)) curX--;
                }
                if (kDown & KEY_RIGHT) {
                    if (!collides(curType, curRot, curX + 1, curY)) curX++;
                }
                if (kDown & KEY_A) {
                    int nrot = (curRot + 1) % 4;
                    if (!collides(curType, nrot, curX, curY)) {
                        curRot = nrot;
                    } else if (!collides(curType, nrot, curX - 1, curY)) {
                        curRot = nrot; curX--;
                    } else if (!collides(curType, nrot, curX + 1, curY)) {
                        curRot = nrot; curX++;
                    }
                }
                softDrop = (kHeld & (KEY_DOWN | KEY_B)) ? 1 : 0;

                gravityTimer++;
                int speed = softDrop ? 3 : gravitySpeed();
                if (gravityTimer >= speed) {
                    gravityTimer = 0;
                    if (!collides(curType, curRot, curX, curY + 1)) {
                        curY++;
                    } else {
                        lockPiece();
                        spawnPiece();
                        if (collides(curType, curRot, curX, curY)) {
                            if (score > best) best = score;
                            state = STATE_GAMEOVER;
                        }
                    }
                }
                break;
            }

            case STATE_GAMEOVER: {
                if (kDown & (KEY_A | KEY_B)) {
                    resetGame();
                    state = STATE_TITLE;
                }
                break;
            }
        }

        // ---- render ----
        consoleSelect(&topScreen);
        consoleClear();
        printf("\x1b[0;0HBLOCK DROP\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Falling-block puzzle.\n");
                printf("\x1b[5;0H  D-Pad Left/Right: move\n");
                printf("\x1b[6;0H  A: rotate   B / Down: soft drop\n");
                printf("\x1b[7;0H  Clear full rows to score.\n");
                printf("\x1b[9;0H  Press A or B to start.\n");
                if (best > 0) {
                    printf("\x1b[12;0H  Best score: %d\n", best);
                }
                break;

            case STATE_PLAY:
            case STATE_GAMEOVER: {
                // Build a render copy of the board including the falling piece.
                char view[BOARD_H][BOARD_W];
                memcpy(view, board, sizeof(view));
                for (int r = 0; r < 4; r++) {
                    for (int c = 0; c < 4; c++) {
                        if (!cellFilled(curType, curRot, r, c)) continue;
                        int bx = curX + c;
                        int by = curY + r;
                        if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
                            view[by][bx] = PIECES[curType].ch;
                        }
                    }
                }

                for (int r = 0; r < BOARD_H; r++) {
                    printf("\x1b[%d;0H|", r + 3);
                    for (int c = 0; c < BOARD_W; c++) {
                        char ch = view[r][c];
                        putchar(ch ? '#' : ' ');
                    }
                    printf("|");
                }
                printf("\x1b[%d;0H+----------+\n", BOARD_H + 3);

                printf("\x1b[3;14HScore: %d\n", score);
                printf("\x1b[4;14HLines: %d\n", lines);
                printf("\x1b[5;14HLevel: %d\n", level);
                if (best > 0) {
                    printf("\x1b[7;14HBest:  %d\n", best);
                }

                if (state == STATE_GAMEOVER) {
                    printf("\x1b[9;14HGAME OVER\n");
                    printf("\x1b[10;14HA/B: restart\n");
                }
                break;
            }
        }

        printf("\x1b[27;0HSTART: quit\n");

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
