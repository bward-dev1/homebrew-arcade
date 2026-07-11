// star-siege: top-down space shooter for GBA, mode 3 bitmap rendering.
//
// LEFT/RIGHT: move ship
// A:          fire (rate limited)
// START:      begin from title / restart from game-over/win
//
// Waves of enemies march down the screen and step sideways at the
// edges (Space-Invaders style). Shoot them all to clear a wave; each
// wave is faster and denser than the last. Get hit by an enemy or by
// an enemy that reaches the bottom and you lose a life.
//
// Physics/movement is integer pixel-space; fire rate and enemy step
// timing are frame-counted so behavior is identical at any clock speed.

#include <tonc.h>
#include <stdlib.h>

#define SHIP_W        10
#define SHIP_H        6
#define SHIP_Y        150
#define SHIP_SPEED    2

#define BULLET_W      1
#define BULLET_H      4
#define BULLET_SPEED  4
#define BULLET_COOLDOWN 12   // frames between shots
#define MAX_BULLETS   4

#define ENEMY_ROWS    4
#define ENEMY_COLS    8
#define ENEMY_W       10
#define ENEMY_H       6
#define ENEMY_GAP_X   4
#define ENEMY_GAP_Y   6
#define ENEMY_TOP     16
#define ENEMY_LEFT    4
#define ENEMY_STEP_Y  6

#define ENEMY_BULLET_W 1
#define ENEMY_BULLET_H 4
#define ENEMY_BULLET_SPEED 2
#define MAX_ENEMY_BULLETS 3

#define LIVES_START   3

typedef enum { STATE_TITLE, STATE_PLAY, STATE_GAMEOVER, STATE_WIN } GameState;

static GameState state;

static int ship_x; // left edge, pixel space

static int bullet_active[MAX_BULLETS];
static int bullet_x[MAX_BULLETS];
static int bullet_y[MAX_BULLETS];
static int fire_cooldown;

static int enemy_bullet_active[MAX_ENEMY_BULLETS];
static int enemy_bullet_x[MAX_ENEMY_BULLETS];
static int enemy_bullet_y[MAX_ENEMY_BULLETS];
static int enemy_fire_cooldown;

static u8 enemies[ENEMY_ROWS][ENEMY_COLS]; // 1 = alive
static int enemies_left;
static int enemy_origin_x; // top-left of formation
static int enemy_origin_y;
static int enemy_dir;      // +1 or -1
static int enemy_step_timer;
static int enemy_step_interval; // frames per formation shuffle, shrinks with wave

static u32 score;
static int lives;
static int wave;
static u32 rng_state;

static const u16 row_colors[ENEMY_ROWS] = {
    CLR_RED, CLR_ORANGE, CLR_MAG, CLR_CYAN
};

static u32 next_rand(void) {
    // xorshift32 - deterministic, no libc rand() dependency on hardware RNG
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void draw_rect(int x, int y, int w, int h, u16 color) {
    for (int row = 0; row < h; row++) {
        m3_hline(x, y + row, x + w - 1, color);
    }
}

static void clear_screen(u16 color) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        m3_hline(0, y, SCREEN_WIDTH - 1, color);
    }
}

static int rects_overlap(int ax, int ay, int aw, int ah,
                          int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

static int formation_width(void) {
    return ENEMY_COLS * (ENEMY_W + ENEMY_GAP_X) - ENEMY_GAP_X;
}

static void reset_enemies(void) {
    enemies_left = 0;
    for (int r = 0; r < ENEMY_ROWS; r++) {
        for (int c = 0; c < ENEMY_COLS; c++) {
            enemies[r][c] = 1;
            enemies_left++;
        }
    }
    enemy_origin_x = ENEMY_LEFT;
    enemy_origin_y = ENEMY_TOP;
    enemy_dir = 1;
    enemy_step_timer = 0;
    enemy_step_interval = 40 - wave * 3;
    if (enemy_step_interval < 10) enemy_step_interval = 10;
}

static void reset_bullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) bullet_active[i] = 0;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) enemy_bullet_active[i] = 0;
    fire_cooldown = 0;
    enemy_fire_cooldown = 30;
}

static void reset_game(void) {
    ship_x = SCREEN_WIDTH / 2 - SHIP_W / 2;
    score = 0;
    lives = LIVES_START;
    wave = 0;
    reset_enemies();
    reset_bullets();
}

static void start_next_wave(void) {
    wave++;
    reset_enemies();
    reset_bullets();
}

static void draw_ship(int x, int y, u16 color) {
    // simple arrow-ish ship: body plus a nose bump
    draw_rect(x, y + 2, SHIP_W, SHIP_H - 2, color);
    draw_rect(x + SHIP_W / 2 - 1, y, 2, 3, color);
}

static void draw_enemies(void) {
    for (int r = 0; r < ENEMY_ROWS; r++) {
        for (int c = 0; c < ENEMY_COLS; c++) {
            if (!enemies[r][c]) continue;
            int ex = enemy_origin_x + c * (ENEMY_W + ENEMY_GAP_X);
            int ey = enemy_origin_y + r * (ENEMY_H + ENEMY_GAP_Y);
            draw_rect(ex, ey, ENEMY_W, ENEMY_H, row_colors[r]);
        }
    }
}

static void draw_bullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullet_active[i]) continue;
        draw_rect(bullet_x[i], bullet_y[i], BULLET_W, BULLET_H, CLR_WHITE);
    }
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!enemy_bullet_active[i]) continue;
        draw_rect(enemy_bullet_x[i], enemy_bullet_y[i], ENEMY_BULLET_W, ENEMY_BULLET_H, CLR_YELLOW);
    }
}

static void draw_hud(void) {
    // score bar: one white pixel-column per 2 points, capped to screen width
    u32 shown = (score / 2) < (u32)SCREEN_WIDTH ? (score / 2) : (u32)SCREEN_WIDTH;
    for (u32 i = 0; i < shown; i++) {
        m3_plot(i, 1, CLR_WHITE);
    }
    // lives: small green pips top-right
    for (int i = 0; i < lives; i++) {
        draw_rect(SCREEN_WIDTH - 8 - i * 8, 1, 5, 3, CLR_LIME);
    }
}

static void draw_title(void) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 44, 34, 88, 12, CLR_WHITE);
    for (int r = 0; r < ENEMY_ROWS; r++) {
        for (int c = 0; c < ENEMY_COLS; c++) {
            int ex = ENEMY_LEFT + c * (ENEMY_W + ENEMY_GAP_X);
            int ey = 56 + r * (ENEMY_H + ENEMY_GAP_Y);
            draw_rect(ex, ey, ENEMY_W, ENEMY_H, row_colors[r]);
        }
    }
    draw_ship(SCREEN_WIDTH / 2 - SHIP_W / 2, SHIP_Y, CLR_CYAN);
}

static void fire_player_bullet(void) {
    if (fire_cooldown > 0) return;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullet_active[i]) {
            bullet_active[i] = 1;
            bullet_x[i] = ship_x + SHIP_W / 2 - BULLET_W / 2;
            bullet_y[i] = SHIP_Y - BULLET_H;
            fire_cooldown = BULLET_COOLDOWN;
            return;
        }
    }
}

static void maybe_fire_enemy_bullet(void) {
    if (enemy_fire_cooldown > 0) return;
    if (enemies_left <= 0) return;

    // pick a random alive enemy to fire from
    int tries = 12;
    while (tries-- > 0) {
        int r = next_rand() % ENEMY_ROWS;
        int c = next_rand() % ENEMY_COLS;
        if (!enemies[r][c]) continue;
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
            if (!enemy_bullet_active[i]) {
                int ex = enemy_origin_x + c * (ENEMY_W + ENEMY_GAP_X);
                int ey = enemy_origin_y + r * (ENEMY_H + ENEMY_GAP_Y);
                enemy_bullet_active[i] = 1;
                enemy_bullet_x[i] = ex + ENEMY_W / 2 - ENEMY_BULLET_W / 2;
                enemy_bullet_y[i] = ey + ENEMY_H;
                enemy_fire_cooldown = 50 - wave * 2;
                if (enemy_fire_cooldown < 18) enemy_fire_cooldown = 18;
                return;
            }
        }
        return;
    }
}

static void step_formation(void) {
    int w = formation_width();
    int would_hit_edge =
        (enemy_dir > 0 && enemy_origin_x + w >= SCREEN_WIDTH - 2) ||
        (enemy_dir < 0 && enemy_origin_x <= 2);

    if (would_hit_edge) {
        enemy_origin_y += ENEMY_STEP_Y;
        enemy_dir = -enemy_dir;
    } else {
        enemy_origin_x += enemy_dir * 3;
    }
}

static void update_play(void) {
    // input: ship movement
    if (key_is_down(KEY_LEFT)) {
        ship_x -= SHIP_SPEED;
    }
    if (key_is_down(KEY_RIGHT)) {
        ship_x += SHIP_SPEED;
    }
    if (ship_x < 0) ship_x = 0;
    if (ship_x > SCREEN_WIDTH - SHIP_W) ship_x = SCREEN_WIDTH - SHIP_W;

    if (fire_cooldown > 0) fire_cooldown--;
    if (enemy_fire_cooldown > 0) enemy_fire_cooldown--;

    if (key_is_down(KEY_A)) {
        fire_player_bullet();
    }

    // move player bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullet_active[i]) continue;
        bullet_y[i] -= BULLET_SPEED;
        if (bullet_y[i] < 0) {
            bullet_active[i] = 0;
        }
    }

    // move enemy bullets
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!enemy_bullet_active[i]) continue;
        enemy_bullet_y[i] += ENEMY_BULLET_SPEED;
        if (enemy_bullet_y[i] > SCREEN_HEIGHT) {
            enemy_bullet_active[i] = 0;
        }
    }

    maybe_fire_enemy_bullet();

    // formation stepping
    enemy_step_timer++;
    if (enemy_step_timer >= enemy_step_interval) {
        enemy_step_timer = 0;
        step_formation();
    }

    // player bullet vs enemy collisions
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullet_active[i]) continue;
        for (int r = 0; r < ENEMY_ROWS; r++) {
            for (int c = 0; c < ENEMY_COLS; c++) {
                if (!enemies[r][c]) continue;
                int ex = enemy_origin_x + c * (ENEMY_W + ENEMY_GAP_X);
                int ey = enemy_origin_y + r * (ENEMY_H + ENEMY_GAP_Y);
                if (rects_overlap(bullet_x[i], bullet_y[i], BULLET_W, BULLET_H,
                                   ex, ey, ENEMY_W, ENEMY_H)) {
                    enemies[r][c] = 0;
                    enemies_left--;
                    score += 10;
                    bullet_active[i] = 0;
                    goto next_bullet;
                }
            }
        }
        next_bullet:;
    }

    // enemy bullet vs ship collision
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!enemy_bullet_active[i]) continue;
        if (rects_overlap(enemy_bullet_x[i], enemy_bullet_y[i], ENEMY_BULLET_W, ENEMY_BULLET_H,
                           ship_x, SHIP_Y, SHIP_W, SHIP_H)) {
            enemy_bullet_active[i] = 0;
            lives--;
            if (lives <= 0) {
                state = STATE_GAMEOVER;
                return;
            }
        }
    }

    // enemies reaching the ship's row / bottom = instant loss of a life and
    // the whole formation resets for the current wave
    for (int r = 0; r < ENEMY_ROWS; r++) {
        for (int c = 0; c < ENEMY_COLS; c++) {
            if (!enemies[r][c]) continue;
            int ey = enemy_origin_y + r * (ENEMY_H + ENEMY_GAP_Y);
            if (ey + ENEMY_H >= SHIP_Y) {
                lives--;
                if (lives <= 0) {
                    state = STATE_GAMEOVER;
                    return;
                }
                reset_enemies();
                reset_bullets();
                goto formation_reset_done;
            }
        }
    }
    formation_reset_done:;

    if (enemies_left <= 0) {
        if (wave >= 5) {
            state = STATE_WIN;
            return;
        }
        start_next_wave();
    }

    // draw
    clear_screen(CLR_BLACK);
    draw_enemies();
    draw_bullets();
    draw_ship(ship_x, SHIP_Y, CLR_CYAN);
    draw_hud();
}

static void draw_end_screen(u16 banner_color) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 30, 50, 60, 10, banner_color);
    draw_hud();
}

int main(void) {
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    rng_state = 0x57A2517E;
    state = STATE_TITLE;
    reset_game();
    draw_title();

    while (1) {
        VBlankIntrWait();
        key_poll();

        switch (state) {
            case STATE_TITLE:
                if (key_hit(KEY_A) || key_hit(KEY_START)) {
                    reset_game();
                    state = STATE_PLAY;
                }
                break;

            case STATE_PLAY:
                update_play();
                if (state == STATE_GAMEOVER) {
                    draw_end_screen(CLR_RED);
                } else if (state == STATE_WIN) {
                    draw_end_screen(CLR_LIME);
                }
                break;

            case STATE_GAMEOVER:
            case STATE_WIN:
                if (key_hit(KEY_START)) {
                    state = STATE_TITLE;
                    draw_title();
                }
                break;
        }
    }

    return 0;
}
