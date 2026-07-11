// Mole Mash - a whack-a-mole reaction game for the DS
//
// Mechanic: a 3x3 grid of holes is shown on the top screen. At random
// intervals a mole pops up in one hole for a short window. The player
// moves a cursor with the D-Pad and presses A to whack whichever hole
// the cursor is over. Whacking the hole while the mole is up scores a
// hit; letting a mole's timer run out (or whacking an empty hole) costs
// a life. The round is timed - survive with score as high as possible
// before the clock or your three lives run out. START restarts.
//
// This is deliberately a different genre from every other game in the
// arcade: it is not a chase (maze-muncher), not a snake (grid-slither),
// not a matching puzzle (card-flip), not a paddle/ball game
// (paddle-bounce), and not a shooter (star-defender) - it is a pure
// reaction-time / target-timing game, rendered entirely with libnds'
// text console (no tile/graphics engine needed).

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GRID_W 3
#define GRID_H 3
#define NUM_HOLES (GRID_W * GRID_H)
#define START_LIVES 3
#define ROUND_FRAMES (60 * 45)   // 45 second round at 60fps

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_GAMEOVER } GameState;

static int cx, cy;
static int moleHole;         // index of hole with an active mole, -1 if none
static int moleTimer;        // frames left before mole disappears on its own
static int spawnTimer;       // frames left before next mole spawns
static int score;
static int lives;
static int roundTimer;       // frames left in the round
static int difficulty;       // increases as score climbs, shortens mole windows

static int idx(int x, int y) { return y * GRID_W + x; }

static void newGame(void) {
    cx = 1;
    cy = 1;
    moleHole = -1;
    moleTimer = 0;
    spawnTimer = 40;
    score = 0;
    lives = START_LIVES;
    roundTimer = ROUND_FRAMES;
    difficulty = 0;
}

static void spawnMole(void) {
    moleHole = rand() % NUM_HOLES;
    int window = 70 - difficulty * 3;
    if (window < 22) window = 22;
    moleTimer = window;
}

static void drawBoard(void) {
    consoleClear();
    iprintf("\n   MOLE MASH\n\n");
    for (int y = 0; y < GRID_H; y++) {
        iprintf("   ");
        for (int x = 0; x < GRID_W; x++) {
            int i = idx(x, y);
            bool selected = (x == cx && y == cy);
            char glyph = (i == moleHole) ? '@' : '.';
            if (selected) iprintf("[%c]", glyph);
            else iprintf(" %c ", glyph);
        }
        iprintf("\n\n");
    }
    iprintf("\n   Score: %3d   Lives: %d\n", score, lives);
    iprintf("   Time: %3d\n", roundTimer / 60);
    iprintf("\n   D-Pad move  A whack  START restart\n");
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        MOLE MASH\n\n");
    iprintf("   Whack the mole before it dives!\n\n");
    iprintf("   D-Pad : move cursor\n");
    iprintf("   A     : whack hole\n");
    iprintf("   START : begin / restart\n\n");
    iprintf("   Miss a mole or whack empty dirt\n");
    iprintf("   and you lose a life. 3 strikes\n");
    iprintf("   and it's over. Survive the clock\n");
    iprintf("   for the highest score you can.\n\n");
    iprintf("        Press START\n");
}

static void drawGameOver(bool timeUp) {
    consoleClear();
    iprintf("\n\n\n");
    if (timeUp) iprintf("        TIME'S UP!\n\n");
    else iprintf("        OUT OF LIVES!\n\n");
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
    bool timeUp = false;
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

                if (down & KEY_LEFT)  { if (cx > 0) cx--; redraw = true; }
                if (down & KEY_RIGHT) { if (cx < GRID_W - 1) cx++; redraw = true; }
                if (down & KEY_UP)    { if (cy > 0) cy--; redraw = true; }
                if (down & KEY_DOWN)  { if (cy < GRID_H - 1) cy++; redraw = true; }

                if (down & KEY_A) {
                    int here = idx(cx, cy);
                    if (moleHole == here) {
                        score++;
                        difficulty = score / 3;
                        moleHole = -1;
                        moleTimer = 0;
                        spawnTimer = 25 + rand() % 40;
                    } else {
                        lives--;
                        if (lives <= 0) {
                            timeUp = false;
                            state = STATE_GAMEOVER;
                            drawGameOver(false);
                            break;
                        }
                    }
                    redraw = true;
                }

                if (state != STATE_PLAYING) break;

                // mole lifecycle
                if (moleHole >= 0) {
                    moleTimer--;
                    if (moleTimer <= 0) {
                        // missed it
                        moleHole = -1;
                        lives--;
                        spawnTimer = 25 + rand() % 40;
                        redraw = true;
                        if (lives <= 0) {
                            state = STATE_GAMEOVER;
                            drawGameOver(false);
                            break;
                        }
                    }
                } else {
                    spawnTimer--;
                    if (spawnTimer <= 0) {
                        spawnMole();
                        redraw = true;
                    }
                }

                roundTimer--;
                if (roundTimer <= 0) {
                    state = STATE_GAMEOVER;
                    drawGameOver(true);
                    break;
                }

                if (redraw) drawBoard();
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
