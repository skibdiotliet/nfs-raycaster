/*
 * ============================================================
 *  RAYCASTER — True 3D raycasting engine in C
 * ============================================================
 *  Wolfenstein-style DDA raycaster. Casts rays through a 2D
 *  grid map, calculates wall distances per screen column,
 *  draws textured/floored strips. That's it.
 *
 *  Build:  make
 *  Run:    ./raycaster
 *
 *  Controls:
 *    W/UP    — Move forward
 *    S/DOWN  — Move backward
 *    A/LEFT  — Rotate left
 *    D/RIGHT — Rotate right
 *    ESC     — Quit
 * ============================================================
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* ── Window ──────────────────────────────────────────────────────────── */
#define SCREEN_W  1024
#define SCREEN_H  768

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
static double plane_y = 0.66;   /* camera plane, 66° FOV */

/* ── Movement ────────────────────────────────────────────────────────── */
#define MOVE_SPEED  3.0
#define ROT_SPEED   2.0

/* ── Wall colours per type (unused — textures handle rendering) ──────── */

/* ── Generate a procedural brick texture ─────────────────────────────── */
#define TEX_SIZE 64

typedef struct {
    uint32_t pixels[TEX_SIZE * TEX_SIZE];
} Texture;

static Texture textures[4]; /* one per wall type */

static void gen_textures(void) {
    for (int t = 0; t < 4; t++) {
        for (int y = 0; y < TEX_SIZE; y++) {
            for (int x = 0; x < TEX_SIZE; x++) {
                uint8_t r, g, b;
                int mortar = (x % 16 < 1) || (y % 8 < 1);
                int x_shifted = x + (((y / 8) % 2) * 8);  /* stagger bricks */

                if (t == 0) {
                    /* Red brick */
                    if (mortar) { r = 120; g = 110; b = 100; }
                    else { r = 160 + (x_shifted*7 + y*13) % 30; g = 60 + (x_shifted*3) % 15; b = 50; }
                } else if (t == 1) {
                    /* Green stone */
                    if (mortar) { r = 60; g = 80; b = 60; }
                    else { r = 40 + (x_shifted*11 + y*7) % 20; g = 140 + (x_shifted*5 + y*9) % 30; b = 50; }
                } else if (t == 2) {
                    /* Blue metal with rivets */
                    int rivet = ((x_shifted-4)%16 < 3 && (y-4)%16 < 3);
                    if (mortar) { r = 40; g = 40; b = 100; }
                    else if (rivet) { r = 120; g = 120; b = 200; }
                    else { r = 50 + (x_shifted+y)%10; g = 50 + (x_shifted+y)%10; b = 170 + (x_shifted*3)%30; }
                } else {
                    /* Grey concrete */
                    if (mortar) { r = 80; g = 80; b = 80; }
                    else { int v = 130 + (x_shifted*17 + y*31) % 25; r = v; g = v; b = v; }
                }
                textures[t].pixels[y * TEX_SIZE + x] = (r << 16) | (g << 8) | b;
            }
        }
    }
}

/* ── Z-buffer for sprite occlusion ───────────────────────────────────── */
static double z_buffer[SCREEN_W];

/* ── Sprite data ─────────────────────────────────────────────────────── */
#define MAX_SPRITES 8

typedef struct {
    double x;
    double y;
    int    type;  /* 0=red barrel, 1=green column, 2=blue pillar */
} Sprite;

static Sprite sprites[MAX_SPRITES] = {
    { 5.5,  5.5,  0 },
    { 10.5, 5.5,  1 },
    { 15.5, 5.5,  2 },
    { 7.5,  14.5, 0 },
    { 12.5, 14.5, 1 },
    { 17.5, 14.5, 2 },
    { 4.5,  20.5, 0 },
    { 20.5, 20.5, 1 },
};

/* ── Generate sprite textures ────────────────────────────────────────── */
#define SPR_TEX_SIZE 64
static Texture spr_textures[3];

static void gen_sprite_textures(void) {
    for (int t = 0; t < 3; t++) {
        for (int y = 0; y < SPR_TEX_SIZE; y++) {
            for (int x = 0; x < SPR_TEX_SIZE; x++) {
                int cx = x - SPR_TEX_SIZE/2;
                int cy = y - SPR_TEX_SIZE/2;
                int dist = cx*cx + cy*cy;
                int radius = (SPR_TEX_SIZE/2 - 4) * (SPR_TEX_SIZE/2 - 4);
                uint8_t r, g, b, a;

                if (dist > radius) {
                    /* Transparent */
                    r = g = b = a = 0;
                } else if (t == 0) {
                    /* Red barrel */
                    int stripe = (x / 8) % 2;
                    if (stripe) { r = 200; g = 40; b = 40; }
                    else { r = 160; g = 30; b = 30; }
                    a = 255;
                } else if (t == 1) {
                    /* Green column */
                    r = 30; g = 160 + (x+y)%20; b = 40;
                    a = 255;
                } else {
                    /* Blue pillar */
                    r = 40; g = 40; b = 180 + (x*3+y*7)%30;
                    a = 255;
                }
                spr_textures[t].pixels[y * SPR_TEX_SIZE + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/* ── SDL globals ─────────────────────────────────────────────────────── */
static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *fb_tex   = NULL;
static uint32_t      framebuf[SCREEN_W * SCREEN_H];
static int           running  = 1;

/* ── Set pixel in framebuffer ────────────────────────────────────────── */
static inline void set_pixel(int x, int y, uint32_t col) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        framebuf[y * SCREEN_W + x] = col;
}

/* ── Floor casting ───────────────────────────────────────────────────── */
static void cast_floor(double ray_dir_x0, double ray_dir_y0,
                       double ray_dir_x1, double ray_dir_y1,
                       int draw_end, int x) {
    for (int y = draw_end + 1; y < SCREEN_H; y++) {
        /* Current y vs centre of screen */
        int p = y - SCREEN_H / 2;
        if (p == 0) continue;

        /* Vertical distance from player to floor for current row */
        double row_dist = 0.5 * SCREEN_H / (double)p;

        /* Real world step between each pixel */
        double floor_step_x = row_dist * (ray_dir_x1 - ray_dir_x0) / SCREEN_W;
        double floor_step_y = row_dist * (ray_dir_y1 - ray_dir_y0) / SCREEN_W;

        /* Starting floor position */
        double floor_x = pos_x + row_dist * ray_dir_x0;
        double floor_y = pos_y + row_dist * ray_dir_y0;

        /* Step along the floor */
        floor_x += floor_step_x * x;
        floor_y += floor_step_y * x;

        /* Tile coordinate */
        int tx = (int)floor_x & (TEX_SIZE - 1);
        int ty = (int)floor_y & (TEX_SIZE - 1);

        /* Checkerboard floor */
        int checker = ((int)floor_x + (int)floor_y) & 1;
        uint32_t fcol;
        if (checker) {
            fcol = textures[3].pixels[ty * TEX_SIZE + tx]; /* grey tile */
        } else {
            fcol = textures[3].pixels[ty * TEX_SIZE + tx] & 0x00FEFEFE; /* darkened */
        }

        /* Ceiling (mirror) */
        int ceil_y = SCREEN_H - y - 1;

        /* Simple ceiling colour */
        int fog = (int)(row_dist * 12);
        if (fog > 100) fog = 100;
        uint8_t cr = 40 - fog/3; if (cr > 40) cr = 0;
        uint8_t cg = 40 - fog/3; if (cg > 40) cg = 0;
        uint8_t cb = 60 - fog/2; if (cb > 60) cb = 0;
        uint32_t ccol = (cr << 16) | (cg << 8) | cb;

        set_pixel(x, y, fcol);
        set_pixel(x, ceil_y, ccol);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  RAYCAST — the core DDA algorithm for one screen column
 * ═══════════════════════════════════════════════════════════════════════ */
static void cast_ray(int x) {
    /* Camera x coordinate: -1 (left) to +1 (right) */
    double camera_x = 2.0 * x / (double)SCREEN_W - 1.0;

    /* Ray direction */
    double ray_dir_x = dir_x + plane_x * camera_x;
    double ray_dir_y = dir_y + plane_y * camera_x;

    /* Grid cell */
    int map_x = (int)pos_x;
    int map_y = (int)pos_y;

    /* Delta dist: length of ray from one side of grid to the other */
    double delta_dist_x = (ray_dir_x == 0) ? 1e30 : fabs(1.0 / ray_dir_x);
    double delta_dist_y = (ray_dir_y == 0) ? 1e30 : fabs(1.0 / ray_dir_y);

    /* Step direction and initial side distance */
    int    step_x, step_y;
    double side_dist_x, side_dist_y;

    if (ray_dir_x < 0) {
        step_x      = -1;
        side_dist_x = (pos_x - map_x) * delta_dist_x;
    } else {
        step_x      = 1;
        side_dist_x = (map_x + 1.0 - pos_x) * delta_dist_x;
    }
    if (ray_dir_y < 0) {
        step_y      = -1;
        side_dist_y = (pos_y - map_y) * delta_dist_y;
    } else {
        step_y      = 1;
        side_dist_y = (map_y + 1.0 - pos_y) * delta_dist_y;
    }

    /* DDA — march through grid until we hit a wall */
    int hit  = 0;
    int side = 0; /* 0 = NS wall, 1 = EW wall */

    while (!hit) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x       += step_x;
            side         = 0;
        } else {
            side_dist_y += delta_dist_y;
            map_y       += step_y;
            side         = 1;
        }
        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) break;
        if (world_map[map_y][map_x] > 0) hit = 1;
    }

    /* Perpendicular distance (avoids fisheye) */
    double perp_dist;
    if (side == 0)
        perp_dist = side_dist_x - delta_dist_x;
    else
        perp_dist = side_dist_y - delta_dist_y;

    if (perp_dist < 0.001) perp_dist = 0.001;

    /* Height of wall strip */
    int line_h = (int)(SCREEN_H / perp_dist);

    /* Draw start/end */
    int draw_start = -line_h / 2 + SCREEN_H / 2;
    if (draw_start < 0) draw_start = 0;
    int draw_end = line_h / 2 + SCREEN_H / 2;
    if (draw_end >= SCREEN_H) draw_end = SCREEN_H - 1;

    /* Wall texture X coordinate (where on the wall did we hit?) */
    double wall_x;
    if (side == 0)
        wall_x = pos_y + perp_dist * ray_dir_y;
    else
        wall_x = pos_x + perp_dist * ray_dir_x;
    wall_x -= floor(wall_x);

    /* Texture column */
    int tex_x = (int)(wall_x * (double)TEX_SIZE);
    if (side == 0 && ray_dir_x > 0) tex_x = TEX_SIZE - tex_x - 1;
    if (side == 1 && ray_dir_y < 0) tex_x = TEX_SIZE - tex_x - 1;

    /* Get wall type */
    int wall_type = 0;
    if (map_x >= 0 && map_x < MAP_W && map_y >= 0 && map_y < MAP_H)
        wall_type = world_map[map_y][map_x];

    int tex_idx = wall_type - 1;
    if (tex_idx < 0 || tex_idx > 3) tex_idx = 3;

    /* Draw textured wall column */
    double step = (double)TEX_SIZE / (double)line_h;
    double tex_pos = (draw_start - SCREEN_H / 2.0 + line_h / 2.0) * step;

    for (int y = draw_start; y <= draw_end; y++) {
        int tex_y = (int)tex_pos & (TEX_SIZE - 1);
        tex_pos += step;

        uint32_t col = textures[tex_idx].pixels[tex_y * TEX_SIZE + tex_x];

        /* Darken EW-facing walls */
        if (side == 1) {
            uint8_t r = (col >> 16) & 0xFF;
            uint8_t g = (col >> 8)  & 0xFF;
            uint8_t b =  col        & 0xFF;
            r = r * 2 / 3;
            g = g * 2 / 3;
            b = b * 2 / 3;
            col = (r << 16) | (g << 8) | b;
        }

        /* Distance fog */
        int fog = (int)(perp_dist * 18);
        if (fog > 150) fog = 150;
        uint8_t r = ((col >> 16) & 0xFF) - fog;
        uint8_t g = ((col >> 8)  & 0xFF) - fog;
        uint8_t b = ( col        & 0xFF) - fog;
        set_pixel(x, y, (r << 16) | (g << 8) | b);
    }

    /* Floor and ceiling */
    cast_floor(ray_dir_x, ray_dir_y,
               dir_x - plane_x, dir_y - plane_y,
               draw_end, x);

    /* Store in z-buffer for sprite occlusion */
    z_buffer[x] = perp_dist;
}

/* ── Draw sprites ────────────────────────────────────────────────────── */
static void draw_sprites(void) {
    /* Sort sprites by distance (far first) */
    double dist[MAX_SPRITES];
    int    order[MAX_SPRITES];
    for (int i = 0; i < MAX_SPRITES; i++) {
        order[i] = i;
        dist[i] = (pos_x - sprites[i].x) * (pos_x - sprites[i].x)
                + (pos_y - sprites[i].y) * (pos_y - sprites[i].y);
    }
    /* Simple bubble sort */
    for (int i = 0; i < MAX_SPRITES - 1; i++) {
        for (int j = i + 1; j < MAX_SPRITES; j++) {
            if (dist[i] < dist[j]) {
                double td = dist[i]; dist[i] = dist[j]; dist[j] = td;
                int    to = order[i]; order[i] = order[j]; order[j] = to;
            }
        }
    }

    for (int i = 0; i < MAX_SPRITES; i++) {
        Sprite *sp = &sprites[order[i]];

        /* Translate relative to camera */
        double sx = sp->x - pos_x;
        double sy = sp->y - pos_y;

        /* Inverse camera matrix transform */
        double inv_det = 1.0 / (plane_x * dir_y - dir_x * plane_y);
        double transform_x = inv_det * (dir_y * sx - dir_x * sy);
        double transform_y = inv_det * (-plane_y * sx + plane_x * sy);

        if (transform_y <= 0.1) continue; /* behind camera */

        int sprite_screen_x = (int)((SCREEN_W / 2.0) * (1.0 + transform_x / transform_y));

        /* Sprite dimensions on screen */
        int sprite_h = abs((int)(SCREEN_H / transform_y));
        int draw_start_y = -sprite_h / 2 + SCREEN_H / 2;
        if (draw_start_y < 0) draw_start_y = 0;
        int draw_end_y = sprite_h / 2 + SCREEN_H / 2;
        if (draw_end_y >= SCREEN_H) draw_end_y = SCREEN_H - 1;

        int sprite_w = abs((int)(SCREEN_H / transform_y));
        int draw_start_x = -sprite_w / 2 + sprite_screen_x;
        if (draw_start_x < 0) draw_start_x = 0;
        int draw_end_x = sprite_w / 2 + sprite_screen_x;
        if (draw_end_x >= SCREEN_W) draw_end_x = SCREEN_W - 1;

        /* Draw sprite columns */
        Texture *stex = &spr_textures[sp->type];
        for (int stripe = draw_start_x; stripe < draw_end_x; stripe++) {
            int tex_x = (int)((stripe - (-sprite_w / 2 + sprite_screen_x))
                              * (double)SPR_TEX_SIZE / sprite_w);
            if (tex_x < 0 || tex_x >= SPR_TEX_SIZE) continue;
            if (transform_y < z_buffer[stripe]) {
                for (int y = draw_start_y; y < draw_end_y; y++) {
                    int d = y * 2 - SCREEN_H + sprite_h;
                    int tex_y = (int)((d * (double)SPR_TEX_SIZE) / sprite_h / 2.0);
                    if (tex_y < 0 || tex_y >= SPR_TEX_SIZE) continue;
                    uint32_t col = stex->pixels[tex_y * SPR_TEX_SIZE + tex_x];
                    uint8_t a = (col >> 24) & 0xFF;
                    if (a > 128) {
                        set_pixel(stripe, y, col & 0x00FFFFFF);
                    }
                }
            }
        }
    }
}

/* ── Draw minimap ────────────────────────────────────────────────────── */
static void draw_minimap(void) {
    int scale = 8;
    int ox = 10, oy = 10;

    for (int my = 0; my < MAP_H; my++) {
        for (int mx = 0; mx < MAP_W; mx++) {
            uint32_t col;
            if (world_map[my][mx] > 0) {
                switch (world_map[my][mx]) {
                    case 1: col = 0x00B43232; break;
                    case 2: col = 0x0032B432; break;
                    case 3: col = 0x003232C8; break;
                    default: col = 0x00969696; break;
                }
            } else {
                col = 0x00181818;
            }
            for (int py = 0; py < scale - 1; py++)
                for (int px = 0; px < scale - 1; px++)
                    set_pixel(ox + mx * scale + px, oy + my * scale + py, col);
        }
    }

    /* Player dot */
    int px = ox + (int)(pos_x * scale);
    int py = oy + (int)(pos_y * scale);
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            set_pixel(px + dx, py + dy, 0x00FFFF00);

    /* Direction line */
    for (int i = 0; i < 12; i++)
        set_pixel(px + (int)(dir_x * i), py + (int)(dir_y * i), 0x00FFFF00);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        "Raycaster — True 3D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Window failed: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer failed: %s\n", SDL_GetError());
        return 1;
    }

    fb_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                               SDL_TEXTUREACCESS_STREAMING,
                               SCREEN_W, SCREEN_H);
    if (!fb_tex) {
        fprintf(stderr, "Texture failed: %s\n", SDL_GetError());
        return 1;
    }

    gen_textures();
    gen_sprite_textures();

    Uint64 prev_tick = SDL_GetPerformanceCounter();

    printf("╔══════════════════════════════╗\n");
    printf("║   RAYCASTER — True 3D        ║\n");
    printf("╠══════════════════════════════╣\n");
    printf("║  W/UP    — Move forward      ║\n");
    printf("║  S/DOWN  — Move backward     ║\n");
    printf("║  A/LEFT  — Rotate left       ║\n");
    printf("║  D/RIGHT — Rotate right      ║\n");
    printf("║  ESC     — Quit              ║\n");
    printf("╚══════════════════════════════╝\n");

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        /* Delta time */
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - prev_tick) / SDL_GetPerformanceFrequency();
        prev_tick = now;
        if (dt > 0.05) dt = 0.05;

        /* ── Input ────────────────────────────────────────────────────── */
        const Uint8 *keys = SDL_GetKeyboardState(NULL);

        /* Move forward/backward */
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
            double new_x = pos_x + dir_x * MOVE_SPEED * dt;
            double new_y = pos_y + dir_y * MOVE_SPEED * dt;
            if (world_map[(int)pos_y][(int)new_x] == 0) pos_x = new_x;
            if (world_map[(int)new_y][(int)pos_x] == 0) pos_y = new_y;
        }
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
            double new_x = pos_x - dir_x * MOVE_SPEED * dt;
            double new_y = pos_y - dir_y * MOVE_SPEED * dt;
            if (world_map[(int)pos_y][(int)new_x] == 0) pos_x = new_x;
            if (world_map[(int)new_y][(int)pos_x] == 0) pos_y = new_y;
        }

        /* Rotate */
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
            double old_dx = dir_x;
            dir_x = dir_x * cos(ROT_SPEED * dt) - dir_y * sin(ROT_SPEED * dt);
            dir_y = old_dx * sin(ROT_SPEED * dt) + dir_y * cos(ROT_SPEED * dt);
            double old_px = plane_x;
            plane_x = plane_x * cos(ROT_SPEED * dt) - plane_y * sin(ROT_SPEED * dt);
            plane_y = old_px * sin(ROT_SPEED * dt) + plane_y * cos(ROT_SPEED * dt);
        }
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
            double old_dx = dir_x;
            dir_x = dir_x * cos(-ROT_SPEED * dt) - dir_y * sin(-ROT_SPEED * dt);
            dir_y = old_dx * sin(-ROT_SPEED * dt) + dir_y * cos(-ROT_SPEED * dt);
            double old_px = plane_x;
            plane_x = plane_x * cos(-ROT_SPEED * dt) - plane_y * sin(-ROT_SPEED * dt);
            plane_y = old_px * sin(-ROT_SPEED * dt) + plane_y * cos(-ROT_SPEED * dt);
        }

        /* ── Render ───────────────────────────────────────────────────── */
        memset(framebuf, 0, sizeof(framebuf));

        /* Cast a ray for every screen column */
        for (int x = 0; x < SCREEN_W; x++) {
            cast_ray(x);
        }

        /* Draw sprites on top */
        draw_sprites();

        /* Minimap overlay */
        draw_minimap();

        /* Blit framebuffer */
        SDL_UpdateTexture(fb_tex, NULL, framebuf, SCREEN_W * sizeof(uint32_t));
        SDL_RenderCopy(renderer, fb_tex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(fb_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
