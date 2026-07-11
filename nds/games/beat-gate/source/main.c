// Beat Gate - a timing-bar rhythm game for the Nintendo DS
//
// Mechanic: a marker sweeps back and forth along a bar. Press A while the
// marker is inside the highlighted target zone to score a hit; the zone
// shrinks and the sweep speeds up every few rounds. Three misses (marker
// leaves the zone without a press that round, or a press while outside the
// zone) end the run. Clear 15 rounds to win. This is genuinely distinct
// from every other game in the arcade so far: none of them are a pure
// reaction-timing/rhythm-bar game - no maze, no chase, no falling pieces,
// no shooting, no matching pairs, no auto-scrolling jump timing, no
// paddle/ball physics, no whack-a-mole.
//
// Rendered entirely with libnds' text console (no tile/graphics engine
// needed) - same low-risk approach as the rest of the arcade.

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BAR_W 30
#define STARTING_LIVES 3
#define ROUNDS_TO_WIN 15

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_ROUND_RESULT, STATE_WIN, STATE_LOSE } GameState;

static int markerPos16;   // fixed point, 16 sub-units per cell
static int markerSpeed16;
static int markerDir;     // +1 or -1
static int zoneStart, zoneLen;
static int score;
static int lives;
static int round_;
static int streak;
static bool lastHit;
static char resultMsg[32];

static void newRound(void) {
    zoneLen = 6 - (round_ / 4);
    if (zoneLen < 2) zoneLen = 2;
    zoneStart = rand() % (BAR_W - zoneLen);

    markerSpeed16 = 10 + round_ * 2;
    if (markerSpeed16 > 40) markerSpeed16 = 40;

    markerDir = (rand() % 2 == 0) ? 1 : -1;
    markerPos16 = (markerDir > 0) ? 0 : (BAR_W - 1) * 16;
}

static void newGame(void) {
    score = 0;
    lives = STARTING_LIVES;
    streak = 0;
    round_ = 1;
    newRound();
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("          BEAT GATE\n\n");
    iprintf("  A marker sweeps along a bar.\n");
    iprintf("  Press A while it's inside the\n");
    iprintf("  highlighted zone to score.\n\n");
    iprintf("  Miss and you lose a life.\n");
    iprintf("  Clear %d rounds to win!\n\n", ROUNDS_TO_WIN);
    iprintf("  A : hit the beat\n");
    iprintf("  START : begin / restart\n\n");
    iprintf("        Press START\n");
}

static void drawPlaying(void) {
    consoleClear();
    iprintf("\n");
    iprintf("  Round %d / %d\n\n", round_, ROUNDS_TO_WIN);

    char bar[BAR_W + 1];
    memset(bar, '-', BAR_W);
    bar[BAR_W] = '\0';
    for (int i = 0; i < zoneLen; i++) bar[zoneStart + i] = '=';

    int mpos = markerPos16 / 16;
    if (mpos < 0) mpos = 0;
    if (mpos >= BAR_W) mpos = BAR_W - 1;
    bar[mpos] = (bar[mpos] == '=') ? '#' : '*';

    iprintf("  [%s]\n\n", bar);
    iprintf("  Score:%5d   Lives:%d   Streak:%d\n\n", score, lives, streak);
    iprintf("  Press A on the beat!\n");
}

static void drawRoundResult(void) {
    consoleClear();
    iprintf("\n\n\n");
    if (lastHit) {
        iprintf("          HIT!\n\n");
    } else {
        iprintf("          MISS!\n\n");
    }
    iprintf("        %s\n\n", resultMsg);
    iprintf("        Score:%5d  Lives:%d\n\n", score, lives);
    iprintf("        Press START to continue\n");
}

static void drawWin(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("       ALL %d ROUNDS CLEAR!\n\n", ROUNDS_TO_WIN);
    iprintf("        Final score: %d\n\n", score);
    iprintf("        Press START to play again\n");
}

static void drawLose(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("          GAME OVER\n\n");
    iprintf("        Final score: %d\n", score);
    iprintf("        Reached round: %d\n\n", round_);
    iprintf("        Press START to try again\n");
}

static void resolveMiss(const char *why) {
    lives--;
    streak = 0;
    lastHit = false;
    strcpy(resultMsg, why);
}

static void resolveHit(void) {
    int tightness = 8 - zoneLen;
    if (tightness < 1) tightness = 1;
    int bonus = 10 * tightness;
    score += bonus;
    streak++;
    lastHit = true;
    strcpy(resultMsg, "Nice timing!");
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

        switch (state) {
            case STATE_TITLE:
                if (down & KEY_START) {
                    newGame();
                    state = STATE_PLAYING;
                    drawPlaying();
                }
                break;

            case STATE_PLAYING: {
                bool roundOver = false;

                if (down & KEY_A) {
                    int mpos = markerPos16 / 16;
                    if (mpos >= zoneStart && mpos < zoneStart + zoneLen) {
                        resolveHit();
                    } else {
                        resolveMiss("Off the beat.");
                    }
                    roundOver = true;
                }

                if (!roundOver) {
                    markerPos16 += markerDir * markerSpeed16;
                    if (markerPos16 < 0) {
                        markerPos16 = 0;
                        markerDir = 1;
                    } else if (markerPos16 >= BAR_W * 16) {
                        markerPos16 = (BAR_W - 1) * 16;
                        markerDir = -1;
                    }
                    drawPlaying();
                } else {
                    if (lives <= 0) {
                        state = STATE_LOSE;
                        drawLose();
                    } else if (lastHit && round_ >= ROUNDS_TO_WIN) {
                        state = STATE_WIN;
                        drawWin();
                    } else {
                        if (lastHit) round_++;
                        state = STATE_ROUND_RESULT;
                        drawRoundResult();
                    }
                }
                break;
            }

            case STATE_ROUND_RESULT:
                if (down & KEY_START) {
                    if (lives <= 0) {
                        state = STATE_LOSE;
                        drawLose();
                    } else {
                        newRound();
                        state = STATE_PLAYING;
                        drawPlaying();
                    }
                }
                break;

            case STATE_WIN:
                if (down & KEY_START) {
                    state = STATE_TITLE;
                    drawTitle();
                }
                break;

            case STATE_LOSE:
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
