/*
 * ========================================================================
 *  NFS RAYCASTER — A Need-for-Speed-style pseudo-3D racing game in C
 * ========================================================================
 *  Uses the classic segment-projection raycasting technique (OutRun style)
 *  to render a road with curves, hills, and rival cars.
 *
 *  Build:  make
 *  Run:    ./nfs_raycaster
 *
 *  Controls:
 *    UP     — Accelerate
 *    DOWN   — Brake / Reverse
 *    LEFT   — Steer Left
 *    RIGHT  — Steer Right
 *    ESC    — Quit
 * ========================================================================
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <time.h>

/* ── Window & Rendering ─────────────────────────────────────────────── */
#define SCREEN_W      1024
#define SCREEN_H      768
#define ROAD_W        2400      /* logical road width in world units      */
#define SEG_LEN       200       /* length of one segment                  */
#define DRAW_DIST     300       /* how many segments to draw ahead        */
#define CAM_DEPTH     0.84      /* camera depth (field-of-view factor)    */
#define CAM_H         1200      /* camera height above road               */

/* ── Gameplay ────────────────────────────────────────────────────────── */
#define MAX_SPEED     (SEG_LEN * 60)   /* top speed units/sec            */
#define ACCEL         (MAX_SPEED / 3)  /* acceleration units/sec^2       */
#define BRAKE         (MAX_SPEED * 1.5)/* brake deceleration             */
#define DECEL         (MAX_SPEED / 5)  /* natural slow-down              */
#define OFF_ROAD_DECEL(MAX_SPEED * 0.8)/* off-road drag                  */
#define STEER_SPEED   3.0               /* lateral movement factor       */
#define CENTRIFUGAL   0.3               /* centrifugal pull on curves    */

/* ── Track ───────────────────────────────────────────────────────────── */
#define TRACK_SEGS    1600
#define TRACK_LAPS    3

/* ── Colours ─────────────────────────────────────────────────────────── */
#define SKY_TOP       { 20, 20, 80 }
#define SKY_BOT       { 140,100,180 }
#define MOUNT_COL     { 60, 50, 70 }
#define TREE_TRUNK    { 80, 50, 20 }
#define TREE_LEAVES   { 20,120, 20 }
#define GRASS_DARK    { 30,130, 30 }
#define GRASS_LIGHT   { 40,160, 40 }
#define ROAD_DARK     { 70, 70, 70 }
#define ROAD_LIGHT    { 80, 80, 80 }
#define RUMBLE_DARK   {200, 40, 40 }
#define RUMBLE_LIGHT  {255,255,255 }
#define LANE_MARK     {255,255,255 }
#define HUD_BG        {  0,  0,  0 }
#define HUD_FG        {255,255,255 }
#define LAP_COL       {255,220,  0 }

/* ── Segment structure ───────────────────────────────────────────────── */
typedef struct {
    float curve;       /* curvature for this segment          */
    float y;           /* hill height at segment start        */
} SegDef;

typedef struct {
    float world_z;     /* z position in world                 */
    float scale;       /* perspective scale                   */
    float screen_x;    /* projected x                         */
    float screen_y;    /* projected y                         */
    float screen_w;    /* projected road half-width           */
    float clip_y;      /* y clip for occlusion                */
} ProjSeg;

/* ── Rival car ───────────────────────────────────────────────────────── */
#define MAX_RIVALS  12

typedef struct {
    float z;           /* world z position                    */
    float x;           /* lateral offset (-1..1 across road)  */
    float speed;       /* units per second                    */
    int   colour;      /* 0=red 1=blue 2=yellow 3=green      */
} Rival;

/* ── Game state ──────────────────────────────────────────────────────── */
static SegDef  track[TRACK_SEGS];
static ProjSeg proj[DRAW_DIST + 1];
static Rival   rivals[MAX_RIVALS];

static float player_x   = 0.0f;   /* -1..1 across road              */
static float player_z   = 0.0f;   /* world z position               */
static float speed      = 0.0f;   /* current speed                  */
static int   lap        = 0;
static float lap_timer  = 0.0f;
static float best_lap   = 999.99f;
static float total_time = 0.0f;
static int   countdown  = 3;       /* 3-2-1-GO countdown             */
static float countdown_t= 0.0f;
static int   game_over  = 0;

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static int  running = 1;
static Uint64 prev_tick = 0;

/* ── Helper: clamp ───────────────────────────────────────────────────── */
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

/* ── Build the track ─────────────────────────────────────────────────── */
static void build_track(void) {
    float y = 0;
    for (int i = 0; i < TRACK_SEGS; i++) {
        float p = (float)i / TRACK_SEGS;

        /* Curves — sin waves of different frequencies */
        float curve = 0;
        if (p > 0.02f && p < 0.12f) curve =  2.0f * sinf((p-0.02f)/0.10f * M_PI);
        if (p > 0.18f && p < 0.30f) curve = -3.5f * sinf((p-0.18f)/0.12f * M_PI);
        if (p > 0.38f && p < 0.50f) curve =  4.0f * sinf((p-0.38f)/0.12f * M_PI);
        if (p > 0.58f && p < 0.68f) curve = -2.5f * sinf((p-0.58f)/0.10f * M_PI);
        if (p > 0.75f && p < 0.88f) curve =  3.0f * sinf((p-0.75f)/0.13f * M_PI);

        /* Hills — elevation changes */
        float hill = 0;
        if (p > 0.05f && p < 0.15f) hill =  2500.0f * sinf((p-0.05f)/0.10f * M_PI);
        if (p > 0.25f && p < 0.35f) hill = -1500.0f * sinf((p-0.25f)/0.10f * M_PI);
        if (p > 0.45f && p < 0.55f) hill =  3000.0f * sinf((p-0.45f)/0.10f * M_PI);
        if (p > 0.65f && p < 0.78f) hill = -2000.0f * sinf((p-0.65f)/0.13f * M_PI);
        if (p > 0.85f && p < 0.95f) hill =  1800.0f * sinf((p-0.85f)/0.10f * M_PI);

        y += hill * 0.005f;
        track[i].curve = curve;
        track[i].y     = y;
    }
}

/* ── Place rival cars ────────────────────────────────────────────────── */
static void init_rivals(void) {
    for (int i = 0; i < MAX_RIVALS; i++) {
        rivals[i].z      = (float)(i + 1) * (TRACK_SEGS * SEG_LEN) / (MAX_RIVALS + 1);
        rivals[i].x      = (float)(rand() % 160 - 80) / 100.0f;
        rivals[i].speed  = MAX_SPEED * (0.30f + 0.40f * (float)rand() / RAND_MAX);
        rivals[i].colour = i % 4;
    }
}

/* ── Project a segment ───────────────────────────────────────────────── */
static void project_segment(ProjSeg *ps, float cam_x, float cam_y, float cam_z,
                            float seg_z, float seg_y) {
    ps->world_z = seg_z;
    float dz = seg_z - cam_z;
    if (dz <= 0) dz = 0.001f;
    ps->scale    = CAM_DEPTH / dz;
    ps->screen_x = SCREEN_W * 0.5f + ps->scale * (CAM_H * (-cam_x)) * SCREEN_W * 0.5f;
    ps->screen_y = SCREEN_H * 0.5f - ps->scale * ((seg_y - cam_y) - CAM_H) * SCREEN_H * 0.5f;
    ps->screen_w = ps->scale * ROAD_W * SCREEN_W * 0.5f;
}

/* ── Draw a filled polygon ───────────────────────────────────────────── */
static void draw_quad(SDL_Renderer *r, SDL_Color c,
                      float x1, float y1, float w1,
                      float x2, float y2, float w2) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    float lx1 = x1 - w1, rx1 = x1 + w1;
    float lx2 = x2 - w2, rx2 = x2 + w2;
    /* Simple scanline fill — draw horizontal lines from top to bottom */
    int top = (int)y1, bot = (int)y2;
    if (bot < top) { int t = top; top = bot; bot = t; }
    if (bot < 0 || top >= SCREEN_H) return;
    if (top < 0)  top = 0;
    if (bot >= SCREEN_H) bot = SCREEN_H - 1;
    for (int row = top; row <= bot; row++) {
        float t = (y2 == y1) ? 0.5f : (row - y1) / (y2 - y1);
        float lx = lx1 + (lx2 - lx1) * t;
        float rx = rx1 + (rx2 - rx1) * t;
        SDL_RenderDrawLine(r, (int)lx, row, (int)rx, row);
    }
}

/* ── Draw a simple car sprite ────────────────────────────────────────── */
static void draw_car(SDL_Renderer *r, float cx, float cy, float scale, int colour) {
    float w = 50 * scale;
    float h = 80 * scale;
    if (w < 2 || h < 3) return;

    SDL_Color body;
    switch (colour) {
        case 0: body = (SDL_Color){220, 30, 30}; break;  /* red    */
        case 1: body = (SDL_Color){ 30, 30,220}; break;  /* blue   */
        case 2: body = (SDL_Color){220,200, 30}; break;  /* yellow */
        case 3: body = (SDL_Color){ 30,180, 30}; break;  /* green  */
        default: body = (SDL_Color){200,200,200}; break;
    }

    /* Body */
    SDL_SetRenderDrawColor(r, body.r, body.g, body.b, 255);
    SDL_Rect rect = { (int)(cx - w/2), (int)(cy - h), (int)w, (int)h };
    SDL_RenderFillRect(r, &rect);

    /* Windshield */
    SDL_SetRenderDrawColor(r, 150, 200, 255, 255);
    float ws_h = h * 0.25f;
    SDL_Rect ws = { (int)(cx - w*0.35f), (int)(cy - h*0.75f),
                    (int)(w*0.7f), (int)ws_h };
    SDL_RenderFillRect(r, &ws);

    /* Wheels */
    SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
    float ww = w * 0.2f, wh = h * 0.2f;
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx - w/2 - ww*0.5f), (int)(cy - h*0.8f), (int)ww, (int)wh });
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx + w/2 - ww*0.5f), (int)(cy - h*0.8f), (int)ww, (int)wh });
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx - w/2 - ww*0.5f), (int)(cy - h*0.25f), (int)ww, (int)wh });
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx + w/2 - ww*0.5f), (int)(cy - h*0.25f), (int)ww, (int)wh });
}

/* ── Draw the player's car ───────────────────────────────────────────── */
static void draw_player_car(SDL_Renderer *r, float steer) {
    float cx = SCREEN_W * 0.5f;
    float cy = SCREEN_H - 20;
    float w  = 60, h = 100;

    /* Slight tilt based on steering */
    float tilt = steer * 15.0f;

    /* Shadow */
    SDL_SetRenderDrawColor(r, 20, 20, 20, 128);
    SDL_Rect shadow = { (int)(cx - w/2 + 5), (int)(cy - h + 5), (int)w, (int)h };
    SDL_RenderFillRect(r, &shadow);

    /* Body — sleek racer shape */
    SDL_SetRenderDrawColor(r, 255, 140, 0, 255);  /* orange NFS style */
    SDL_Rect body = { (int)(cx - w/2 + tilt*0.3f), (int)(cy - h), (int)w, (int)h };
    SDL_RenderFillRect(r, &body);

    /* Hood detail */
    SDL_SetRenderDrawColor(r, 200, 100, 0, 255);
    SDL_Rect hood = { (int)(cx - w*0.35f + tilt*0.3f), (int)(cy - h*0.55f), (int)(w*0.7f), (int)(h*0.3f) };
    SDL_RenderFillRect(r, &hood);

    /* Windshield */
    SDL_SetRenderDrawColor(r, 100, 180, 255, 255);
    SDL_Rect ws = { (int)(cx - w*0.3f + tilt*0.2f), (int)(cy - h*0.72f), (int)(w*0.6f), (int)(h*0.18f) };
    SDL_RenderFillRect(r, &ws);

    /* Racing stripe */
    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx - 4 + tilt*0.3f), (int)(cy - h), 8, (int)h });

    /* Wheels */
    SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
    float ww = w * 0.18f, wh = h * 0.18f;
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx - w/2 - ww*0.6f + tilt*0.4f), (int)(cy - h*0.85f), (int)ww, (int)wh });
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx + w/2 - ww*0.4f + tilt*0.4f), (int)(cy - h*0.85f), (int)ww, (int)wh });
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx - w/2 - ww*0.6f), (int)(cy - h*0.2f), (int)ww, (int)wh });
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx + w/2 - ww*0.4f), (int)(cy - h*0.2f), (int)ww, (int)wh });

    /* Tail lights */
    SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx - w*0.4f), (int)(cy - 6), 8, 5 });
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(cx + w*0.4f - 8), (int)(cy - 6), 8, 5 });
}

/* ── Draw scenery (trees / posts) ────────────────────────────────────── */
static void draw_tree(SDL_Renderer *r, float x, float y, float scale) {
    float tw = 12 * scale, th = 40 * scale;
    float lw = 50 * scale, lh = 60 * scale;
    if (th < 2 || lh < 2) return;

    /* Trunk */
    SDL_SetRenderDrawColor(r, TREE_TRUNK.r, TREE_TRUNK.g, TREE_TRUNK.b, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(x - tw/2), (int)(y - th), (int)tw, (int)th });

    /* Canopy */
    SDL_SetRenderDrawColor(r, TREE_LEAVES.r, TREE_LEAVES.g, TREE_LEAVES.b, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(x - lw/2), (int)(y - th - lh), (int)lw, (int)lh });
}

static void draw_post(SDL_Renderer *r, float x, float y, float scale) {
    float pw = 6 * scale, ph = 80 * scale;
    if (ph < 2) return;
    SDL_SetRenderDrawColor(r, 180, 180, 180, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(x - pw/2), (int)(y - ph), (int)pw, (int)ph });
    SDL_SetRenderDrawColor(r, 255, 50, 50, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ (int)(x - pw), (int)(y - ph), (int)(pw*2), (int)(8*scale) });
}

/* ── HUD drawing ─────────────────────────────────────────────────────── */
static void draw_hud(SDL_Renderer *r, float spd, int current_lap, float lap_t, float best, float total) {
    char buf[128];

    /* Background bar */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
    SDL_Rect hud_bg = { 0, 0, SCREEN_W, 50 };
    SDL_RenderFillRect(r, &hud_bg);

    /* Speed */
    int kph = (int)(spd / MAX_SPEED * 320);
    snprintf(buf, sizeof(buf), "SPEED: %3d km/h", kph);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    /* We'll draw text as simple shapes — use a small helper below */

    /* Speed bar */
    float bar_w = 200.0f * (spd / MAX_SPEED);
    SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ 20, 15, (int)bar_w, 20 });
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawRect(r, &(SDL_Rect){ 20, 15, 200, 20 });

    /* Lap info */
    snprintf(buf, sizeof(buf), "LAP %d/%d", current_lap, TRACK_LAPS);
    /* Position indicator */
    snprintf(buf, sizeof(buf), "BEST: %.2fs", best);

    /* Tachometer-style RPM indicator */
    float rpm_pct = spd / MAX_SPEED;
    SDL_SetRenderDrawColor(r, 255, 50, 50, 255);
    if (rpm_pct > 0.8f) {
        SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
    } else if (rpm_pct > 0.6f) {
        SDL_SetRenderDrawColor(r, 255, 200, 0, 255);
    }
    SDL_RenderFillRect(r, &(SDL_Rect){ 240, 15, (int)(100 * rpm_pct), 20 });
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawRect(r, &(SDL_Rect){ 240, 15, 100, 20 });
}

/* ── Draw text using SDL2 built-in ───────────────────────────────────── */
static SDL_Texture* make_text_texture(SDL_Renderer *r, const char *text,
                                       SDL_Color fg, int size) {
    /* We use a simple bitmap approach — draw characters as rectangles */
    /* For proper text we'd need SDL_ttf, but let's keep dependencies minimal */
    return NULL;  /* Placeholder — we'll draw HUD with shapes only */
}

static void draw_text_simple(SDL_Renderer *r, const char *text,
                              int x, int y, SDL_Color col, int scale) {
    /* 3x5 pixel font — each char is 3 wide, 5 tall */
    static const uint8_t font[128][5] = {
        ['0']={0xe,0x11,0x13,0x15,0xe}, ['1']={0x4,0xc,0x4,0x4,0xe},
        ['2']={0xe,0x11,0x2,0x4,0x1f}, ['3']={0xe,0x11,0x6,0x11,0xe},
        ['4']={0x12,0x14,0x1f,0x10,0x10},['5']={0x1f,0x1,0x1f,0x10,0x1f},
        ['6']={0xe,0x1,0x1f,0x11,0xe}, ['7']={0x1f,0x10,0x8,0x4,0x4},
        ['8']={0xe,0x11,0xe,0x11,0xe}, ['9']={0xe,0x11,0x1e,0x10,0xe},
        ['A']={0xe,0x11,0x1f,0x11,0x11},['B']={0x1e,0x11,0x1e,0x11,0x1e},
        ['C']={0xe,0x11,0x1,0x11,0xe}, ['D']={0x1e,0x11,0x11,0x11,0x1e},
        ['E']={0x1f,0x1,0x1f,0x1,0x1f},['F']={0x1f,0x1,0x1f,0x1,0x1},
        ['G']={0xe,0x1,0x17,0x11,0xe}, ['H']={0x11,0x11,0x1f,0x11,0x11},
        ['I']={0xe,0x4,0x4,0x4,0xe},  ['K']={0x11,0x12,0x1c,0x12,0x11},
        ['L']={0x1,0x1,0x1,0x1,0x1f}, ['M']={0x11,0x1b,0x15,0x11,0x11},
        ['N']={0x11,0x19,0x15,0x13,0x11},['O']={0xe,0x11,0x11,0x11,0xe},
        ['P']={0x1f,0x11,0x1f,0x1,0x1},['R']={0x1e,0x11,0x1e,0x12,0x11},
        ['S']={0xe,0x1,0xe,0x10,0xe}, ['T']={0x1f,0x4,0x4,0x4,0x4},
        ['U']={0x11,0x11,0x11,0x11,0xe},['V']={0x11,0x11,0x11,0xa,0x4},
        ['W']={0x11,0x11,0x15,0x1b,0x11},['X']={0x11,0xa,0x4,0xa,0x11},
        ['Y']={0x11,0xa,0x4,0x4,0x4}, ['Z']={0x1f,0x10,0x8,0x4,0x1f},
        [':']={0x0,0x4,0x0,0x4,0x0},  ['/']={0x10,0x8,0x4,0x2,0x1},
        [' ']={0x0,0x0,0x0,0x0,0x0},  ['.']={0x0,0x0,0x0,0x0,0x4},
        ['-']={0x0,0x0,0xe,0x0,0x0},  ['s']={0x0,0xe,0x1,0x1e,0xe},
    };
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, 255);
    for (const char *c = text; *c; c++) {
        int idx = (unsigned char)*c;
        if (idx < 0 || idx >= 128 || font[idx][0] == 0 && *c != ' ' && *c != '0') {
            x += 4 * scale;
            continue;
        }
        for (int row = 0; row < 5; row++) {
            uint8_t bits = font[idx][row];
            for (int col = 0; col < 4; col++) {
                if (bits & (0x10 >> col)) {
                    SDL_RenderFillRect(r, &(SDL_Rect){
                        x + col * scale,
                        y + row * scale,
                        scale, scale
                    });
                }
            }
        }
        x += 4 * scale;
    }
}

/* ── Draw sky gradient ───────────────────────────────────────────────── */
static void draw_sky(SDL_Renderer *r) {
    SDL_Color top = SKY_TOP;
    SDL_Color bot = SKY_BOT;
    for (int y = 0; y < SCREEN_H / 2; y++) {
        float t = (float)y / (SCREEN_H / 2);
        SDL_Color c = {
            (uint8_t)(top.r + (bot.r - top.r) * t),
            (uint8_t)(top.g + (bot.g - top.g) * t),
            (uint8_t)(top.b + (bot.b - top.b) * t)
        };
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        SDL_RenderDrawLine(r, 0, y, SCREEN_W, y);
    }
}

/* ── Draw mountains ──────────────────────────────────────────────────── */
static void draw_mountains(SDL_Renderer *r) {
    SDL_Color mc = MOUNT_COL;
    SDL_SetRenderDrawColor(r, mc.r, mc.g, mc.b, 255);
    int base = SCREEN_H / 2;
    for (int x = 0; x < SCREEN_W; x++) {
        float h1 = sinf(x * 0.005f) * 60;
        float h2 = sinf(x * 0.012f + 2.0f) * 30;
        float h3 = sinf(x * 0.003f + 5.0f) * 40;
        int peak = base - (int)(h1 + h2 + h3) - 30;
        SDL_RenderDrawLine(r, x, peak, x, base);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  MAIN RENDER — the raycaster core
 * ═══════════════════════════════════════════════════════════════════════ */
static void render(SDL_Renderer *r) {
    float track_len = TRACK_SEGS * SEG_LEN;

    /* Camera */
    int   base_seg  = (int)(player_z / SEG_LEN) % TRACK_SEGS;
    float base_pct  = fmodf(player_z, SEG_LEN) / SEG_LEN;
    float cam_z     = player_z;
    float cam_y     = track[base_seg].y + (track[(base_seg+1)%TRACK_SEGS].y - track[base_seg].y) * base_pct;

    /* Accumulated curve for parallax */
    float x_offset = 0;
    float dx       = 0;

    /* ── Sky ──────────────────────────────────────────────────────────── */
    draw_sky(r);
    draw_mountains(r);

    /* ── Project all visible segments ─────────────────────────────────── */
    for (int n = 0; n <= DRAW_DIST; n++) {
        int idx = (base_seg + n) % TRACK_SEGS;
        float seg_z = (base_seg + n) * SEG_LEN;
        if (seg_z < cam_z) seg_z += track_len;

        float seg_y = track[idx].y;
        project_segment(&proj[n], player_x * ROAD_W * 0.5f, cam_y, cam_z, seg_z, seg_y);

        /* Apply curve offset */
        proj[n].screen_x += x_offset;
        x_offset += dx;
        dx += track[idx].curve * 0.015f;

        /* Clip */
        proj[n].clip_y = SCREEN_H;
    }

    /* ── Draw road from far to near ───────────────────────────────────── */
    for (int n = DRAW_DIST - 1; n > 0; n--) {
        ProjSeg *p1 = &proj[n];     /* far */
        ProjSeg *p2 = &proj[n - 1]; /* near */

        if (p1->screen_y >= p2->clip_y && p2->screen_y >= p1->clip_y) continue;

        int idx = (base_seg + n) % TRACK_SEGS;
        int alt = ((base_seg + n) / 3) % 2;

        /* Grass */
        SDL_Color gc = alt ? GRASS_LIGHT : GRASS_DARK;
        draw_quad(r, gc,
                  0, p1->screen_y, SCREEN_W,
                  0, p2->screen_y, SCREEN_W);

        /* Road */
        SDL_Color rc = alt ? ROAD_LIGHT : ROAD_DARK;
        draw_quad(r, rc,
                  p1->screen_x, p1->screen_y, p1->screen_w,
                  p2->screen_x, p2->screen_y, p2->screen_w);

        /* Rumble strips */
        SDL_Color rumc = alt ? RUMBLE_LIGHT : RUMBLE_DARK;
        float rw1 = p1->screen_w * 1.15f, rw2 = p2->screen_w * 1.15f;
        draw_quad(r, rumc,
                  p1->screen_x, p1->screen_y, rw1,
                  p2->screen_x, p2->screen_y, rw2);
        /* Re-draw road on top of rumble inner */
        draw_quad(r, rc,
                  p1->screen_x, p1->screen_y, p1->screen_w,
                  p2->screen_x, p2->screen_y, p2->screen_w);

        /* Lane markings */
        if (alt) {
            float lw1 = p1->screen_w * 0.01f;
            float lw2 = p2->screen_w * 0.01f;
            for (int lane = -1; lane <= 1; lane++) {
                float off1 = p1->screen_w * lane * 0.33f;
                float off2 = p2->screen_w * lane * 0.33f;
                draw_quad(r, LANE_MARK,
                          p1->screen_x + off1, p1->screen_y, lw1,
                          p2->screen_x + off2, p2->screen_y, lw2);
            }
        }

        /* Centre line */
        if (!alt) {
            float cw1 = p1->screen_w * 0.015f;
            float cw2 = p2->screen_w * 0.015f;
            draw_quad(r, LANE_MARK,
                      p1->screen_x, p1->screen_y, cw1,
                      p2->screen_x, p2->screen_y, cw2);
        }

        /* ── Scenery ──────────────────────────────────────────────────── */
        if (n % 8 == 0) {
            /* Trees on both sides */
            float tree_x_left  = p2->screen_x - p2->screen_w * 1.8f;
            float tree_x_right = p2->screen_x + p2->screen_w * 1.8f;
            float tree_scale   = p2->scale * 800.0f;
            draw_tree(r, tree_x_left,  p2->screen_y, tree_scale);
            draw_tree(r, tree_x_right, p2->screen_y, tree_scale);
        }

        /* Roadside posts every 4 segments */
        if (n % 4 == 0) {
            float post_scale = p2->scale * 600.0f;
            draw_post(r, p2->screen_x - p2->screen_w * 1.2f, p2->screen_y, post_scale);
            draw_post(r, p2->screen_x + p2->screen_w * 1.2f, p2->screen_y, post_scale);
        }

        /* Update clip */
        if (p2->screen_y < proj[n-1].clip_y) {
            proj[n-1].clip_y = p2->screen_y;
        }
    }

    /* ── Rival cars ───────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_RIVALS; i++) {
        float rel_z = rivals[i].z - player_z;
        if (rel_z < 0) rel_z += track_len;
        if (rel_z > DRAW_DIST * SEG_LEN || rel_z < SEG_LEN) continue;

        int seg_n = (int)(rel_z / SEG_LEN);
        if (seg_n >= DRAW_DIST || seg_n < 1) continue;

        ProjSeg *ps = &proj[seg_n];
        float car_x = ps->screen_x + rivals[i].x * ps->screen_w;
        float car_scale = ps->scale * 800.0f;
        draw_car(r, car_x, ps->screen_y, car_scale, rivals[i].colour);
    }

    /* ── Player car ───────────────────────────────────────────────────── */
    draw_player_car(r, player_x);

    /* ── HUD ──────────────────────────────────────────────────────────── */
    SDL_Color white = {255, 255, 255};
    SDL_Color yellow = {255, 220, 0};
    SDL_Color red = {255, 50, 50};
    SDL_Color green = {50, 255, 50};

    /* HUD background */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_RenderFillRect(r, &(SDL_Rect){0, 0, SCREEN_W, 55});

    /* Speed display */
    int kph = (int)(speed / MAX_SPEED * 320);
    char speed_str[32];
    snprintf(speed_str, sizeof(speed_str), "%3d KPH", kph);
    draw_text_simple(r, speed_str, 20, 8, white, 3);

    /* Speed bar */
    float bar_pct = speed / MAX_SPEED;
    SDL_Color bar_col = bar_pct > 0.8f ? red : bar_pct > 0.6f ? yellow : green;
    SDL_SetRenderDrawColor(r, bar_col.r, bar_col.g, bar_col.b, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){20, 35, (int)(200 * bar_pct), 12});
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawRect(r, &(SDL_Rect){20, 35, 200, 12});

    /* Lap counter */
    char lap_str[32];
    snprintf(lap_str, sizeof(lap_str), "LAP %d/%d", lap > TRACK_LAPS ? TRACK_LAPS : lap, TRACK_LAPS);
    draw_text_simple(r, lap_str, 400, 8, yellow, 3);

    /* Time */
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "TIME %.1f", total_time);
    draw_text_simple(r, time_str, 400, 32, white, 2);

    /* Best lap */
    char best_str[32];
    if (best_lap < 999.0f) {
        snprintf(best_str, sizeof(best_str), "BEST %.2f", best_lap);
    } else {
        snprintf(best_str, sizeof(best_str), "BEST --.--");
    }
    draw_text_simple(r, best_str, 600, 8, green, 2);

    /* Gear indicator */
    int gear = 1 + (int)(speed / MAX_SPEED * 5);
    if (gear > 6) gear = 6;
    char gear_str[8];
    snprintf(gear_str, sizeof(gear_str), "G%d", gear);
    draw_text_simple(r, gear_str, 600, 32, white, 3);

    /* ── Countdown / Game Over overlay ────────────────────────────────── */
    if (countdown > 0) {
        char cd_str[8];
        snprintf(cd_str, sizeof(cd_str), "%d", countdown);
        draw_text_simple(r, cd_str, SCREEN_W/2 - 20, SCREEN_H/2 - 40, red, 10);
    } else if (countdown == 0 && countdown_t < 1.0f) {
        draw_text_simple(r, "GO", SCREEN_W/2 - 30, SCREEN_H/2 - 30, green, 10);
    }

    if (game_over) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
        SDL_RenderFillRect(r, &(SDL_Rect){SCREEN_W/4, SCREEN_H/3, SCREEN_W/2, SCREEN_H/3});
        draw_text_simple(r, "RACE COMPLETE", SCREEN_W/2 - 180, SCREEN_H/2 - 40, yellow, 4);
        char final_str[64];
        snprintf(final_str, sizeof(final_str), "TOTAL %.2f", total_time);
        draw_text_simple(r, final_str, SCREEN_W/2 - 120, SCREEN_H/2 + 20, white, 3);
        draw_text_simple(r, "PRESS R TO RESTART", SCREEN_W/2 - 180, SCREEN_H/2 + 60, white, 2);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  UPDATE — physics, AI, collision
 * ═══════════════════════════════════════════════════════════════════════ */
static void update(float dt) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float track_len = TRACK_SEGS * SEG_LEN;

    /* Countdown */
    if (countdown > 0) {
        countdown_t += dt;
        if (countdown_t >= 1.0f) {
            countdown_t = 0.0f;
            countdown--;
        }
        return;
    }
    countdown_t += dt;  /* for "GO" display */

    if (game_over) {
        if (keys[SDL_SCANCODE_R]) {
            /* Restart */
            player_x = 0; player_z = 0; speed = 0;
            lap = 1; lap_timer = 0; total_time = 0; best_lap = 999.0f;
            countdown = 3; countdown_t = 0; game_over = 0;
            init_rivals();
        }
        return;
    }

    /* Steering */
    float steer = 0;
    if (keys[SDL_SCANCODE_LEFT])  steer = -1.0f;
    if (keys[SDL_SCANCODE_RIGHT]) steer =  1.0f;

    /* Acceleration / braking */
    if (keys[SDL_SCANCODE_UP])   speed += ACCEL * dt;
    else if (keys[SDL_SCANCODE_DOWN]) speed -= BRAKE * dt;
    else                           speed -= DECEL * dt;

    /* Off-road penalty */
    if (fabsf(player_x) > 1.0f) {
        speed -= OFF_ROAD_DECEL * dt;
    }

    speed = clampf(speed, 0, MAX_SPEED);

    /* Centrifugal force — pushed to the outside of curves */
    int seg_idx = (int)(player_z / SEG_LEN) % TRACK_SEGS;
    float curve = track[seg_idx].curve;
    player_x += curve * CENTRIFUGAL * (speed / MAX_SPEED) * dt * STEER_SPEED;

    /* Apply steering */
    player_x += steer * STEER_SPEED * (speed / MAX_SPEED) * dt;

    /* Move forward */
    player_z += speed * dt;

    /* Lap detection */
    float prev_z = player_z - speed * dt;
    if (player_z >= track_len && prev_z < track_len) {
        if (lap_timer < best_lap) best_lap = lap_timer;
        lap_timer = 0;
        lap++;
        if (lap > TRACK_LAPS) {
            game_over = 1;
        }
    }
    player_z = fmodf(player_z, track_len);

    lap_timer  += dt;
    total_time += dt;

    /* ── Rival AI ─────────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_RIVALS; i++) {
        rivals[i].z += rivals[i].speed * dt;
        if (rivals[i].z >= track_len) rivals[i].z -= track_len;

        /* Gentle lateral weave */
        int rseg = (int)(rivals[i].z / SEG_LEN) % TRACK_SEGS;
        rivals[i].x += track[rseg].curve * 0.002f;
        rivals[i].x = clampf(rivals[i].x, -0.8f, 0.8f);

        /* Collision with player */
        float rel_z = rivals[i].z - player_z;
        if (rel_z < 0) rel_z += track_len;
        if (rel_z < SEG_LEN * 0.5f && fabsf(rivals[i].x - player_x) < 0.4f) {
            speed *= 0.5f;  /* slow down on hit */
            /* Push player sideways */
            player_x += (player_x - rivals[i].x) * 0.5f;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        "NFS Raycaster — Pseudo-3D Racing",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        return 1;
    }

    build_track();
    init_rivals();
    lap = 1;
    prev_tick = SDL_GetPerformanceCounter();

    printf("╔══════════════════════════════════════╗\n");
    printf("║     NFS RAYCASTER — Let's Race!      ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║  UP/DOWN   — Accelerate / Brake      ║\n");
    printf("║  LEFT/RIGHT— Steer                   ║\n");
    printf("║  ESC       — Quit                    ║\n");
    printf("║  R         — Restart (after finish)   ║\n");
    printf("╚══════════════════════════════════════╝\n");

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - prev_tick) / SDL_GetPerformanceFrequency();
        prev_tick = now;
        if (dt > 0.05f) dt = 0.05f;  /* cap delta time */

        update(dt);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        render(renderer);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Thanks for racing!\n");
    return 0;
}
