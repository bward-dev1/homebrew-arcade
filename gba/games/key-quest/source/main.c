// key-quest: top-down exploration maze game for GBA, mode 3 bitmap rendering.
//
// D-PAD: move one tile at a time through the maze
// A:     also confirms on title/end screens
// START: restart from title, or restart after game-over/win
//
// Goal: walk over every key scattered through the maze before the timer
// runs out, then reach the exit door (which stays locked/red until all
// keys are collected, then turns green) to win. This is a grid-based
// collect-and-escape exploration game - distinct from the ghost-chase
// pac-man-style maze on the NDS side (no chasing enemies here, the
// pressure is a countdown clock) and distinct from the jumper/breakout
// GBA games already in the arcade.
//
// Movement and the maze itself are tile-grid based (16x16 tiles on a
// 240x160 screen = 15x10 grid) so there's no float/physics dependency;
// everything is plain integer grid math.

#include <tonc.h>

#define CLR_WALLGRAY  RGB15(10, 10, 12)

#define TILE          16
#define MAZE_W        15
#define MAZE_H        10

#define MAX_KEYS      6

#define MOVE_DELAY    8     // frames between grid steps (debounce)
#define TIME_START    (60 * 45) // 45 seconds at 60fps

typedef enum { STATE_TITLE, STATE_PLAY, STATE_GAMEOVER, STATE_WIN } GameState;

static GameState state;

// 1 = wall, 0 = floor. Border is walled; interior has a fixed layout.
static const u8 maze[MAZE_H][MAZE_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,1,0,0,0,0,0,1,0,0,0,1},
    {1,0,1,0,1,0,1,1,0,1,1,0,1,0,1},
    {1,0,1,0,0,0,1,0,0,0,0,0,1,0,1},
    {1,0,1,1,1,1,1,0,1,1,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1},
    {1,0,1,0,0,0,0,0,1,0,0,0,1,0,1},
    {1,0,0,0,1,1,1,0,1,0,1,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

static const int key_pos[MAX_KEYS][2] = {
    {1, 1}, {13, 1}, {1, 8}, {13, 8}, {7, 3}, {5, 5}
};

static const int exit_pos[2] = {7, 5};
static const int start_pos[2] = {1, 5};

static int player_x, player_y;   // grid coords
static u8 key_collected[MAX_KEYS];
static int keys_left;
static int move_timer;
static int time_left;
static u32 score;

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

static int all_keys_found(void) {
    return keys_left <= 0;
}

static void reset_game(void) {
    player_x = start_pos[0];
    player_y = start_pos[1];
    keys_left = MAX_KEYS;
    for (int i = 0; i < MAX_KEYS; i++) key_collected[i] = 0;
    move_timer = 0;
    time_left = TIME_START;
    score = 0;
}

static void draw_maze(void) {
    for (int y = 0; y < MAZE_H; y++) {
        for (int x = 0; x < MAZE_W; x++) {
            u16 c = maze[y][x] ? CLR_WALLGRAY : CLR_BLACK;
            draw_rect(x * TILE, y * TILE, TILE, TILE, c);
        }
    }
}

static void draw_keys(void) {
    for (int i = 0; i < MAX_KEYS; i++) {
        if (key_collected[i]) continue;
        int px = key_pos[i][0] * TILE + 4;
        int py = key_pos[i][1] * TILE + 4;
        draw_rect(px, py, TILE - 8, TILE - 8, CLR_YELLOW);
    }
}

static void draw_exit(void) {
    int px = exit_pos[0] * TILE + 2;
    int py = exit_pos[1] * TILE + 2;
    u16 c = all_keys_found() ? CLR_LIME : CLR_RED;
    draw_rect(px, py, TILE - 4, TILE - 4, c);
}

static void draw_player(void) {
    int px = player_x * TILE + 3;
    int py = player_y * TILE + 3;
    draw_rect(px, py, TILE - 6, TILE - 6, CLR_CYAN);
}

static void draw_hud(void) {
    // time bar, top row of pixels, scaled to screen width
    int shown = (time_left * SCREEN_WIDTH) / TIME_START;
    if (shown < 0) shown = 0;
    if (shown > SCREEN_WIDTH) shown = SCREEN_WIDTH;
    u16 bar_color = (time_left < TIME_START / 4) ? CLR_RED : CLR_WHITE;
    for (int i = 0; i < shown; i++) {
        m3_plot(i, 0, bar_color);
    }
    // keys remaining: yellow pips, top-right, below the time bar
    for (int i = 0; i < MAX_KEYS - keys_left; i++) {
        draw_rect(SCREEN_WIDTH - 8 - i * 8, 3, 5, 3, CLR_YELLOW);
    }
}

static void draw_title(void) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 50, 30, 100, 12, CLR_WHITE);
    for (int i = 0; i < 4; i++) {
        draw_rect(40 + i * 40, 70, 10, 10, CLR_YELLOW);
    }
    draw_rect(SCREEN_WIDTH / 2 - 8, 110, 16, 16, CLR_RED);
    draw_rect(SCREEN_WIDTH / 2 - 8, 140, 16, 16, CLR_CYAN);
}

static void draw_end_screen(u16 banner_color) {
    clear_screen(CLR_BLACK);
    draw_rect(SCREEN_WIDTH / 2 - 30, 50, 60, 10, banner_color);
    draw_hud();
}

static void try_move(int dx, int dy) {
    int nx = player_x + dx;
    int ny = player_y + dy;
    if (nx < 0 || nx >= MAZE_W || ny < 0 || ny >= MAZE_H) return;
    if (maze[ny][nx]) return;
    player_x = nx;
    player_y = ny;
}

static void update_play(void) {
    time_left--;
    if (time_left <= 0) {
        state = STATE_GAMEOVER;
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
            try_move(dx, dy);
            move_timer = MOVE_DELAY;
        }
    }

    // key pickup
    for (int i = 0; i < MAX_KEYS; i++) {
        if (key_collected[i]) continue;
        if (key_pos[i][0] == player_x && key_pos[i][1] == player_y) {
            key_collected[i] = 1;
            keys_left--;
            score += 50;
        }
    }

    // exit check
    if (all_keys_found() && player_x == exit_pos[0] && player_y == exit_pos[1]) {
        score += time_left; // time bonus
        state = STATE_WIN;
        return;
    }

    // draw
    draw_maze();
    draw_keys();
    draw_exit();
    draw_player();
    draw_hud();
}

int main(void) {
    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

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
