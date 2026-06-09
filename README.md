# Raycaster — True 3D Raycasting Engine in C

A **true 3D raycaster** built from scratch in pure C with SDL2. Wolfenstein-style DDA algorithm — cast rays through a 2D grid, get wall distances, draw columns. That's the engine.

No game logic. No physics. No AI. Just the raycaster.

---

## What It Does

- **DDA raycasting** — Digital Differential Analyser, the real algorithm Wolfenstein 3D uses
- **Textured walls** — procedurally generated brick, stone, metal, concrete textures
- **Floor & ceiling casting** — real floor raycasting with checkerboard tiles
- **Sprite rendering** — billboarded sprites with Z-buffer occlusion
- **Distance fog** — walls and floor fade with distance
- **Wall shading** — EW-facing walls are darker than NS-facing walls
- **Minimap** — top-left overlay showing the grid and player position
- **Framebuffer rendering** — direct pixel buffer blitted to SDL2 texture

---

## How Raycasting Works

### The Core Idea

You're in a 2D grid. Each cell is either empty or a wall. For every vertical column of pixels on screen, you cast a single ray from the player's position and walk it through the grid until it hits a wall. The distance tells you how tall to draw that wall strip.

```
        Screen
     ┌──────────────┐
     │  █           │  ← Close wall = tall strip
     │  ██          │
     │  ███         │
     │  ████        │
     │  ██████      │
     │  ████████    │
     │  ██████████  │  ← Far wall = short strip
     └──────────────┘

     Each column = 1 ray
```

### DDA Algorithm (per ray)

1. **Calculate ray direction** from player direction + camera plane
2. **Find which grid edge the ray hits first** (side_dist_x vs side_dist_y)
3. **Step through the grid** — always jump to the nearest next grid line
4. **Stop when you hit a wall** — record which side was hit (NS or EW)
5. **Calculate perpendicular distance** — this avoids the fisheye effect
6. **Draw the wall strip** — height = screen_height / distance

### Why Perpendicular Distance?

If you use raw Euclidean distance, walls look curved (fisheye). Using perpendicular distance — the distance projected onto the camera direction — keeps walls flat:

```
perp_dist = (side_dist_x - delta_dist_x)   // for NS wall hit
perp_dist = (side_dist_y - delta_dist_y)   // for EW wall hit
```

### Camera Plane & FOV

The camera plane vector controls the field of view. It's always perpendicular to the direction vector:

```
dir   = (1.0, 0.0)    // looking east
plane = (0.0, 0.66)   // 66° FOV

ray = dir + plane * (2 * x / SCREEN_W - 1)
```

Changing `plane` magnitude changes FOV. `0.66` gives roughly 66 degrees.

---

## Build & Run

### Prerequisites (Ubuntu / WSL)

```bash
sudo apt install libsdl2-dev build-essential
```

### Build

```bash
git clone https://github.com/skibdiotliet/nfs-raycaster.git
cd nfs-raycaster
make
```

### Run

```bash
./raycaster
```

### WSL Display

WSL2 + WSLg (Windows 11) should just work. For older setups:

```bash
export DISPLAY=$(ip route show default | awk '{print $3}'):0.0
```

---

## Controls

| Key | Action |
|-----|--------|
| `W` / `↑` | Move forward |
| `S` / `↓` | Move backward |
| `A` / `←` | Rotate left |
| `D` / `→` | Rotate right |
| `ESC` | Quit |

---

## Code Architecture

```
nfs-raycaster/
├── src/
│   └── main.c       # Entire raycaster (~500 lines)
├── Makefile          # Build
└── README.md         # This
```

### Key Functions

| Function | What it does |
|----------|-------------|
| `cast_ray(x)` | DDA raycast for screen column `x` — wall + floor + ceiling |
| `cast_floor()` | Floor/ceiling raycasting for one column |
| `draw_sprites()` | Billboard sprite rendering with Z-buffer |
| `draw_minimap()` | Top-left map overlay |
| `gen_textures()` | Procedural wall texture generation |
| `gen_sprite_textures()` | Procedural sprite texture generation |

### The Map

Defined as a 24×24 integer grid in `world_map`:

```c
0 = empty space
1 = red brick wall
2 = green stone wall
3 = blue metal wall
```

Edit it however you want. The raycaster reads it directly.

---

## Modifying

### Change the map

Edit the `world_map` array. Values 1-3 are different wall types with different textures and colours.

### Add wall types

1. Add a new value to the map (e.g. `4`)
2. Add a colour case in `wall_colour()`
3. Add a texture pattern in `gen_textures()`

### Change FOV

```c
static double plane_y = 0.66;  // increase = wider FOV, decrease = narrower
```

### Change resolution

```c
#define SCREEN_W  1920
#define SCREEN_H  1080
```

### Add sprites

```c
static Sprite sprites[MAX_SPRITES] = {
    { 5.5, 5.5, 0 },   // x, y, type
    // add more...
};
```

---

## References

- [Lode's Raycasting Tutorial](https://lodev.org/cgtutor/raycasting.html) — the definitive reference
- [DDA Algorithm Explained](https://en.wikipedia.org/wiki/Digital_differential_analyzer_(graphics_algorithm))
- [Wolfenstein 3D Source](https://github.com/id-Software/wolf3d) — original id Software code

---

## License

MIT
