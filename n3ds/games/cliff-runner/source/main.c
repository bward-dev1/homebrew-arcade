// Cliff Runner - a side-scrolling platform jumper for 3DS.
//
// Mechanic: the player auto-runs left-to-right across a fixed-length level
// built from ground, pits, and spike tiles. Press A to jump over hazards;
// clear the level and touch the flag to win. Fall in a pit or land on a
// spike and you lose a life; run out of lives and it's game over. This is
// a finite platformer-with-a-goal, mechanically distinct from every other
// game in this repo: simon-sez (memory pattern), block-drop (falling-block
// puzzle), meteor-dash (dodge/survival), vault-run (ordered-collection
// maze), sky-hopper (vertical climbing jumper - no horizontal level, no
// hazards to jump over), asteroid-field (rotate/thrust shooter), gem-swap
// (tile-swap match-3), reflex-dig (whack-a-mole reaction), brick-smash
// (paddle/ball physics), and the NDS titles (chase, snake, memory-matching
// card-flip, top-down shooter, endless auto-scroll runner with no fixed
// goal or jump-arc physics, timing-bar rhythm gate, bubble shooter,
// falling-block cascade).

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_W 38
#define VIEW_H 18

#define GROUND_ROW (VIEW_H - 4)

#define TILE_GROUND 0
#define TILE_PIT    1
#define TILE_SPIKE  2
#define TILE_FLAG   3

#define LEVEL_LEN 90

#define START_LIVES 3
#define JUMP_FRAMES 12
#define JUMP_HEIGHT 5

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE
} GameState;

static int level[LEVEL_LEN];

static int playerX;      // world column the player currently occupies
static int camX;         // left edge of the visible window in world coords
static int lives;
static int score;
static int best;
static int moveTick;     // frame counter that gates forward movement speed
static int jumping;      // 0 = on ground, >0 = frames remaining in jump
static int jumpFrame;    // counts up while jumping, used for the arc shape
static int hitFlash;     // frames remaining to flash "ouch" after a hazard

static int jumpOffset(void) {
    if (!jumping) return 0;
    // simple parabolic arc peaking at JUMP_HEIGHT in the middle of the jump
    float t = (float)jumpFrame / (float)JUMP_FRAMES;
    float arc = 4.0f * t * (1.0f - t); // 0..1..0
    int off = (int)(arc * JUMP_HEIGHT + 0.5f);
    return off;
}

static void buildLevel(void) {
    for (int i = 0; i < LEVEL_LEN; i++) level[i] = TILE_GROUND;

    // Scatter a repeatable-but-varied set of pits and spikes, leaving
    // landing room after every hazard so the level is always beatable.
    int i = 6;
    while (i < LEVEL_LEN - 6) {
        int roll = rand() % 3;
        if (roll == 0) {
            int w = 1 + (rand() % 2); // pit width 1-2
            for (int k = 0; k < w && i + k < LEVEL_LEN; k++) level[i + k] = TILE_PIT;
            i += w + 3;
        } else if (roll == 1) {
            level[i] = TILE_SPIKE;
            i += 3;
        } else {
            i += 4;
        }
    }
    level[LEVEL_LEN - 1] = TILE_FLAG;
}

static void resetGame(void) {
    buildLevel();
    playerX = 0;
    camX = 0;
    lives = START_LIVES;
    score = 0;
    moveTick = 0;
    jumping = 0;
    jumpFrame = 0;
    hitFlash = 0;
}

static void loseLife(void) {
    lives--;
    hitFlash = 15;
    jumping = 0;
    jumpFrame = 0;
    // knock back a little so a hazard can't be re-triggered instantly
    playerX -= 2;
    if (playerX < 0) playerX = 0;
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
                if (kDown & KEY_A) {
                    resetGame();
                    state = STATE_PLAY;
                }
                break;
            }

            case STATE_PLAY: {
                if (hitFlash > 0) hitFlash--;

                if ((kDown & (KEY_A | KEY_UP)) && !jumping) {
                    jumping = JUMP_FRAMES;
                    jumpFrame = 0;
                }

                if (jumping) {
                    jumpFrame++;
                    jumping--;
                    if (jumping == 0) jumpFrame = 0;
                }

                // forward auto-run, sped up while holding right, briefly
                // paused while holding left (can't reverse past the start)
                moveTick++;
                int speed = (kHeld & (KEY_RIGHT | KEY_CPAD_RIGHT)) ? 4 : 7;
                if (kHeld & (KEY_LEFT | KEY_CPAD_LEFT)) speed = 14;
                if (moveTick >= speed) {
                    moveTick = 0;
                    if (playerX < LEVEL_LEN - 1) {
                        playerX++;
                        score++;
                    }
                }

                int tile = level[playerX];
                int airborne = jumpOffset() > 0;

                if (tile == TILE_PIT && !airborne) {
                    if (lives - 1 <= 0) {
                        lives = 0;
                        if (score > best) best = score;
                        state = STATE_LOSE;
                    } else {
                        loseLife();
                    }
                } else if (tile == TILE_SPIKE && !airborne) {
                    if (lives - 1 <= 0) {
                        lives = 0;
                        if (score > best) best = score;
                        state = STATE_LOSE;
                    } else {
                        loseLife();
                    }
                } else if (tile == TILE_FLAG) {
                    score += 50;
                    if (score > best) best = score;
                    state = STATE_WIN;
                }

                // camera follows the player, clamped to the level bounds
                camX = playerX - VIEW_W / 3;
                if (camX < 0) camX = 0;
                if (camX > LEVEL_LEN - VIEW_W) camX = (LEVEL_LEN > VIEW_W) ? LEVEL_LEN - VIEW_W : 0;
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
        printf("\x1b[0;0HCLIFF RUNNER\n");
        printf("\x1b[1;0H--------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Run right, jump the pits and spikes.\n");
                printf("\x1b[5;0H  A / Up = jump    Right = speed up\n");
                printf("\x1b[6;0H  Reach the flag to win the level.\n");
                printf("\x1b[8;0H  %d lives. A hazard costs one life.\n", START_LIVES);
                if (best > 0) {
                    printf("\x1b[11;0H  Best score: %d\n", best);
                }
                printf("\x1b[14;0H  Press A to start.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_PLAY: {
                int off = jumpOffset();

                for (int row = 0; row < VIEW_H; row++) {
                    printf("\x1b[%d;0H", row + 2);
                    for (int col = 0; col < VIEW_W; col++) {
                        int wx = camX + col;
                        char c = ' ';

                        if (wx >= 0 && wx < LEVEL_LEN) {
                            int t = level[wx];
                            int drawPlayerHere = (wx == playerX) && (row == GROUND_ROW - off);

                            if (drawPlayerHere) {
                                c = hitFlash > 0 ? 'X' : '@';
                            } else if (row == GROUND_ROW) {
                                if (t == TILE_PIT) c = ' ';
                                else if (t == TILE_SPIKE) c = '^';
                                else if (t == TILE_FLAG) c = 'F';
                                else c = '_';
                            } else if (row == GROUND_ROW + 1 && t == TILE_PIT) {
                                c = ' '; // open pit below ground line
                            } else if (row == GROUND_ROW + 1) {
                                c = '#';
                            }
                        }
                        putchar(c);
                    }
                    putchar('\n');
                }

                printf("\x1b[%d;0HLives: ", VIEW_H + 3);
                for (int i = 0; i < lives; i++) putchar('*');
                for (int i = lives; i < START_LIVES; i++) putchar('.');
                printf("   Score: %d   Progress: %d/%d\n", score, playerX, LEVEL_LEN - 1);
                break;
            }

            case STATE_WIN:
                printf("\x1b[4;0H  FLAG REACHED - LEVEL CLEAR!\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[10;0H  Press A to return to title.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_LOSE:
                printf("\x1b[4;0H  OUT OF LIVES\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[8;0H  Got to %d/%d.\n", playerX, LEVEL_LEN - 1);
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
