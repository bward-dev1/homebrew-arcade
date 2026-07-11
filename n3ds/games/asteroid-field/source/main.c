// Asteroid Field - a rotate-and-thrust asteroids-style shooter for 3DS.
//
// Mechanic: a ship with inertia drifts through open space. Left/Right
// rotate the ship in 8 fixed headings, A applies thrust in the facing
// direction (with drag so momentum matters), and B fires a bullet that
// flies straight until it hits an asteroid or leaves the field. Asteroids
// drift at fixed velocities and wrap around the screen edges; so does the
// ship. Destroy every asteroid to win; run out of lives (3, lost on
// collision) and it's game over. This is mechanically distinct from every
// other 3DS game in the set: simon-sez (memory pattern, no movement),
// block-drop (falling-block puzzle), meteor-dash (dodge/survival, no
// shooting or inertia), vault-run (maze ordered-collection, no combat),
// and sky-hopper (vertical climbing jumper). Rotate+thrust+shoot physics
// against drifting targets has no analog elsewhere in the arcade.

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FIELD_W 40
#define FIELD_H 20
#define MAX_ASTEROIDS 8
#define MAX_BULLETS 6
#define START_LIVES 3
#define BULLET_LIFE 26
#define THRUST 0.06f
#define DRAG 0.985f
#define MAX_SPEED 0.6f

typedef enum {
    STATE_TITLE,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE
} GameState;

// 8 headings, index 0 = up, clockwise.
static const float HEAD_DX[8] = { 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f };
static const float HEAD_DY[8] = { -1.0f, -0.7071f, 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f };
static const char HEAD_CHAR[8] = { '^', '/', '>', '\\', 'v', '\\', '<', '/' };

typedef struct {
    float x, y, vx, vy;
    int alive;
} Asteroid;

typedef struct {
    float x, y, vx, vy;
    int life;
    int active;
} Bullet;

static float shipX, shipY, shipVX, shipVY;
static int shipHeading;
static int lives;
static int score;
static int best;
static Asteroid asteroids[MAX_ASTEROIDS];
static Bullet bullets[MAX_BULLETS];
static int asteroidsAlive;
static int startAsteroids;
static char grid[FIELD_H][FIELD_W + 1];

static float frand(float lo, float hi) {
    return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo);
}

static void wrap(float *x, float *y) {
    if (*x < 0) *x += FIELD_W;
    if (*x >= FIELD_W) *x -= FIELD_W;
    if (*y < 0) *y += FIELD_H;
    if (*y >= FIELD_H) *y -= FIELD_H;
}

static void spawnAsteroids(int count) {
    startAsteroids = count;
    asteroidsAlive = 0;
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (i < count) {
            // Keep asteroids away from the ship's starting spot in the center.
            float x, y;
            do {
                x = frand(0, FIELD_W);
                y = frand(0, FIELD_H);
            } while (fabsf(x - FIELD_W / 2.0f) < 6 && fabsf(y - FIELD_H / 2.0f) < 4);
            asteroids[i].x = x;
            asteroids[i].y = y;
            asteroids[i].vx = frand(-0.15f, 0.15f);
            asteroids[i].vy = frand(-0.15f, 0.15f);
            if (fabsf(asteroids[i].vx) < 0.03f) asteroids[i].vx = 0.05f;
            if (fabsf(asteroids[i].vy) < 0.03f) asteroids[i].vy = 0.05f;
            asteroids[i].alive = 1;
            asteroidsAlive++;
        } else {
            asteroids[i].alive = 0;
        }
    }
}

static void resetGame(void) {
    shipX = FIELD_W / 2.0f;
    shipY = FIELD_H / 2.0f;
    shipVX = shipVY = 0.0f;
    shipHeading = 0;
    lives = START_LIVES;
    score = 0;
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
    spawnAsteroids(5);
}

static void fireBullet(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].x = shipX;
            bullets[i].y = shipY;
            bullets[i].vx = HEAD_DX[shipHeading] * 0.9f;
            bullets[i].vy = HEAD_DY[shipHeading] * 0.9f;
            bullets[i].life = BULLET_LIFE;
            bullets[i].active = 1;
            return;
        }
    }
}

static void placeShipAtSafeSpot(void) {
    shipX = FIELD_W / 2.0f;
    shipY = FIELD_H / 2.0f;
    shipVX = shipVY = 0.0f;
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
                if (kDown & (KEY_LEFT | KEY_DLEFT | KEY_L)) {
                    shipHeading = (shipHeading + 7) % 8;
                }
                if (kDown & (KEY_RIGHT | KEY_DRIGHT | KEY_R)) {
                    shipHeading = (shipHeading + 1) % 8;
                }
                if (kHeld & (KEY_A | KEY_UP | KEY_DUP)) {
                    shipVX += HEAD_DX[shipHeading] * THRUST;
                    shipVY += HEAD_DY[shipHeading] * THRUST;
                    float speed = sqrtf(shipVX * shipVX + shipVY * shipVY);
                    if (speed > MAX_SPEED) {
                        shipVX = shipVX / speed * MAX_SPEED;
                        shipVY = shipVY / speed * MAX_SPEED;
                    }
                }
                if (kDown & KEY_B) {
                    fireBullet();
                }

                // Physics update.
                shipVX *= DRAG;
                shipVY *= DRAG;
                shipX += shipVX;
                shipY += shipVY;
                wrap(&shipX, &shipY);

                for (int i = 0; i < MAX_ASTEROIDS; i++) {
                    if (!asteroids[i].alive) continue;
                    asteroids[i].x += asteroids[i].vx;
                    asteroids[i].y += asteroids[i].vy;
                    wrap(&asteroids[i].x, &asteroids[i].y);
                }

                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) continue;
                    bullets[i].x += bullets[i].vx;
                    bullets[i].y += bullets[i].vy;
                    bullets[i].life--;
                    if (bullets[i].life <= 0 ||
                        bullets[i].x < 0 || bullets[i].x >= FIELD_W ||
                        bullets[i].y < 0 || bullets[i].y >= FIELD_H) {
                        bullets[i].active = 0;
                        continue;
                    }
                    for (int j = 0; j < MAX_ASTEROIDS; j++) {
                        if (!asteroids[j].alive) continue;
                        float dx = bullets[i].x - asteroids[j].x;
                        float dy = bullets[i].y - asteroids[j].y;
                        if (dx * dx + dy * dy < 1.4f) {
                            asteroids[j].alive = 0;
                            asteroidsAlive--;
                            bullets[i].active = 0;
                            score += 150;
                            break;
                        }
                    }
                }

                // Ship-asteroid collision.
                for (int j = 0; j < MAX_ASTEROIDS; j++) {
                    if (!asteroids[j].alive) continue;
                    float dx = shipX - asteroids[j].x;
                    float dy = shipY - asteroids[j].y;
                    if (dx * dx + dy * dy < 1.6f) {
                        asteroids[j].alive = 0;
                        asteroidsAlive--;
                        lives--;
                        placeShipAtSafeSpot();
                        if (lives <= 0) {
                            if (score > best) best = score;
                            state = STATE_LOSE;
                        }
                        break;
                    }
                }

                if (state == STATE_PLAY && asteroidsAlive <= 0) {
                    score += 500;
                    if (score > best) best = score;
                    state = STATE_WIN;
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
        printf("\x1b[0;0HASTEROID FIELD\n");
        printf("\x1b[1;0H----------------------------------------\n");

        switch (state) {
            case STATE_TITLE:
                printf("\x1b[3;0H  Rotate, thrust, and shoot your way\n");
                printf("\x1b[4;0H  through a drifting asteroid field.\n");
                printf("\x1b[6;0H  Left/Right = rotate heading\n");
                printf("\x1b[7;0H  A = thrust    B = fire\n");
                printf("\x1b[8;0H  Momentum carries over -- watch your drift!\n");
                printf("\x1b[10;0H  Destroy all asteroids to win.\n");
                printf("\x1b[11;0H  %d lives -- a collision costs one.\n", START_LIVES);
                printf("\x1b[13;0H  A = start\n");
                if (best > 0) {
                    printf("\x1b[15;0H  Best score: %d\n", best);
                }
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_PLAY: {
                for (int y = 0; y < FIELD_H; y++) {
                    memset(grid[y], ' ', FIELD_W);
                    grid[y][FIELD_W] = '\0';
                }
                for (int i = 0; i < MAX_ASTEROIDS; i++) {
                    if (!asteroids[i].alive) continue;
                    int ax = (int)asteroids[i].x, ay = (int)asteroids[i].y;
                    if (ax >= 0 && ax < FIELD_W && ay >= 0 && ay < FIELD_H) grid[ay][ax] = 'O';
                }
                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) continue;
                    int bx = (int)bullets[i].x, by = (int)bullets[i].y;
                    if (bx >= 0 && bx < FIELD_W && by >= 0 && by < FIELD_H) grid[by][bx] = '.';
                }
                int sx = (int)shipX, sy = (int)shipY;
                if (sx >= 0 && sx < FIELD_W && sy >= 0 && sy < FIELD_H) {
                    grid[sy][sx] = HEAD_CHAR[shipHeading];
                }
                for (int y = 0; y < FIELD_H; y++) {
                    printf("\x1b[%d;0H%s\n", 3 + y, grid[y]);
                }
                printf("\x1b[%d;0HLives: %d   Asteroids left: %d/%d   Score: %d\n",
                       4 + FIELD_H, lives, asteroidsAlive, startAsteroids, score);
                break;
            }

            case STATE_WIN:
                printf("\x1b[4;0H  FIELD CLEARED!\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
                printf("\x1b[10;0H  Press A to return to title.\n");
                printf("\x1b[20;0H  START = quit\n");
                break;

            case STATE_LOSE:
                printf("\x1b[4;0H  SHIP DESTROYED\n");
                printf("\x1b[6;0H  Score: %d\n", score);
                printf("\x1b[7;0H  Best:  %d\n", best);
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
