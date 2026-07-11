// pixel-jumper: single-screen endless jumper for GBA, mode 3 bitmap rendering.
//
// A button: jump (in gameplay) / start game (from title)
// START:    restart from game-over back to title
//
// Physics is fixed-point (8.8) so it runs identically at any clock speed
// without float support pulled in from libgcc.

#include <tonc.h>
#include <stdlib.h>

#define GROUND_Y      120
#define PLAYER_X      30
#define PLAYER_SIZE   8
#define GRAVITY       24     // 8.8 fixed-point units per frame^2
#define JUMP_IMPULSE  -640   // 8.8 fixed-point units per frame

#define OBSTACLE_W    6
#define OBSTACLE_H    14
#define OBSTACLE_SPEED 3
#define MAX_OBSTACLES 4
#define SPAWN_GAP_MIN 60
#define SPAWN_GAP_MAX 110

typedef enum { STATE_TITLE, STATE_PLAY, STATE_GAMEOVER } GameState;

typedef struct {
    int active;
    int x; // pixel space, integer
} Obstacle;

static GameState state;
static int player_y_fp;   // 8.8 fixed point, 0 == GROUND_Y
static int player_vy_fp;  // 8.8 fixed point
static int on_ground;
static Obstacle obstacles[MAX_OBSTACLES];
static int frames_until_spawn;
static u32 score;
static u32 rng_state;

static u32 next_rand(void) {
    // xorshift32 - deterministic, no libc rand() dependency on hardware RNG
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void reset_game(void) {
    player_y_fp = 0;
    player_vy_fp = 0;
    on_ground = 1;
    score = 0;
    frames_until_spawn = SPAWN_GAP_MIN;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        obstacles[i].active = 0;
    }
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

static void draw_title(void) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 40, 60, 80, 12, CLR_WHITE);
    draw_rect(PLAYER_X, GROUND_Y - PLAYER_SIZE, PLAYER_SIZE, PLAYER_SIZE, CLR_RED);
    draw_rect(0, GROUND_Y, SCREEN_WIDTH - 1, 2, CLR_LIME);
}

static void spawn_obstacle(void) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) {
            obstacles[i].active = 1;
            obstacles[i].x = SCREEN_WIDTH;
            return;
        }
    }
}

static int rects_overlap(int ax, int ay, int aw, int ah,
                          int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

static void update_play(void) {
    // input
    if (on_ground && (key_hit(KEY_A))) {
        player_vy_fp = JUMP_IMPULSE;
        on_ground = 0;
    }

    // physics
    player_vy_fp += GRAVITY;
    player_y_fp += player_vy_fp;
    if (player_y_fp >= 0) {
        player_y_fp = 0;
        player_vy_fp = 0;
        on_ground = 1;
    }

    int player_screen_y = GROUND_Y - PLAYER_SIZE + (player_y_fp >> 8);

    // spawn/scroll obstacles
    frames_until_spawn--;
    if (frames_until_spawn <= 0) {
        spawn_obstacle();
        frames_until_spawn = SPAWN_GAP_MIN + (next_rand() % (SPAWN_GAP_MAX - SPAWN_GAP_MIN));
    }

    int player_alive = 1;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        obstacles[i].x -= OBSTACLE_SPEED;
        if (obstacles[i].x + OBSTACLE_W < 0) {
            obstacles[i].active = 0;
            score++;
            continue;
        }
        if (rects_overlap(PLAYER_X, player_screen_y, PLAYER_SIZE, PLAYER_SIZE,
                           obstacles[i].x, GROUND_Y - OBSTACLE_H, OBSTACLE_W, OBSTACLE_H)) {
            player_alive = 0;
        }
    }

    if (!player_alive) {
        state = STATE_GAMEOVER;
        return;
    }

    // draw
    clear_screen(CLR_BLACK);
    draw_rect(0, GROUND_Y, SCREEN_WIDTH - 1, 2, CLR_LIME);
    draw_rect(PLAYER_X, player_screen_y, PLAYER_SIZE, PLAYER_SIZE, CLR_RED);
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        draw_rect(obstacles[i].x, GROUND_Y - OBSTACLE_H, OBSTACLE_W, OBSTACLE_H, CLR_YELLOW);
    }

    // score bar: one white pixel-column per point, capped to screen width
    u32 shown = score < (u32)SCREEN_WIDTH ? score : (u32)SCREEN_WIDTH;
    for (u32 i = 0; i < shown; i++) {
        m3_plot(i, 2, CLR_WHITE);
    }
}

static void draw_gameover(void) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 30, 50, 60, 10, CLR_RED);
    u32 shown = score < (u32)SCREEN_WIDTH ? score : (u32)SCREEN_WIDTH;
    for (u32 i = 0; i < shown; i++) {
        m3_plot(i, 80, CLR_WHITE);
    }
}

int main(void) {
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    rng_state = 0xC0FFEE01;
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
                    draw_gameover();
                }
                break;

            case STATE_GAMEOVER:
                if (key_hit(KEY_START)) {
                    state = STATE_TITLE;
                    draw_title();
                }
                break;
        }
    }

    return 0;
}
