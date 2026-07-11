// Siege Forge - an arcade siege-machine builder + launcher for 3DS.
//
// Mechanic: in BUILD phase the player snaps blocks onto a small horizontal
// chassis grid (wheel / beam / cannon-arm / counterweight), then locks the
// design in. In LAUNCH phase a simplified arcade formula (not a real
// physics/joint simulation) turns total mass, launch-force block count and
// wheel count into a parabolic arc + rolling distance toward a target
// fortress rendered in console text, and scores on proximity. Three rounds
// at increasing target distance give real progression. This is an original
// design in the general spirit of siege-contraption-building games, not a
// copy of any specific game's assets, name, or UI.

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GRID_W 8
#define GRID_H 3
#define NUM_ROUNDS 3
#define ANIM_STEPS 24
#define TRACK_W 60

typedef enum {
    STATE_TITLE,
    STATE_BUILD,
    STATE_LAUNCH_ANIM,
    STATE_ROUND_RESULT,
    STATE_GAME_OVER
} GameState;

// Block types. BLOCK_EMPTY must be 0 so a fresh grid is empty.
typedef enum {
    BLOCK_EMPTY = 0,
    BLOCK_WHEEL,
    BLOCK_BEAM,
    BLOCK_CANNON,
    BLOCK_COUNTERWEIGHT,
    BLOCK_COUNT
} BlockType;

static const char BLOCK_GLYPH[BLOCK_COUNT] = { '.', 'o', '=', '^', '#' };
static const char *BLOCK_NAME[BLOCK_COUNT] = {
    "empty", "wheel", "beam", "cannon-arm", "counterweight"
};

static int grid[GRID_H][GRID_W];
static int cursorX, cursorY;
static int selType = BLOCK_WHEEL; // block type A will place next

static int round_num;      // 1..NUM_ROUNDS
static int target_dist[NUM_ROUNDS] = { 40, 70, 105 };
static int totalScore;
static int best;

static int wheelCount, beamCount, cannonCount, cwCount, mass;
static float launchForce;
static int airDistance, rollDistance, finalDistance;
static int animStep;
static char roundMsg[64];
static int roundScore;

static void resetGridForRound(void) {
    memset(grid, 0, sizeof(grid));
    cursorX = 0;
    cursorY = 0;
    selType = BLOCK_WHEEL;
}

static void resetGame(void) {
    round_num = 1;
    totalScore = 0;
    resetGridForRound();
}

static void analyzeDesign(void) {
    wheelCount = beamCount = cannonCount = cwCount = 0;
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            switch (grid[y][x]) {
                case BLOCK_WHEEL: wheelCount++; break;
                case BLOCK_BEAM: beamCount++; break;
                case BLOCK_CANNON: cannonCount++; break;
                case BLOCK_COUNTERWEIGHT: cwCount++; break;
            }
        }
    }

    // Arcade mass/force model -- deliberately simple, no joints/rigid
    // bodies. Beams and counterweights add mass; wheels are nearly free.
    mass = beamCount * 2 + cwCount * 3 + cannonCount * 2 + wheelCount * 1;
    if (mass < 1) mass = 1;

    // Cannon arms provide raw launch force; counterweights amplify the
    // swing (bonus scales with how many cannons they're paired with);
    // excess mass drags the throw down.
    launchForce = (float)(cannonCount * 14) + (float)(cwCount * 9);
    launchForce -= (float)mass * 0.6f;
    if (launchForce < 4.0f) launchForce = 4.0f;

    airDistance = (int)(launchForce * 1.8f);
    rollDistance = wheelCount * 6;
    finalDistance = airDistance + rollDistance;
}

static void tryPlace(void) {
    selType = (selType + 1) % BLOCK_COUNT;
    grid[cursorY][cursorX] = selType;
}

int main(int argc, char **argv) {
    gfxInitDefault();
    PrintConsole topScreen;
    consoleInit(GFX_TOP, &topScreen);
    consoleSelect(&topScreen);

    srand((unsigned int)svcGetSystemTick());

    GameState state = STATE_TITLE;
    resetGame();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START && state != STATE_BUILD) break;

        switch (state) {
            case STATE_TITLE: {
                if (kDown & KEY_A) {
                    resetGame();
                    state = STATE_BUILD;
                }
                break;
            }

            case STATE_BUILD: {
                if (kDown & (KEY_UP | KEY_DUP)) cursorY = (cursorY - 1 + GRID_H) % GRID_H;
                else if (kDown & (KEY_DOWN | KEY_DDOWN)) cursorY = (cursorY + 1) % GRID_H;
                else if (kDown & (KEY_LEFT | KEY_DLEFT)) cursorX = (cursorX - 1 + GRID_W) % GRID_W;
                else if (kDown & (KEY_RIGHT | KEY_DRIGHT)) cursorX = (cursorX + 1) % GRID_W;
                else if (kDown & KEY_A) tryPlace();
                else if (kDown & KEY_B) grid[cursorY][cursorX] = BLOCK_EMPTY;
                else if (kDown & KEY_START) {
                    analyzeDesign();
                    animStep = 0;
                    state = STATE_LAUNCH_ANIM;
                }
                break;
            }

            case STATE_LAUNCH_ANIM: {
                animStep++;
                if (animStep >= ANIM_STEPS) {
                    int diff = abs(finalDistance - target_dist[round_num - 1]);
                    if (diff <= 5) {
                        roundScore = 500 - diff * 20;
                        snprintf(roundMsg, sizeof(roundMsg), "FORTRESS BREACHED!");
                    } else if (finalDistance < target_dist[round_num - 1]) {
                        roundScore = (diff <= 20) ? 150 - diff * 4 : 20;
                        if (roundScore < 10) roundScore = 10;
                        snprintf(roundMsg, sizeof(roundMsg),
                                 "Fell short - try more counterweights");
                    } else {
                        roundScore = (diff <= 20) ? 150 - diff * 4 : 20;
                        if (roundScore < 10) roundScore = 10;
                        snprintf(roundMsg, sizeof(roundMsg),
                                 "Overshot - trim mass or cannons");
                    }
                    if (roundScore < 0) roundScore = 0;
                    totalScore += roundScore;
                    state = STATE_ROUND_RESULT;
                }
                break;
            }

            case STATE_ROUND_RESULT: {
                if (kDown & KEY_A) {
                    if (round_num >= NUM_ROUNDS) {
                        if (totalScore > best) best = totalScore;
                        state = STATE_GAME_OVER;
                    } else {
                        round_num++;
                        resetGridForRound();
                        state = STATE_BUILD;
                    }
                }
                break;
            }

            case STATE_GAME_OVER: {
                if (kDown & KEY_A) {
                    resetGame();
                    state = STATE_TITLE;
                }
                break;
            }
        }

        // ---- render ----
        consoleClear();
        printf("\x1b[0;0HSIEGE FORGE\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Build a crude siege machine, then\n");
                printf("\x1b[4;0H  launch it at the fortress.\n");
                printf("\x1b[6;0H  D-Pad = move cursor   A = cycle block\n");
                printf("\x1b[7;0H  B = clear cell   START = lock design\n");
                printf("\x1b[9;0H  Blocks: wheel(o) beam(=) cannon(^)\n");
                printf("\x1b[10;0H          counterweight(#)\n");
                printf("\x1b[12;0H  %d rounds, escalating distance.\n", NUM_ROUNDS);
                if (best > 0) printf("\x1b[14;0H  Best total score: %d\n", best);
                printf("\x1b[17;0H  A = start\n");
                printf("\x1b[20;0H  START (at title/results) = quit\n");
                break;

            case STATE_BUILD: {
                printf("\x1b[3;0H  Round %d/%d - target %d m\n",
                       round_num, NUM_ROUNDS, target_dist[round_num - 1]);
                printf("\x1b[4;0H  Next placed block: %s\n", BLOCK_NAME[selType]);
                for (int y = 0; y < GRID_H; y++) {
                    printf("\x1b[%d;0H  ", 6 + y);
                    for (int x = 0; x < GRID_W; x++) {
                        if (x == cursorX && y == cursorY) {
                            printf("[%c]", BLOCK_GLYPH[grid[y][x]]);
                        } else {
                            printf(" %c ", BLOCK_GLYPH[grid[y][x]]);
                        }
                    }
                }
                printf("\x1b[11;0H  A=cycle block  B=clear  START=launch\n");
                printf("\x1b[13;0H  wheel(o)=rolls  beam(=)=mass\n");
                printf("\x1b[14;0H  cannon(^)=force  counterweight(#)=swing force\n");
                break;
            }

            case STATE_LAUNCH_ANIM: {
                printf("\x1b[3;0H  Round %d/%d - target %d m\n",
                       round_num, NUM_ROUNDS, target_dist[round_num - 1]);
                printf("\x1b[4;0H  mass=%d  cannons=%d  counterweights=%d  wheels=%d\n",
                       mass, cannonCount, cwCount, wheelCount);
                printf("\x1b[5;0H  force=%.1f\n", (double)launchForce);

                int pos = (int)(((float)animStep / (float)ANIM_STEPS) * finalDistance);
                if (pos > TRACK_W) pos = TRACK_W;
                printf("\x1b[7;0H  |");
                for (int i = 0; i < TRACK_W; i++) putchar('-');
                printf("|\n");

                int targetCol = target_dist[round_num - 1];
                if (targetCol > TRACK_W) targetCol = TRACK_W;
                printf("\x1b[8;0H  ");
                for (int i = 0; i < TRACK_W; i++) putchar(i == targetCol ? 'F' : ' ');
                printf("\n");

                printf("\x1b[9;0H  ");
                for (int i = 0; i < TRACK_W; i++) {
                    if (i == pos) putchar('*');
                    else if (i == targetCol) putchar('F');
                    else putchar(' ');
                }
                printf("\n");
                printf("\x1b[11;0H  distance so far: %d m (target %d m)\n",
                       pos, target_dist[round_num - 1]);
                break;
            }

            case STATE_ROUND_RESULT:
                printf("\x1b[3;0H  Round %d/%d result\n", round_num, NUM_ROUNDS);
                printf("\x1b[5;0H  Traveled: %d m (air %d + roll %d)\n",
                       finalDistance, airDistance, rollDistance);
                printf("\x1b[6;0H  Target:   %d m\n", target_dist[round_num - 1]);
                printf("\x1b[8;0H  %s\n", roundMsg);
                printf("\x1b[9;0H  Round score: %d\n", roundScore);
                printf("\x1b[10;0H  Total score: %d\n", totalScore);
                printf("\x1b[13;0H  A = %s\n",
                       round_num >= NUM_ROUNDS ? "final results" : "next round");
                break;

            case STATE_GAME_OVER:
                printf("\x1b[3;0H  CAMPAIGN COMPLETE\n");
                printf("\x1b[5;0H  Final score: %d\n", totalScore);
                printf("\x1b[6;0H  Best ever:   %d\n", best);
                printf("\x1b[9;0H  A = play again\n");
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
