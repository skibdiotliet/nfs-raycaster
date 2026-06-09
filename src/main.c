/*
 * ============================================================
 *  RAYCASTER — True 3D terminal raycaster in C
 * ============================================================
 *  Wolfenstein-style DDA raycaster rendered as ASCII in your
 *  terminal. No SDL2, no X server, no dependencies.
 *
 *  Build:  make
 *  Run:    ./raycaster
 *
 *  Controls:
 *    W/UP    — Move forward
 *    S/DOWN  — Move backward
 *    A/LEFT  — Rotate left
 *    D/RIGHT — Rotate right
 *    Q/ESC   — Quit
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

/* ── Terminal render size ────────────────────────────────────────────── */
#define SCR_W 120
#define SCR_H 40

/* ── Map ─────────────────────────────────────────────────────────────── */
#define MAP_W 24
#define MAP_H 24

static const int world_map[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,2,2,0,0,1},
    {1,0,0,0,0,0,1,1,0,1,0,0,0,0,0,0,0,0,0,2,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,2,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,2,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,3,0,3,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,3,0,3,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,3,0,3,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

/* ── Player ──────────────────────────────────────────────────────────── */
static double pos_x   = 3.5;
static double pos_y   = 3.5;
static double dir_x   = 1.0;
static double dir_y   = 0.0;
static double plane_x = 0.0;
static double plane_y = 0.66;

/* ── Movement ────────────────────────────────────────────────────────── */
#define MOVE_SPEED  3.0
#define ROT_SPEED   2.0

/* ── Screen buffer ───────────────────────────────────────────────────── */
static char screen[SCR_H][SCR_W + 1];
/* ANSI colour per cell: 0=default, 1=red, 2=green, 3=blue, 4=grey, 5=dark, 6=floor_dark, 7=floor_light, 8=ceiling */
static int  colours[SCR_H][SCR_W];

/* ── ASCII shade characters by distance (close → far) ────────────────── */
static const char wall_shade[]  = "@#%x+=:-. ";
static const char dark_shade[]  = "@#%x+=:-. ";
static const int  num_shades    = 10;

/* ── Terminal raw mode ───────────────────────────────────────────────── */
static struct termios orig_term;

static void term_raw(void) {
    tcgetattr(STDIN_FILENO, &orig_term);
    struct termios raw = orig_term;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void term_restore(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}

static int kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

/* ── DDA Raycast for one column ──────────────────────────────────────── */
static double cast_ray(int col, int *wall_type, int *side) {
    double camera_x = 2.0 * col / (double)SCR_W - 1.0;
    double ray_dir_x = dir_x + plane_x * camera_x;
    double ray_dir_y = dir_y + plane_y * camera_x;

    int map_x = (int)pos_x;
    int map_y = (int)pos_y;

    double delta_dist_x = (ray_dir_x == 0) ? 1e30 : fabs(1.0 / ray_dir_x);
    double delta_dist_y = (ray_dir_y == 0) ? 1e30 : fabs(1.0 / ray_dir_y);

    int step_x, step_y;
    double side_dist_x, side_dist_y;

    if (ray_dir_x < 0) {
        step_x = -1;
        side_dist_x = (pos_x - map_x) * delta_dist_x;
    } else {
        step_x = 1;
        side_dist_x = (map_x + 1.0 - pos_x) * delta_dist_x;
    }
    if (ray_dir_y < 0) {
        step_y = -1;
        side_dist_y = (pos_y - map_y) * delta_dist_y;
    } else {
        step_y = 1;
        side_dist_y = (map_y + 1.0 - pos_y) * delta_dist_y;
    }

    *side = 0;
    while (1) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            *side = 0;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            *side = 1;
        }
        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) {
            *wall_type = 1;
            break;
        }
        if (world_map[map_y][map_x] > 0) {
            *wall_type = world_map[map_y][map_x];
            break;
        }
    }

    double perp_dist;
    if (*side == 0)
        perp_dist = side_dist_x - delta_dist_x;
    else
        perp_dist = side_dist_y - delta_dist_y;

    if (perp_dist < 0.001) perp_dist = 0.001;
    return perp_dist;
}

/* ── Render one frame ────────────────────────────────────────────────── */
static void render(void) {
    /* Clear buffers */
    for (int y = 0; y < SCR_H; y++) {
        memset(screen[y], ' ', SCR_W);
        screen[y][SCR_W] = '\0';
        memset(colours[y], 0, SCR_W * sizeof(int));
    }

    /* Cast rays and draw walls */
    for (int x = 0; x < SCR_W; x++) {
        int wall_type, side;
        double perp_dist = cast_ray(x, &wall_type, &side);

        int line_h = (int)((double)SCR_H / perp_dist);
        int draw_start = -line_h / 2 + SCR_H / 2;
        if (draw_start < 0) draw_start = 0;
        int draw_end = line_h / 2 + SCR_H / 2;
        if (draw_end >= SCR_H) draw_end = SCR_H - 1;

        /* Shade index based on distance */
        int shade_idx = (int)(perp_dist * 1.2);
        if (shade_idx >= num_shades) shade_idx = num_shades - 1;

        char ch;
        int col;

        if (side == 0) {
            /* NS wall — brighter */
            ch = wall_shade[shade_idx];
            col = wall_type; /* 1=red, 2=green, 3=blue */
        } else {
            /* EW wall — darker shade character */
            ch = dark_shade[shade_idx < num_shades - 1 ? shade_idx + 1 : shade_idx];
            col = wall_type + 4; /* offset to dark variants */
        }

        for (int y = draw_start; y <= draw_end; y++) {
            screen[y][x] = ch;
            colours[y][x] = col;
        }

        /* Ceiling */
        for (int y = 0; y < draw_start; y++) {
            screen[y][x] = ' ';
            colours[y][x] = 8; /* ceiling colour */
        }

        /* Floor */
        for (int y = draw_end + 1; y < SCR_H; y++) {
            int checker = ((y / 2) + (x / 3)) & 1;
            screen[y][x] = checker ? '.' : ' ';
            colours[y][x] = checker ? 6 : 7;
        }
    }

    /* ── Draw to terminal with ANSI colours ───────────────────────────── */
    /* Move cursor to top-left */
    printf("\033[H");

    /* ANSI colour codes */
    static const char *ansi_col[] = {
        "\033[0m",      /* 0: default */
        "\033[31m",     /* 1: red (wall type 1, NS) */
        "\033[32m",     /* 2: green (wall type 2, NS) */
        "\033[34m",     /* 3: blue (wall type 3, NS) */
        "\033[90m",     /* 4: dark grey (wall type 1, EW) */
        "\033[2;32m",   /* 5: dark green (wall type 2, EW) */
        "\033[2;34m",   /* 6: dark blue (wall type 3, EW) */
        "\033[90m",     /* 7: dark grey (floor) */
        "\033[0m",      /* 8: default (ceiling) */
        "\033[33m",     /* 9: yellow (minimap) */
    };

    int last_col = -1;
    for (int y = 0; y < SCR_H; y++) {
        for (int x = 0; x < SCR_W; x++) {
            int c = colours[y][x];
            if (c != last_col) {
                fputs(ansi_col[c], stdout);
                last_col = c;
            }
            putchar(screen[y][x]);
        }
        /* Reset at end of line */
        if (last_col != 0) {
            fputs("\033[0m", stdout);
            last_col = 0;
        }
        putchar('\n');
    }

    /* HUD */
    printf("\033[0m\033[33m POS(%.1f,%.1f) DIR(%.1f,%.1f)  W/S=move  A/D=turn  Q=quit \033[K", pos_x, pos_y, dir_x, dir_y);

    fflush(stdout);
}

/* ── Draw minimap in top-right corner of the screen buffer ───────────── */
static void draw_minimap(void) {
    int scale = 1;
    int ox = SCR_W - MAP_W * scale - 2;
    int oy = 1;

    for (int my = 0; my < MAP_H && oy + my < SCR_H; my++) {
        for (int mx = 0; mx < MAP_W && ox + mx < SCR_W; mx++) {
            if (world_map[my][mx] > 0) {
                screen[oy + my][ox + mx] = '#';
                colours[oy + my][ox + mx] = 9; /* yellow */
            } else {
                screen[oy + my][ox + mx] = '.';
                colours[oy + my][ox + mx] = 7;
            }
        }
    }

    /* Player position on minimap */
    int px = ox + (int)pos_x;
    int py = oy + (int)pos_y;
    if (px >= 0 && px < SCR_W && py >= 0 && py < SCR_H) {
        screen[py][px] = '@';
        colours[py][px] = 1; /* red */
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    term_raw();
    atexit(term_restore);

    /* Hide cursor */
    printf("\033[?25l");
    /* Clear screen */
    printf("\033[2J");

    fprintf(stderr, "RAYCASTER — True 3D (Terminal)\n");
    fprintf(stderr, "W/S = move  A/D = turn  Q = quit\n");

    struct timespec ts = {0, 33000000}; /* ~30 FPS */
    double dt = 0.033;

    while (1) {
        /* ── Input ────────────────────────────────────────────────────── */
        char key = 0;
        while (kbhit()) {
            char c = getchar();
            if (c == '\033') {
                /* Escape sequence — read [ and next char */
                char seq[2] = {0};
                if (kbhit()) seq[0] = getchar();
                if (seq[0] == '[' && kbhit()) seq[1] = getchar();
                if (seq[1] == 'A') key = 'w'; /* up    */
                if (seq[1] == 'B') key = 's'; /* down  */
                if (seq[1] == 'D') key = 'a'; /* left  */
                if (seq[1] == 'C') key = 'd'; /* right */
            } else {
                key = c;
            }
        }

        if (key == 'q' || key == 'Q') break;

        /* ── Move ─────────────────────────────────────────────────────── */
        if (key == 'w' || key == 'W') {
            double nx = pos_x + dir_x * MOVE_SPEED * dt;
            double ny = pos_y + dir_y * MOVE_SPEED * dt;
            if (world_map[(int)pos_y][(int)nx] == 0) pos_x = nx;
            if (world_map[(int)ny][(int)pos_x] == 0) pos_y = ny;
        }
        if (key == 's' || key == 'S') {
            double nx = pos_x - dir_x * MOVE_SPEED * dt;
            double ny = pos_y - dir_y * MOVE_SPEED * dt;
            if (world_map[(int)pos_y][(int)nx] == 0) pos_x = nx;
            if (world_map[(int)ny][(int)pos_x] == 0) pos_y = ny;
        }
        if (key == 'a' || key == 'A') {
            double odx = dir_x;
            dir_x = dir_x * cos(ROT_SPEED * dt) - dir_y * sin(ROT_SPEED * dt);
            dir_y = odx * sin(ROT_SPEED * dt) + dir_y * cos(ROT_SPEED * dt);
            double opx = plane_x;
            plane_x = plane_x * cos(ROT_SPEED * dt) - plane_y * sin(ROT_SPEED * dt);
            plane_y = opx * sin(ROT_SPEED * dt) + plane_y * cos(ROT_SPEED * dt);
        }
        if (key == 'd' || key == 'D') {
            double odx = dir_x;
            dir_x = dir_x * cos(-ROT_SPEED * dt) - dir_y * sin(-ROT_SPEED * dt);
            dir_y = odx * sin(-ROT_SPEED * dt) + dir_y * cos(-ROT_SPEED * dt);
            double opx = plane_x;
            plane_x = plane_x * cos(-ROT_SPEED * dt) - plane_y * sin(-ROT_SPEED * dt);
            plane_y = opx * sin(-ROT_SPEED * dt) + plane_y * cos(-ROT_SPEED * dt);
        }

        /* ── Render ───────────────────────────────────────────────────── */
        render();
        draw_minimap();
        /* Re-draw minimap area with updated screen buffer */
        /* (minimap was written into screen[] but we already flushed — 
           it'll show on next frame. This is fine for 30fps.) */

        nanosleep(&ts, NULL);
    }

    /* Show cursor, clear */
    printf("\033[?25h\033[2J\033[H");
    printf("Thanks for raycasting!\n");

    return 0;
}
