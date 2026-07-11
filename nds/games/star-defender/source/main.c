// Star Defender - a top-down projectile shooter for the Nintendo DS
//
// Mechanic: the player's ship sits at the bottom of the top screen and
// slides left/right along a fixed row. Enemies spawn at the top and
// drift downward at varying speed/column; the player fires shots
// upward with B to destroy them before they reach the bottom row and
// cost a life. Score climbs per kill and enemy spawn rate ramps up
// over time. Three lives; lose them all and it's game over. START
// restarts from the title screen at any time.
//
// Rendered entirely with libnds' text console (no tile/sprite engine
// needed), matching the console-only approach already proven in
// maze-muncher - keeps this a genuinely different mechanic (shooter,
// vs. chase/snake/cards/breakout) while staying a complete, solid game.

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FIELD_W 30
#define FIELD_H 20
#define PLAYER_ROW (FIELD_H - 1)
#define MAX_SHOTS 8
#define MAX_ENEMIES 8
#define MAX_LIVES 3

typedef enum { STATE_TITLE, STATE_PLAYING, STATE_GAMEOVER } GameState;

typedef struct {
    bool active;
    int x, y;
} Shot;

typedef struct {
    bool active;
    int x, y;
    int fallCounter;
    int fallRate; // frames per row-drop; lower is faster
} Enemy;

static int playerX;
static int lives;
static int score;
static int frameCount;
static int spawnCounter;
static int spawnInterval;
static Shot shots[MAX_SHOTS];
static Enemy enemies[MAX_ENEMIES];

static void resetGame(void) {
    playerX = FIELD_W / 2;
    lives = MAX_LIVES;
    score = 0;
    frameCount = 0;
    spawnCounter = 0;
    spawnInterval = 60; // ramps down (faster spawns) as score climbs
    memset(shots, 0, sizeof(shots));
    memset(enemies, 0, sizeof(enemies));
}

static void fireShot(void) {
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].active) {
            shots[i].active = true;
            shots[i].x = playerX;
            shots[i].y = PLAYER_ROW - 1;
            return;
        }
    }
}

static void spawnEnemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].active = true;
            enemies[i].x = rand() % FIELD_W;
            enemies[i].y = 0;
            enemies[i].fallCounter = 0;
            // faster fall as score climbs, floor at 6 frames/row
            int rate = 20 - (score / 50);
            if (rate < 6) rate = 6;
            enemies[i].fallRate = rate;
            return;
        }
    }
}

static void drawField(void) {
    char grid[FIELD_H][FIELD_W + 1];
    for (int y = 0; y < FIELD_H; y++) {
        memset(grid[y], ' ', FIELD_W);
        grid[y][FIELD_W] = '\0';
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].y >= 0 && enemies[i].y < FIELD_H)
            grid[enemies[i].y][enemies[i].x] = 'V';
    }
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (shots[i].active && shots[i].y >= 0 && shots[i].y < FIELD_H)
            grid[shots[i].y][shots[i].x] = '|';
    }
    grid[PLAYER_ROW][playerX] = 'A';

    consoleClear();
    for (int y = 0; y < FIELD_H; y++)
        iprintf("%s\n", grid[y]);
    iprintf("Score:%4d   Lives:%d\n", score, lives);
}

static void drawTitle(void) {
    consoleClear();
    iprintf("\n\n");
    iprintf("       STAR DEFENDER\n\n");
    iprintf("  Shoot the invaders (V) before\n");
    iprintf("  they reach the bottom row.\n\n");
    iprintf("   LEFT/RIGHT : move\n");
    iprintf("   B          : fire\n");
    iprintf("   START      : begin / restart\n\n");
    iprintf("        Press START\n");
}

static void drawGameOver(void) {
    consoleClear();
    iprintf("\n\n\n");
    iprintf("        GAME OVER\n\n");
    iprintf("        Final score: %d\n\n", score);
    iprintf("        Press START to try again\n");
}

static void updatePlaying(int down, int held) {
    (void)down;
    if (held & KEY_LEFT && playerX > 0) playerX--;
    else if (held & KEY_RIGHT && playerX < FIELD_W - 1) playerX++;

    if (down & KEY_B) fireShot();

    // advance shots
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].active) continue;
        shots[i].y--;
        if (shots[i].y < 0) shots[i].active = false;
    }

    // advance enemies (each on its own fall cadence)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        enemies[i].fallCounter++;
        if (enemies[i].fallCounter >= enemies[i].fallRate) {
            enemies[i].fallCounter = 0;
            enemies[i].y++;
            if (enemies[i].y >= FIELD_H) {
                enemies[i].active = false;
                lives--;
            }
        }
    }

    // shot/enemy collisions
    for (int s = 0; s < MAX_SHOTS; s++) {
        if (!shots[s].active) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].active) continue;
            if (shots[s].x == enemies[e].x && shots[s].y == enemies[e].y) {
                shots[s].active = false;
                enemies[e].active = false;
                score += 10;
                break;
            }
        }
    }

    // enemy reaching the player's row directly is also a hit
    for (int e = 0; e < MAX_ENEMIES; e++) {
        if (enemies[e].active && enemies[e].y == PLAYER_ROW && enemies[e].x == playerX) {
            enemies[e].active = false;
            lives--;
        }
    }

    // spawn ramp-up
    spawnCounter++;
    if (spawnCounter >= spawnInterval) {
        spawnCounter = 0;
        spawnEnemy();
        spawnInterval = 60 - (score / 20);
        if (spawnInterval < 18) spawnInterval = 18;
    }

    frameCount++;
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
        int held = keysHeld();

        switch (state) {
            case STATE_TITLE:
                if (down & KEY_START) {
                    resetGame();
                    state = STATE_PLAYING;
                    drawField();
                }
                break;

            case STATE_PLAYING:
                updatePlaying(down, held);
                if (lives <= 0) {
                    state = STATE_GAMEOVER;
                    drawGameOver();
                } else {
                    drawField();
                }
                if (down & KEY_START) {
                    state = STATE_TITLE;
                    drawTitle();
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
