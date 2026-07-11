// brick-blaster: paddle-and-ball brick breaker for GBA, mode 3 bitmap rendering.
//
// LEFT/RIGHT: move paddle
// A / START:  launch ball (from title or when ball is stuck to paddle)
// START:      restart from game-over/win back to title
//
// Physics is fixed-point (8.8) so it runs identically at any clock speed
// without float support pulled in from libgcc.

#include <tonc.h>
#include <stdlib.h>

#define PADDLE_W      24
#define PADDLE_H      4
#define PADDLE_Y      148
#define PADDLE_SPEED  3

#define BALL_SIZE     3
#define BALL_SPEED_FP 384   // 8.8 fixed-point units per frame (1.5 px/frame)

#define BRICK_ROWS    5
#define BRICK_COLS    10
#define BRICK_W       14
#define BRICK_H       6
#define BRICK_GAP     2
#define BRICK_TOP     16
#define BRICK_LEFT    4

#define LIVES_START   3

typedef enum { STATE_TITLE, STATE_PLAY, STATE_GAMEOVER, STATE_WIN } GameState;

static GameState state;

static int paddle_x; // pixel space, integer, left edge

static int ball_x_fp, ball_y_fp;   // 8.8 fixed point, top-left of ball
static int ball_vx_fp, ball_vy_fp; // 8.8 fixed point
static int ball_stuck;             // ball riding on paddle, waiting for launch

static u8 bricks[BRICK_ROWS][BRICK_COLS]; // 1 = alive
static int bricks_left;

static u32 score;
static int lives;
static u32 rng_state;

static const u16 row_colors[BRICK_ROWS] = {
    CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIME, CLR_CYAN
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

static void reset_ball_on_paddle(void) {
    ball_x_fp = (paddle_x + PADDLE_W / 2 - BALL_SIZE / 2) << 8;
    ball_y_fp = (PADDLE_Y - BALL_SIZE) << 8;
    ball_vx_fp = 0;
    ball_vy_fp = 0;
    ball_stuck = 1;
}

static void reset_bricks(void) {
    bricks_left = 0;
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            bricks[r][c] = 1;
            bricks_left++;
        }
    }
}

static void reset_game(void) {
    paddle_x = SCREEN_WIDTH / 2 - PADDLE_W / 2;
    score = 0;
    lives = LIVES_START;
    reset_bricks();
    reset_ball_on_paddle();
}

static void launch_ball(void) {
    ball_stuck = 0;
    ball_vy_fp = -BALL_SPEED_FP;
    // launch angle varies a little so it's not perfectly predictable
    int wobble = (int)(next_rand() % 5) - 2; // -2..2
    ball_vx_fp = wobble * 96;
    if (ball_vx_fp == 0) ball_vx_fp = 96;
}

static int rects_overlap(int ax, int ay, int aw, int ah,
                          int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

static void draw_bricks(void) {
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!bricks[r][c]) continue;
            int bx = BRICK_LEFT + c * (BRICK_W + BRICK_GAP);
            int by = BRICK_TOP + r * (BRICK_H + BRICK_GAP);
            draw_rect(bx, by, BRICK_W, BRICK_H, row_colors[r]);
        }
    }
}

static void draw_hud(void) {
    // score bar: one white pixel-column per point, capped to screen width
    u32 shown = score < (u32)SCREEN_WIDTH ? score : (u32)SCREEN_WIDTH;
    for (u32 i = 0; i < shown; i++) {
        m3_plot(i, 1, CLR_WHITE);
    }
    // lives: small magenta pips top-right
    for (int i = 0; i < lives; i++) {
        draw_rect(SCREEN_WIDTH - 8 - i * 8, 1, 5, 3, CLR_MAG);
    }
}

static void draw_title(void) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 44, 40, 88, 12, CLR_WHITE);
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            int bx = BRICK_LEFT + c * (BRICK_W + BRICK_GAP);
            int by = BRICK_TOP + 60 + r * (BRICK_H + BRICK_GAP);
            draw_rect(bx, by, BRICK_W, BRICK_H, row_colors[r]);
        }
    }
    draw_rect(SCREEN_WIDTH / 2 - PADDLE_W / 2, PADDLE_Y, PADDLE_W, PADDLE_H, CLR_CYAN);
}

static void update_play(void) {
    // input: paddle movement
    if (key_is_down(KEY_LEFT)) {
        paddle_x -= PADDLE_SPEED;
    }
    if (key_is_down(KEY_RIGHT)) {
        paddle_x += PADDLE_SPEED;
    }
    if (paddle_x < 0) paddle_x = 0;
    if (paddle_x > SCREEN_WIDTH - PADDLE_W) paddle_x = SCREEN_WIDTH - PADDLE_W;

    if (ball_stuck) {
        ball_x_fp = (paddle_x + PADDLE_W / 2 - BALL_SIZE / 2) << 8;
        if (key_hit(KEY_A) || key_hit(KEY_START)) {
            launch_ball();
        }
    } else {
        ball_x_fp += ball_vx_fp;
        ball_y_fp += ball_vy_fp;

        int bx = ball_x_fp >> 8;
        int by = ball_y_fp >> 8;

        // wall bounces
        if (bx <= 0) {
            bx = 0;
            ball_x_fp = 0;
            ball_vx_fp = -ball_vx_fp;
        } else if (bx >= SCREEN_WIDTH - BALL_SIZE) {
            bx = SCREEN_WIDTH - BALL_SIZE;
            ball_x_fp = bx << 8;
            ball_vx_fp = -ball_vx_fp;
        }
        if (by <= 8) {
            by = 8;
            ball_y_fp = by << 8;
            ball_vy_fp = -ball_vy_fp;
        }

        // paddle bounce
        if (ball_vy_fp > 0 &&
            rects_overlap(bx, by, BALL_SIZE, BALL_SIZE,
                           paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H)) {
            ball_vy_fp = -ball_vy_fp;
            // add spin based on where it hit the paddle
            int hit_offset = (bx + BALL_SIZE / 2) - (paddle_x + PADDLE_W / 2);
            ball_vx_fp = hit_offset * 24;
            if (ball_vx_fp > 480) ball_vx_fp = 480;
            if (ball_vx_fp < -480) ball_vx_fp = -480;
            by = PADDLE_Y - BALL_SIZE;
            ball_y_fp = by << 8;
        }

        // brick collisions
        for (int r = 0; r < BRICK_ROWS && ball_vy_fp != 0; r++) {
            for (int c = 0; c < BRICK_COLS; c++) {
                if (!bricks[r][c]) continue;
                int brx = BRICK_LEFT + c * (BRICK_W + BRICK_GAP);
                int bry = BRICK_TOP + r * (BRICK_H + BRICK_GAP);
                if (rects_overlap(bx, by, BALL_SIZE, BALL_SIZE,
                                   brx, bry, BRICK_W, BRICK_H)) {
                    bricks[r][c] = 0;
                    bricks_left--;
                    score += 10;
                    ball_vy_fp = -ball_vy_fp;
                    goto brick_hit_done;
                }
            }
        }
        brick_hit_done:;

        // fell off bottom
        if (by > SCREEN_HEIGHT) {
            lives--;
            if (lives <= 0) {
                state = STATE_GAMEOVER;
                return;
            }
            reset_ball_on_paddle();
        }

        if (bricks_left <= 0) {
            state = STATE_WIN;
            return;
        }
    }

    // draw
    clear_screen(CLR_BLACK);
    draw_bricks();
    draw_rect(paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H, CLR_CYAN);
    draw_rect(ball_x_fp >> 8, ball_y_fp >> 8, BALL_SIZE, BALL_SIZE, CLR_WHITE);
    draw_hud();
}

static void draw_end_screen(u16 banner_color) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 30, 50, 60, 10, banner_color);
    draw_hud();
}

int main(void) {
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    rng_state = 0xB01DFACE;
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
