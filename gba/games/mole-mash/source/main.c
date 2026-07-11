// mole-mash: whack-a-mole reaction/timing game for GBA, mode 3 bitmap rendering.
//
// D-PAD: move the cursor between the 9 holes (3x3 grid)
// A:     whack whichever hole the cursor is on
// START: confirm on title/end screens, or restart after game-over/win
//
// A mole pops up in a random hole for a short window. Move the cursor
// onto that hole and press A before it ducks back down to score. Miss
// three moles (they duck away un-whacked) and it's game over; survive
// the clock with at least one life left and you win with your score.
// This is a reaction-timing game against a hidden random target - distinct
// from the platformer/breakout/maze-collect GBA games already in the
// arcade, and from the ghost-chase/snake/matching NDS games and the
// pattern-memory/tetris/dodge 3DS games.

#include <tonc.h>

#define GRID_W        3
#define GRID_H        3
#define NUM_HOLES     (GRID_W * GRID_H)

#define HOLE_W        60
#define HOLE_H        44
#define GRID_LEFT     30
#define GRID_TOP      18

#define MOVE_DELAY    8       // frames between cursor grid steps (debounce)
#define TIME_START    (60 * 45)  // 45 seconds at 60fps
#define START_LIVES   3

#define MOLE_UP_MIN   35      // fastest mole stays up (frames)
#define MOLE_UP_MAX   75      // slowest mole stays up (frames)
#define MOLE_GAP_MIN  20      // min frames between moles
#define MOLE_GAP_MAX  55      // max frames between moles

typedef enum { STATE_TITLE, STATE_PLAY, STATE_GAMEOVER, STATE_WIN } GameState;
typedef enum { MOLE_NONE, MOLE_UP, MOLE_HIT } MoleState;

static GameState state;

static int cursor_x, cursor_y;   // grid coords
static int move_timer;

static MoleState mole_state;
static int mole_hole;            // index 0..NUM_HOLES-1 of active hole
static int mole_timer;           // frames remaining for current state
static int hit_flash;            // frames left showing a hit flash

static int time_left;
static u32 score;
static int lives;
static u32 rng_state;

static u32 next_rand(void) {
    // simple xorshift so we don't depend on qran seeding specifics
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void hole_pos(int idx, int *x, int *y) {
    int gx = idx % GRID_W;
    int gy = idx / GRID_W;
    *x = GRID_LEFT + gx * HOLE_W;
    *y = GRID_TOP + gy * HOLE_H;
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

static void reset_game(void) {
    cursor_x = 1;
    cursor_y = 1;
    move_timer = 0;
    mole_state = MOLE_NONE;
    mole_hole = -1;
    mole_timer = 20 + (next_rand() % MOLE_GAP_MAX);
    hit_flash = 0;
    time_left = TIME_START;
    score = 0;
    lives = START_LIVES;
}

static void spawn_mole(void) {
    mole_hole = next_rand() % NUM_HOLES;
    mole_state = MOLE_UP;
    mole_timer = MOLE_UP_MIN + (next_rand() % (MOLE_UP_MAX - MOLE_UP_MIN));
}

static void draw_holes(void) {
    for (int i = 0; i < NUM_HOLES; i++) {
        int x, y;
        hole_pos(i, &x, &y);
        draw_rect(x + 6, y + 6, HOLE_W - 12, HOLE_H - 12, CLR_MAROON);
    }
}

static void draw_mole(void) {
    if (mole_state == MOLE_NONE) return;
    int x, y;
    hole_pos(mole_hole, &x, &y);
    u16 c = (mole_state == MOLE_HIT) ? CLR_YELLOW : CLR_LIME;
    draw_rect(x + 14, y + 8, HOLE_W - 28, HOLE_H - 20, c);
}

static void draw_cursor(void) {
    int idx = cursor_y * GRID_W + cursor_x;
    int x, y;
    hole_pos(idx, &x, &y);
    draw_rect(x + 2, y + 2, HOLE_W - 4, 3, CLR_WHITE);
    draw_rect(x + 2, y + HOLE_H - 5, HOLE_W - 4, 3, CLR_WHITE);
    draw_rect(x + 2, y + 2, 3, HOLE_H - 4, CLR_WHITE);
    draw_rect(x + HOLE_W - 5, y + 2, 3, HOLE_H - 4, CLR_WHITE);
}

static void draw_hud(void) {
    int shown = (time_left * SCREEN_WIDTH) / TIME_START;
    if (shown < 0) shown = 0;
    if (shown > SCREEN_WIDTH) shown = SCREEN_WIDTH;
    u16 bar_color = (time_left < TIME_START / 4) ? CLR_RED : CLR_WHITE;
    for (int i = 0; i < shown; i++) {
        m3_plot(i, 0, bar_color);
    }
    // lives remaining: red pips, top-right
    for (int i = 0; i < lives; i++) {
        draw_rect(SCREEN_WIDTH - 8 - i * 8, 3, 5, 3, CLR_RED);
    }
    // score: yellow pips scaled down (10 pts per pip, capped) under time bar
    u32 pips = score / 10;
    if (pips > 30) pips = 30;
    for (u32 i = 0; i < pips; i++) {
        m3_plot(i * 2, 3, CLR_YELLOW);
    }
}

static void draw_title(void) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 55, 20, 110, 14, CLR_WHITE);
    for (int i = 0; i < NUM_HOLES; i++) {
        int x, y;
        hole_pos(i, &x, &y);
        draw_rect(x + 6, y + 40, HOLE_W - 12, HOLE_H - 12, CLR_MAROON);
    }
    draw_rect(GRID_LEFT + HOLE_W + 14, GRID_TOP + 40 + 8, HOLE_W - 28, HOLE_H - 20, CLR_LIME);
}

static void draw_end_screen(u16 banner_color) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 35, 50, 70, 12, banner_color);
    draw_hud();
}

static void move_cursor(int dx, int dy) {
    int nx = cursor_x + dx;
    int ny = cursor_y + dy;
    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) return;
    cursor_x = nx;
    cursor_y = ny;
}

static void update_play(void) {
    time_left--;
    if (time_left <= 0) {
        state = STATE_WIN;
        return;
    }

    if (move_timer > 0) {
        move_timer--;
    } else {
        int dx = 0, dy = 0;
        if (key_is_down(KEY_LEFT)) dx = -1;
        else if (key_is_down(KEY_RIGHT)) dx = 1;
        else if (key_is_down(KEY_UP)) dy = -1;
        else if (key_is_down(KEY_DOWN)) dy = 1;

        if (dx != 0 || dy != 0) {
            move_cursor(dx, dy);
            move_timer = MOVE_DELAY;
        }
    }

    if (hit_flash > 0) hit_flash--;

    switch (mole_state) {
        case MOLE_NONE:
            mole_timer--;
            if (mole_timer <= 0) spawn_mole();
            break;

        case MOLE_UP: {
            int cursor_idx = cursor_y * GRID_W + cursor_x;
            if (key_hit(KEY_A) && cursor_idx == mole_hole) {
                score += 10;
                mole_state = MOLE_HIT;
                mole_timer = 8;
                hit_flash = 8;
            } else {
                mole_timer--;
                if (mole_timer <= 0) {
                    // missed - mole ducked away on its own
                    mole_state = MOLE_NONE;
                    mole_hole = -1;
                    mole_timer = MOLE_GAP_MIN + (next_rand() % (MOLE_GAP_MAX - MOLE_GAP_MIN));
                    lives--;
                    if (lives <= 0) {
                        state = STATE_GAMEOVER;
                        return;
                    }
                }
            }
            break;
        }

        case MOLE_HIT:
            mole_timer--;
            if (mole_timer <= 0) {
                mole_state = MOLE_NONE;
                mole_hole = -1;
                mole_timer = MOLE_GAP_MIN + (next_rand() % (MOLE_GAP_MAX - MOLE_GAP_MIN));
            }
            break;
    }

    clear_screen(CLR_BLACK);
    draw_holes();
    draw_mole();
    draw_cursor();
    draw_hud();
}

int main(void) {
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    rng_state = 0xC0FFEE1u;
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
