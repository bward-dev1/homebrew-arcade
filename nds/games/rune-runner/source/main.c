// Rune Runner - an auto-scrolling obstacle-jump runner for the DS
//
// Mechanic: the player stands at a fixed spot on the bottom screen while
// the track scrolls toward them from the right. Rock obstacles ('#') and
// pits (' ') appear in the track; press A to jump over whatever is coming
// - jumping means you clear both a rock and a pit, but you must be back
// on the ground before the next hazard reaches you or you crash into it
// / fall in. Speed ramps up with distance, so timing gets tighter. Score
// is distance traveled. One crash ends the run; START restarts.
//
// This is deliberately a different genre from every other game already
// in the NDS arcade: it is not a chase (maze-muncher), not a snake
// (grid-slither), not a matching puzzle (card-flip), not a paddle/ball
// game (paddle-bounce), not a shooter (star-defender), and not a
// reaction/timing whack game (mole-mash) - it is a side-scrolling
// timed-jump runner, rendered entirely with libnds' text console.

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TRACK_LEN     28      // visible columns of track
#define PLAYER_COL    4       // fixed column where the player stands
#define JUMP_FRAMES   14      // total frames a jump lasts
#define JUMP_PEAK     7       // frame at which the player is airborne-high
#define START_LIVES   1

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_GAMEOVER } GameState;

// track[i] : 0 = clear ground, 1 = rock obstacle, 2 = pit
static int track[TRACK_LEN];

static int scrollSpeed;     // frames per scroll step (lower = faster), *256 fixed point
static int distance;        // score, in track steps traveled
static int jumping;         // frames remaining in current jump, 0 = grounded
static int nextHazardGap;   // columns until next hazard is placed at the spawn edge
static int lives;

static void resetTrack(void) {
    for (int i = 0; i < TRACK_LEN; i++) track[i] = 0;
}

static void newGame(void) {
    resetTrack();
    scrollSpeed = 256; // fixed point: shrinks (speeds up) as distance grows
    distance = 0;
    jumping = 0;
    nextHazardGap = 10 + rand() % 6;
    lives = START_LIVES;
}

static void spawnColumn(void) {
    // shift the whole track left by one, drop the leftmost column
    for (int i = 0; i < TRACK_LEN - 1; i++) track[i] = track[i + 1];

    if (nextHazardGap > 0) {
        track[TRACK_LEN - 1] = 0;
        nextHazardGap--;
    } else {
        track[TRACK_LEN - 1] = (rand() % 2) ? 1 : 2; // rock or pit
        nextHazardGap = 6 + rand() % 7; // guarantee spacing between hazards
    }
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("        RUNE RUNNER\n\n");
    iprintf("   The path scrolls toward you.\n");
    iprintf("   Rocks (#) and pits ( ) will\n");
    iprintf("   wreck your run - press A just\n");
    iprintf("   before they reach you to jump\n");
    iprintf("   clear over them.\n\n");
    iprintf("   A     : jump\n");
    iprintf("   START : begin / restart\n\n");
    iprintf("   Speed climbs the further you\n");
    iprintf("   run. One crash ends it.\n\n");
    iprintf("        Press START\n");
}

static void drawBoard(void) {
    consoleClear();
    iprintf("\n  RUNE RUNNER   Dist: %5d\n\n", distance);

    // air row: show player mid-air near jump peak
    iprintf("  ");
    for (int i = 0; i < TRACK_LEN; i++) {
        if (i == PLAYER_COL && jumping > JUMP_FRAMES / 3) {
            iprintf("O");
        } else {
            iprintf(" ");
        }
    }
    iprintf("\n");

    // ground row: player glyph or track hazards
    iprintf("  ");
    for (int i = 0; i < TRACK_LEN; i++) {
        if (i == PLAYER_COL) {
            if (jumping > 0 && jumping <= JUMP_FRAMES / 3) {
                iprintf("O"); // low part of jump arc, still drawn on ground line
            } else if (jumping > 0) {
                iprintf(track[i] == 2 ? " " : (track[i] == 1 ? "#" : "_"));
            } else {
                iprintf(track[i] == 2 ? "X" : "P"); // X = standing in a pit (dead), P = ok
            }
        } else {
            if (track[i] == 1) iprintf("#");
            else if (track[i] == 2) iprintf(" ");
            else iprintf("_");
        }
    }
    iprintf("\n\n");
    iprintf("  A jump   START restart\n");
}

static void drawGameOver(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        CRASH!\n\n");
    iprintf("        Distance: %d\n\n", distance);
    iprintf("        Press START to run again\n");
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

                if ((down & KEY_A) && jumping == 0) {
                    jumping = JUMP_FRAMES;
                    redraw = true;
                }

                if (jumping > 0) {
                    jumping--;
                    redraw = true;
                }

                static int frame = 0;
                frame++;
                int stepEvery = scrollSpeed / 32; // frames between scroll steps
                if (stepEvery < 3) stepEvery = 3;

                if (frame % stepEvery == 0) {
                    spawnColumn();
                    redraw = true;

                    // collision check: happens the instant the hazard reaches
                    // the player's column
                    int hazard = track[PLAYER_COL];
                    bool airborneEnough = jumping > 0; // any active jump clears
                    if (hazard != 0 && !airborneEnough) {
                        lives--;
                        state = STATE_GAMEOVER;
                        drawGameOver();
                        break;
                    }

                    distance++;
                    if (scrollSpeed > 90) scrollSpeed -= 1; // gradual speed-up
                }

                if (state != STATE_PLAYING) break;
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
