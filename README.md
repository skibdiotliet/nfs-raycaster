# Raycaster — True 3D Terminal Raycaster in C

A **true 3D raycaster** that runs entirely in your terminal. No SDL2, no X server, no display server, no external dependencies. Just `make` and `./raycaster`.

Wolfenstein-style DDA raycasting algorithm rendered as ASCII characters with ANSI colour.

---

## What It Does

- **DDA raycasting** — the real Wolfenstein 3D algorithm
- **ASCII rendering** — walls rendered as `@#%x+=:-.` based on distance
- **ANSI colour** — red brick, green stone, blue metal walls with dark/light shading
- **Minimap** — top-right corner shows the map and your position
- **Floor & ceiling** — checkerboard floor pattern, dark ceiling
- **EW/NS wall shading** — side-facing walls are darker than front-facing walls
- **Distance fog** — walls fade as they get further away
- **Zero dependencies** — just your terminal

---

## Build & Run

```bash
git clone https://github.com/skibdiotliet/nfs-raycaster.git
cd nfs-raycaster
make
./raycaster
```

That's it. No `apt install`, no display server, no nothing.

---

## Controls

| Key | Action |
|-----|--------|
| `W` / `↑` | Move forward |
| `S` / `↓` | Move backward |
| `A` / `←` | Rotate left |
| `D` / `→` | Rotate right |
| `Q` | Quit |

---

## How It Works

### DDA Raycasting

For every column of the terminal screen, a ray is cast from the player position through the 2D grid. The DDA (Digital Differential Analyser) algorithm steps through grid cells until it hits a wall. The perpendicular distance determines the wall height on screen.

```
Close wall:  ████████████   (@#% characters)
Mid wall:    ....####....   (x+=: characters)
Far wall:    ..  ....  ..   (:-. characters)
```

### ASCII Shading

Characters are chosen by distance:

```
@  #  %  x  +  =  :  -  .  (space)
← close                    far →
```

EW-facing walls use one shade darker than NS-facing walls, same as Wolfenstein 3D.

### The Map

24×24 grid defined in `world_map`:

```
0 = empty space
1 = red brick wall
2 = green stone wall
3 = blue metal wall
```

Edit it however you want.

---

## Modifying

### Change the map
Edit the `world_map` array in `src/main.c`.

### Change render size
```c
#define SCR_W 120
#define SCR_H 40
```
Match it to your terminal size. Bigger = more detail but slower.

### Change FOV
```c
static double plane_y = 0.66;  // increase = wider FOV
```

### Change movement speed
```c
#define MOVE_SPEED  3.0
#define ROT_SPEED   2.0
```

### Change wall characters
```c
static const char wall_shade[] = "@#%x+=:-. ";
```
Swap in any ASCII characters you want.

---

## Architecture

```
nfs-raycaster/
├── src/
│   └── main.c       # Entire raycaster (~350 lines, zero dependencies)
├── Makefile          # Build (no SDL2 needed)
└── README.md
```

| Function | What it does |
|----------|-------------|
| `cast_ray(col)` | DDA raycast for screen column, returns distance + wall info |
| `render()` | Casts all rays, fills screen buffer, draws to terminal with ANSI |
| `draw_minimap()` | Overlays map in top-right corner |
| `term_raw()` / `term_restore()` | Terminal raw mode for non-blocking input |

---

## References

- [Lode's Raycasting Tutorial](https://lodev.org/cgtutor/raycasting.html)
- [DDA Algorithm](https://en.wikipedia.org/wiki/Digital_differential_analyzer_(graphics_algorithm))
- [Wolfenstein 3D Source](https://github.com/id-Software/wolf3d)

---

## License

MIT
