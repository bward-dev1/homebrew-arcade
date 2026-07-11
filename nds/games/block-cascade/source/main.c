// Block Cascade - a falling-block stacking puzzle for the DS
//
// Mechanic: classic falling-tetromino gameplay rendered with libnds' text
// console. Pieces made of four blocks fall down a 10-wide well; the player
// slides them left/right, rotates them, and drops them faster, trying to
// fill complete horizontal rows. Completed rows clear and award points;
// the fall speed increases every few line clears. The game ends when a
// new piece cannot spawn because the well is stacked to the top.
//
// This closes out the NDS lineup with a genre none of the other nine
// games use: no chase (maze-muncher), no snake (grid-slither), no
// memory-match (card-flip), no paddle/ball (paddle-bounce), no shooter
// (star-defender), no whack-a-mole (mole-mash), no auto-runner
// (rune-runner), no rhythm bar (beat-gate), no bubble-aim (bubble-burst)
// - this is the arcade's first falling-block stacking puzzle.

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WELL_W 10
#define WELL_H 16
#define NUM_PIECES 7
#define START_FALL_FRAMES 45

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_GAMEOVER } GameState;

// each piece: 4 rotations, each a 4x4 bitmask (16 bits, row-major, bit0 = top-left)
static const u16 PIECES[NUM_PIECES][4] = {
    // I
    { 0x0F00, 0x2222, 0x00F0, 0x4444 },
    // O
    { 0x0660, 0x0660, 0x0660, 0x0660 },
    // T
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 },
    // S
    { 0x06C0, 0x8C40, 0x06C0, 0x8C40 },
    // Z
    { 0x0C60, 0x4C80, 0x0C60, 0x4C80 },
    // J
    { 0x8E00, 0x6440, 0x0E20, 0x44C0 },
    // L
    { 0x2E00, 0x4460, 0x0E80, 0xC440 }
};

static char well[WELL_H][WELL_W]; // 0 = empty, else glyph char

static int curPiece, curRot, curX, curY;
static int nextPiece;
static int score;
static int linesCleared;
static int fallTimer, fallFrames;

static char pieceGlyph(int p) {
    static const char glyphs[NUM_PIECES] = { 'I', 'O', 'T', 'S', 'Z', 'J', 'L' };
    return glyphs[p];
}

static bool cellSet(int piece, int rot, int px, int py) {
    u16 mask = PIECES[piece][rot];
    int bit = py * 4 + px;
    return (mask >> (15 - bit)) & 1;
}

static bool collides(int piece, int rot, int x, int y) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!cellSet(piece, rot, px, py)) continue;
            int wx = x + px;
            int wy = y + py;
            if (wx < 0 || wx >= WELL_W || wy >= WELL_H) return true;
            if (wy >= 0 && well[wy][wx]) return true;
        }
    }
    return false;
}

static void spawnPiece(void) {
    curPiece = nextPiece;
    nextPiece = rand() % NUM_PIECES;
    curRot = 0;
    curX = WELL_W / 2 - 2;
    curY = -1;
}

static void newGame(void) {
    memset(well, 0, sizeof(well));
    score = 0;
    linesCleared = 0;
    fallFrames = START_FALL_FRAMES;
    fallTimer = fallFrames;
    nextPiece = rand() % NUM_PIECES;
    spawnPiece();
}

static void lockPiece(void) {
    char g = pieceGlyph(curPiece);
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!cellSet(curPiece, curRot, px, py)) continue;
            int wx = curX + px;
            int wy = curY + py;
            if (wy >= 0 && wy < WELL_H && wx >= 0 && wx < WELL_W) well[wy][wx] = g;
        }
    }
}

static int clearLines(void) {
    int cleared = 0;
    for (int y = WELL_H - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < WELL_W; x++) {
            if (!well[y][x]) { full = false; break; }
        }
        if (full) {
            cleared++;
            for (int yy = y; yy > 0; yy--) {
                memcpy(well[yy], well[yy - 1], WELL_W);
            }
            memset(well[0], 0, WELL_W);
            y++; // re-check this row index after shifting
        }
    }
    return cleared;
}

static void drawBoard(bool gameOver) {
    consoleClear();
    iprintf("  BLOCK CASCADE   Score:%5d\n", score);
    iprintf(" +----------+\n");

    char frame[WELL_H][WELL_W];
    memcpy(frame, well, sizeof(well));
    if (!gameOver) {
        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                if (!cellSet(curPiece, curRot, px, py)) continue;
                int wx = curX + px;
                int wy = curY + py;
                if (wy >= 0 && wy < WELL_H && wx >= 0 && wx < WELL_W)
                    frame[wy][wx] = pieceGlyph(curPiece);
            }
        }
    }

    for (int y = 0; y < WELL_H; y++) {
        iprintf(" |");
        for (int x = 0; x < WELL_W; x++) {
            char c = frame[y][x];
            iprintf("%c", c ? c : '.');
        }
        iprintf("|\n");
    }
    iprintf(" +----------+\n");
    iprintf(" Next: %c   Lines: %d\n", pieceGlyph(nextPiece), linesCleared);
    iprintf(" L/R move  UP rotate  DOWN drop\n");
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        BLOCK CASCADE\n\n");
    iprintf("   Stack the falling tetrominoes\n");
    iprintf("   and clear full rows.\n\n");
    iprintf("   D-Pad L/R : slide piece\n");
    iprintf("   D-Pad UP  : rotate\n");
    iprintf("   D-Pad DN  : soft drop\n");
    iprintf("   START     : begin / restart\n\n");
    iprintf("   The well fills as you clear\n");
    iprintf("   lines faster and faster. Top\n");
    iprintf("   out and it's game over.\n\n");
    iprintf("        Press START\n");
}

static void drawGameOver(void) {
    iprintf("\n        GAME OVER\n\n");
    iprintf("        Final score: %d\n", score);
    iprintf("        Lines cleared: %d\n\n", linesCleared);
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
        int held = keysHeld();

        switch (state) {
            case STATE_TITLE:
                if (down & KEY_START) {
                    newGame();
                    state = STATE_PLAYING;
                    drawBoard(false);
                }
                break;

            case STATE_PLAYING: {
                bool redraw = false;

                if (down & KEY_LEFT) {
                    if (!collides(curPiece, curRot, curX - 1, curY)) { curX--; redraw = true; }
                }
                if (down & KEY_RIGHT) {
                    if (!collides(curPiece, curRot, curX + 1, curY)) { curX++; redraw = true; }
                }
                if (down & KEY_UP) {
                    int nr = (curRot + 1) % 4;
                    if (!collides(curPiece, nr, curX, curY)) {
                        curRot = nr;
                        redraw = true;
                    } else if (!collides(curPiece, nr, curX - 1, curY)) {
                        curRot = nr; curX--;
                        redraw = true;
                    } else if (!collides(curPiece, nr, curX + 1, curY)) {
                        curRot = nr; curX++;
                        redraw = true;
                    }
                }

                bool fastDrop = held & KEY_DOWN;
                fallTimer -= fastDrop ? 5 : 1;

                if (fallTimer <= 0) {
                    fallTimer = fallFrames;
                    if (!collides(curPiece, curRot, curX, curY + 1)) {
                        curY++;
                    } else {
                        lockPiece();
                        int c = clearLines();
                        if (c > 0) {
                            static const int points[5] = { 0, 100, 300, 500, 800 };
                            score += points[c] ;
                            linesCleared += c;
                            fallFrames = START_FALL_FRAMES - (linesCleared / 5) * 3;
                            if (fallFrames < 10) fallFrames = 10;
                        }
                        spawnPiece();
                        if (collides(curPiece, curRot, curX, curY)) {
                            state = STATE_GAMEOVER;
                            drawBoard(true);
                            drawGameOver();
                            break;
                        }
                    }
                    redraw = true;
                }

                if (state != STATE_PLAYING) break;
                if (redraw) drawBoard(false);
                break;
            }

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
