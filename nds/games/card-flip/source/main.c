// Card Flip - a Concentration-style card-matching memory game for the DS
//
// Mechanic: a 4x4 grid of face-down cards is laid out on the top screen.
// The player moves a cursor with the D-Pad and presses A to flip a card.
// Flip a second card and, if the symbols match, both stay face-up and
// score a point; otherwise both flip back down after a short pause.
// Match all 8 pairs to win. START restarts at any time.
//
// This is deliberately a different genre from every other game in the
// arcade: it is not reaction/sequence-recall (simon-sez), not a chasing
// maze (maze-muncher), not a snake (grid-slither), and not falling-block
// puzzle (block-drop) - it is a pure grid memory/matching puzzle, solved
// entirely with libnds' text console (no tile/graphics engine needed).

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GRID_W 4
#define GRID_H 4
#define NUM_CARDS (GRID_W * GRID_H)
#define NUM_PAIRS (NUM_CARDS / 2)

// Symbols used for the 8 pairs. Kept as single printable characters so
// the whole board renders cleanly through iprintf/consoleDemoInit.
static const char SYMBOLS[NUM_PAIRS] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'
};

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_WIN } GameState;

static char cardValue[NUM_CARDS];
static bool cardMatched[NUM_CARDS];
static bool cardFaceUp[NUM_CARDS];

static int cx, cy;              // cursor position on the grid
static int firstPick;           // index of first flipped card this turn, -1 if none
static int secondPick;          // index of second flipped card, -1 if none
static int mismatchTimer;       // frames left before an unmatched pair flips back
static int moves;
static int matches;

static int idx(int x, int y) { return y * GRID_W + x; }

static void shuffleDeck(void) {
    for (int i = 0; i < NUM_PAIRS; i++) {
        cardValue[i * 2]     = SYMBOLS[i];
        cardValue[i * 2 + 1] = SYMBOLS[i];
    }
    // Fisher-Yates shuffle
    for (int i = NUM_CARDS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        char tmp = cardValue[i];
        cardValue[i] = cardValue[j];
        cardValue[j] = tmp;
    }
}

static void newGame(void) {
    shuffleDeck();
    for (int i = 0; i < NUM_CARDS; i++) {
        cardMatched[i] = false;
        cardFaceUp[i] = false;
    }
    cx = 0;
    cy = 0;
    firstPick = -1;
    secondPick = -1;
    mismatchTimer = 0;
    moves = 0;
    matches = 0;
}

static void drawBoard(void) {
    consoleClear();
    iprintf("\n   CARD FLIP\n\n");
    for (int y = 0; y < GRID_H; y++) {
        iprintf("   ");
        for (int x = 0; x < GRID_W; x++) {
            int i = idx(x, y);
            bool selected = (x == cx && y == cy);
            char glyph;
            if (cardMatched[i]) glyph = cardValue[i];
            else if (cardFaceUp[i]) glyph = cardValue[i];
            else glyph = '?';

            if (selected) iprintf("[%c]", glyph);
            else iprintf(" %c ", glyph);
        }
        iprintf("\n\n");
    }
    iprintf("\n   Moves: %3d   Pairs: %d/%d\n", moves, matches, NUM_PAIRS);
    iprintf("\n   D-Pad move  A flip  START restart\n");
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        CARD FLIP\n\n");
    iprintf("   Find all 8 matching pairs.\n\n");
    iprintf("   D-Pad : move cursor\n");
    iprintf("   A     : flip card\n");
    iprintf("   START : begin / restart\n\n");
    iprintf("        Press START\n");
}

static void drawWin(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        ALL MATCHED!\n\n");
    iprintf("        Moves taken: %d\n\n", moves);
    iprintf("        Press START to play again\n");
}

int main(void) {
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleDemoInit(); // sets up main-screen text console by default

    // Seed the shuffle from the DS's own tick counter so each game deals
    // a different layout without needing any external entropy source.
    srand(IPC->random ^ (unsigned)cpuGetTiming());

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

                if (mismatchTimer > 0) {
                    // Hold the mismatched pair face-up briefly so the
                    // player can actually see and memorise them, then
                    // flip both back down and unlock input again.
                    mismatchTimer--;
                    if (mismatchTimer == 0) {
                        cardFaceUp[firstPick] = false;
                        cardFaceUp[secondPick] = false;
                        firstPick = -1;
                        secondPick = -1;
                        redraw = true;
                    }
                } else {
                    if (down & KEY_LEFT  && cx > 0)          { cx--; redraw = true; }
                    if (down & KEY_RIGHT && cx < GRID_W - 1)  { cx++; redraw = true; }
                    if (down & KEY_UP    && cy > 0)          { cy--; redraw = true; }
                    if (down & KEY_DOWN  && cy < GRID_H - 1)  { cy++; redraw = true; }

                    if (down & KEY_A) {
                        int i = idx(cx, cy);
                        if (!cardMatched[i] && !cardFaceUp[i] && secondPick == -1) {
                            cardFaceUp[i] = true;
                            if (firstPick == -1) {
                                firstPick = i;
                            } else {
                                secondPick = i;
                                moves++;
                                if (cardValue[firstPick] == cardValue[secondPick]) {
                                    cardMatched[firstPick] = true;
                                    cardMatched[secondPick] = true;
                                    firstPick = -1;
                                    secondPick = -1;
                                    matches++;
                                } else {
                                    // brief pause (~40 frames) so both
                                    // faces are visible before hiding again
                                    mismatchTimer = 40;
                                }
                            }
                            redraw = true;
                        }
                    }
                }

                if (matches == NUM_PAIRS) {
                    state = STATE_WIN;
                    drawWin();
                } else if (redraw) {
                    drawBoard();
                }

                if (down & KEY_START) {
                    state = STATE_TITLE;
                    drawTitle();
                }
                break;
            }

            case STATE_WIN:
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
