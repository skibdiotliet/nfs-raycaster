# 🏎️ NFS Raycaster — Pseudo-3D Racing Game in C

A **Need for Speed-style pseudo-3D racing game** built from scratch in pure C using the classic segment-projection raycasting technique. No game engines, no frameworks — just C and SDL2.

![Platform](https://img.shields.io/badge/platform-Linux%20%2F%20WSL-green)
![Language](https://img.shields.io/badge/language-C11-blue)
![License](https://img.shields.io/badge/license-MIT-yellow)

---

## 🎮 Features

- **Pseudo-3D raycaster engine** — OutRun/NFS-style road rendering using segment projection
- **Curves & hills** — sin-wave-based track with elevation changes and banking
- **12 rival cars** with basic AI, collision detection, and speed variation
- **Full HUD** — speedometer, gear indicator, lap counter, best lap time
- **3-2-1 countdown** and race completion screen
- **Off-road penalty** — driving on the grass slows you down
- **Centrifugal force** — curves push you toward the outside
- **Procedural scenery** — trees, roadside posts, mountains, gradient sky
- **Bitmap text renderer** — no SDL_ttf dependency, custom 3×5 pixel font
- **Zero external assets** — everything is drawn in code

---

## 📸 How It Works

This game uses the **segment-projection raycasting** technique, the same method used in classic arcade racers like **OutRun (1986)**, **Pole Position (1982)**, and early **Need for Speed** titles.

### The Raycasting Principle

```
   Player (Camera)
        \
         \  ← ray for each scanline
          \
           \
            → Road Segment (projected)
```

Instead of casting rays horizontally like Wolfenstein 3D, a racing raycaster works **depth-wise**:

1. **The track** is a chain of segments, each with a curve value and elevation
2. **Each frame**, we project 300 segments from the camera position into screen space
3. **Segments are drawn back-to-front** (painter's algorithm) — far segments first
4. **Curve accumulation** shifts each segment laterally, creating the illusion of a curved road
5. **Elevation values** shift segments vertically, creating hills and valleys

The result is a convincing 3D perspective from a 2D data structure — pure math, no GPU 3D pipeline needed.

---

## 🛠️ Installation & Build

### Prerequisites (Ubuntu / WSL)

```bash
# Install SDL2 development libraries
sudo apt update
sudo apt install -y libsdl2-dev build-essential
```

### Build & Run

```bash
# Clone the repository
git clone https://github.com/<your-username>/nfs-raycaster.git
cd nfs-raycaster

# Build
make

# Run
./nfs_raycaster
```

### WSL Display Setup

If you're running on **WSL1**, you need an X server:

```bash
# Install VcXsrv or X410 on Windows, then:
export DISPLAY=$(ip route show default | awk '{print $3}'):0.0

# For WSL2 with WSLg (Windows 11), it should just work
```

If you're on **WSL2 with WSLg** (default on Windows 11), SDL2 windows should appear automatically — no configuration needed.

---

## 🕹️ Controls

| Key | Action |
|-----|--------|
| `↑` UP | Accelerate |
| `↓` DOWN | Brake / Reverse |
| `←` LEFT | Steer Left |
| `→` RIGHT | Steer Right |
| `ESC` | Quit |
| `R` | Restart (after race complete) |

---

## 🏗️ Architecture Deep-Dive

### File Structure

```
nfs-raycaster/
├── src/
│   └── main.c          # Complete game source (~600 lines)
├── Makefile             # Build system
└── README.md            # This file
```

### Core Data Structures

#### Segment Definition (`SegDef`)
```c
typedef struct {
    float curve;    // Curvature: positive = right, negative = left
    float y;        // Elevation at segment start
} SegDef;
```

Each segment is 200 world units long. The track is 1600 segments — about 3.2 km per lap.

#### Projected Segment (`ProjSeg`)
```c
typedef struct {
    float world_z;     // Z position in world space
    float scale;       // Perspective scale factor (1/distance)
    float screen_x;    // Projected X on screen
    float screen_y;    // Projected Y on screen
    float screen_w;    // Projected road half-width
    float clip_y;      // Y-coordinate for occlusion clipping
} ProjSeg;
```

The projection math:
```
scale    = CAMERA_DEPTH / distance
screen_x = SCREEN_CENTER + scale * camera_offset
screen_y = SCREEN_CENTER - scale * (elevation - camera_height)
screen_w = scale * ROAD_WIDTH
```

### Rendering Pipeline

```
1. Build track (once)          →  Generate curve/elevation from sin waves
2. Project segments (each frame)→  Camera → screen space transform
3. Accumulate curves           →  Parallax offset for each segment
4. Draw back-to-front          →  Painter's algorithm
   a. Grass (full-width quad)
   b. Road (narrower quad)
   c. Rumble strips
   d. Lane markings
   e. Centre line
   f. Scenery (trees, posts)
5. Draw rival cars             →  Position on projected segments
6. Draw player car             →  Fixed screen position
7. Draw HUD                    →  Speed, laps, time
```

### Track Generation

The track is generated procedurally using overlapping sine waves:

```c
// Gentle right curve
if (p > 0.02 && p < 0.12) curve = 2.0 * sin((p-0.02)/0.10 * PI);

// Sharp left bend
if (p > 0.18 && p < 0.30) curve = -3.5 * sin((p-0.18)/0.12 * PI);

// Steep hill
if (p > 0.45 && p < 0.55) hill = 3000.0 * sin((p-0.45)/0.10 * PI);
```

This creates a track with 5 distinct curve sections and 5 elevation changes, giving each lap a unique feel as you go from sweeping bends to tight hairpins and over rolling hills.

### The Curve Illusion

The most important trick in the renderer is **curve accumulation**:

```c
x_offset += dx;
dx += track[idx].curve * 0.015;
```

Each segment's curve value adds a small delta to `dx`, and `dx` itself accumulates into `x_offset`. This means segments farther away get progressively more lateral offset, making the road appear to curve naturally. The curve strength is subtle for near segments and dramatic for far ones — exactly how perspective works.

### Custom Bitmap Font

The game includes a built-in 3×5 pixel font for all HUD text:

```c
static const uint8_t font[128][5] = {
    ['A']={0xe,0x11,0x1f,0x11,0x11},
    // Each number is a 4-bit row: bit 3=leftmost pixel
};
```

Each character is defined as 5 rows of 4-bit values, where each bit represents one pixel. This eliminates the need for SDL_ttf and keeps the dependency count at just SDL2.

---

## 🎯 Modding Guide

### Change the Track

Edit `build_track()` in `src/main.c`:

```c
// Add a crazy chicane (S-curve)
if (p > 0.50 && p < 0.53) curve = 6.0 * sin(...);  // sharp right
if (p > 0.53 && p < 0.56) curve = -6.0 * sin(...); // sharp left
```

### Adjust Physics

At the top of `main.c`:

```c
#define MAX_SPEED     (SEG_LEN * 60)   // Increase for faster top speed
#define ACCEL         (MAX_SPEED / 3)   // Higher = quicker acceleration
#define CENTRIFUGAL   0.3               // Higher = harder to hold curves
#define STEER_SPEED   3.0               // Higher = more responsive steering
```

### Add More Rivals

```c
#define MAX_RIVALS  20   // More traffic!
```

### Change Resolution

```c
#define SCREEN_W  1920
#define SCREEN_H  1080
```

---

## 🔧 Troubleshooting

### "SDL init failed"
```bash
sudo apt install libsdl2-dev
```

### "Could not create window" (WSL)
Make sure you have a display server:
- **WSL1**: Install VcXsrv on Windows, set `DISPLAY` environment variable
- **WSL2 + Windows 11**: WSLg should handle this automatically
- **WSL2 + Windows 10**: You may need to install an X server

```bash
# Test if X11 is working
xdpyinfo 2>/dev/null && echo "X11 OK" || echo "No display"
```

### Slow framerate
- Make sure you have `SDL_RENDERER_ACCELERATED` support
- Try reducing `DRAW_DIST` from 300 to 200
- On WSL, software rendering may be used — this is normal

### Segfault on start
- Ensure `libsdl2-dev` is installed (not just `libsdl2`)
- Run with `GALLIUM_HUD=fps ./nfs_raycaster` to debug rendering

---

## 📚 Further Reading

If you want to understand pseudo-3D racing game development in depth:

- **[Lou's Pseudo 3D Page](http://www.extentofthejam.com/pseudo/)** — The definitive reference for OutRun-style rendering
- **[Code the Classics](https://wireframe.raspberrypi.com/books/code-the-classics-vol1)** — Raspberry Pi Press book with a Python racer
- **[OutRun Source Code](https://github.com/brenns10/outrun)** — C++ reimplementation with detailed comments
- **[Jake Gordon's JavaScript Racer](https://github.com/jakesgordon/javascript-racer)** — Step-by-step tutorial in JavaScript

---

## 📝 License

MIT License — use this code for anything you want. Learn from it, mod it, build your own racer on top of it.

---

## 🏁 Credits

Built with pure C and SDL2. No game engines were harmed in the making of this racer.

**Now hit the road!**
