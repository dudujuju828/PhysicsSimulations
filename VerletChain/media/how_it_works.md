# How VerletChain Works

This document walks through the entire simulation — the physics behind it, how the code is structured, and what every function does. No prior knowledge of physics simulations is assumed.

## Table of Contents

- [The Big Idea](#the-big-idea)
- [What Is Verlet Integration?](#what-is-verlet-integration)
- [How the Chain Is Built](#how-the-chain-is-built)
- [The Simulation Loop](#the-simulation-loop)
  - [Step 1: Integration (Applying Physics)](#step-1-integration-applying-physics)
  - [Step 2: Constraint Solving (Keeping the Chain Together)](#step-2-constraint-solving-keeping-the-chain-together)
  - [Step 3: Syncing Positions for Rendering](#step-3-syncing-positions-for-rendering)
- [Mouse Interaction](#mouse-interaction)
- [Rendering (Drawing to the Screen)](#rendering-drawing-to-the-screen)
- [The Main Loop](#the-main-loop)
- [File-by-File Breakdown](#file-by-file-breakdown)
  - [vec2.h](#vec2h)
  - [chain.h / chain.cpp](#chainh--chaincpp)
  - [renderer.h / renderer.cpp](#rendererh--renderercpp)
  - [main.cpp](#maincpp)

---

## The Big Idea

Imagine a rope hanging from a nail. Gravity pulls it down, and if you grab part of it, the rest follows. This simulation recreates that behavior using a series of **particles** (points in 2D space) connected by **constraints** (invisible rigid links that keep them a fixed distance apart).

Each frame — roughly 60 times per second — the program:

1. Moves every particle according to gravity
2. Corrects the distances between connected particles so the chain doesn't stretch or collapse
3. Draws the result to the screen

---

## What Is Verlet Integration?

Most physics engines track each object's **position** and **velocity** as separate values. Verlet integration takes a different approach: it only stores the **current position** and the **previous position**. Velocity is never stored explicitly — it's implied by the difference between those two positions.

The update formula is:

```
new_position = current_position + (current_position - previous_position) + acceleration * dt^2
```

Breaking that down:

- `current_position - previous_position` is the implied velocity — how far the particle moved last frame. The particle will keep moving in the same direction by the same amount (inertia).
- `acceleration * dt^2` adds the effect of forces like gravity. `dt` is the time elapsed since the last frame, so multiplying by `dt^2` scales the acceleration correctly.
- The result is the new position. The old "current" becomes the new "previous" for next frame.

**Why use this instead of regular velocity?** Verlet integration makes constraint solving much simpler. When you directly adjust positions to satisfy constraints (like keeping two particles a fixed distance apart), the velocity automatically adapts on the next frame because it's derived from position history. With explicit velocity, you'd have to manually update both position *and* velocity every time you adjust a constraint, which is error-prone.

---

## How the Chain Is Built

When the program starts, it creates a chain by placing particles in a vertical line below an anchor point near the top of the window. Each particle is spaced `25 pixels` apart.

**`Chain::Chain` (constructor)** in `chain.cpp` does this:

1. Creates 20 particles in a vertical line. Particle 0 is at the anchor, particle 1 is 25px below it, particle 2 is 50px below, and so on.
2. Sets each particle's `prev_pos` equal to its `pos` — so it starts with zero velocity.
3. **Pins** particle 0 (the top one). A pinned particle is locked in place — it ignores gravity and can't be moved by the constraint solver. This is the "nail" the rope hangs from.
4. Creates a constraint between each consecutive pair: particle 0-1, 1-2, 2-3, and so on. Each constraint records the two particle indices and a rest length of 25px.

---

## The Simulation Loop

Every frame, `Chain::update` is called. It runs three steps in order:

### Step 1: Integration (Applying Physics)

**`Chain::integrate`** moves every non-pinned particle according to the Verlet formula.

```cpp
Vec2 accel = gravity * (dt * dt);       // pre-compute gravity * dt^2
for (auto& p : particles_) {
    if (p.pinned) continue;              // pinned particles don't move
    Vec2 vel = p.pos - p.prev_pos;       // implied velocity from last frame
    p.prev_pos = p.pos;                  // save current as previous
    p.pos = p.pos + vel + accel;         // move: inertia + gravity
}
```

After this step, particles have moved under gravity, but the distances between connected particles are probably wrong — some links have stretched, others have compressed. That's where constraints come in.

### Step 2: Constraint Solving (Keeping the Chain Together)

**`Chain::solve_constraints`** fixes the distances. For each constraint, it checks the actual distance between two connected particles and compares it to the desired rest length. If they're too far apart, it pushes them closer; if too close, it pushes them apart.

```cpp
Vec2 delta = particles_[b].pos - particles_[a].pos;  // vector from a to b
float dist = delta.length();                           // actual distance
float error = (dist - rest) / dist;                    // how far off (normalized)
Vec2 correction = delta * (0.5f * error);              // split correction in half

if (!particles_[a].pinned) particles_[a].pos += correction;  // push a toward b
if (!particles_[b].pinned) particles_[b].pos -= correction;  // push b toward a
```

The `0.5f` splits the correction evenly — each particle moves halfway toward the correct distance. If one particle is pinned, only the other one moves (it gets pushed the full correction implicitly, since the pinned particle's `+=` is skipped).

**Why iterate multiple times?** Fixing one constraint can break another. If you push particles 3 and 4 to the right distance, that might stretch the link between 4 and 5. By repeating the process multiple times (8 iterations in this simulation), the errors shrink until the chain looks correct. More iterations = stiffer, more accurate chain. Fewer iterations = softer, more elastic.

A guard (`if (dist < 1e-6f) continue`) skips constraints where two particles are at the exact same position, preventing a division-by-zero crash.

### Step 3: Syncing Positions for Rendering

**`Chain::sync_pos_cache`** copies all particle positions into a flat array of `Vec2` values. This array is what gets sent to the GPU for drawing. The simulation stores particles as `Particle` structs (with `pos`, `prev_pos`, and `pinned`), but the GPU only needs the `(x, y)` positions. This copy keeps the rendering code completely separate from the simulation internals.

---

## Mouse Interaction

The user can click and drag any particle. This involves three GLFW callbacks in `main.cpp`:

**`mouse_button_callback`** — When the left mouse button is pressed:
1. Converts the mouse position to simulation coordinates
2. Calls `Chain::find_nearest` to search all particles and find the closest one within 25 pixels
3. If a particle is found, records whether it was already pinned, then pins it and snaps it to the mouse position. Pinning it tells the constraint solver to treat it as an immovable anchor — the rest of the chain hangs from it.

When the button is released:
1. If the particle was not originally pinned (i.e., it's not the top anchor), it gets unpinned so it can resume free motion
2. The drag state is cleared

**`cursor_position_callback`** — While dragging, this fires every time the mouse moves. It converts GLFW's screen coordinates (where Y=0 is at the top) to simulation coordinates (where Y=0 is at the bottom) by flipping: `sim_y = window_height - glfw_y`. Then it calls `Chain::set_particle_pos` to move the dragged particle.

**`Chain::set_particle_pos`** sets both `pos` and `prev_pos` to the same value. This is critical — if only `pos` were updated, the Verlet integrator would see a huge difference between `pos` and `prev_pos` next frame and interpret it as a massive velocity, launching the particle when released. Setting both to the same value zeroes out the implied velocity during the drag.

**`Chain::find_nearest`** does a simple linear scan over all particles, computing the squared distance to each one (avoiding the cost of a square root). It returns the index of the closest particle within the pick radius, or a sentinel value (`npos`) if nothing is close enough.

---

## Rendering (Drawing to the Screen)

The rendering module (`renderer.h` / `renderer.cpp`) handles everything related to OpenGL. It knows nothing about particles, constraints, or Verlet integration — it just receives an array of 2D positions and draws them.

### GPU Setup

**`ChainRenderer::init`** prepares the GPU:

1. **Compiles two shaders** (small programs that run on the GPU):
   - The **vertex shader** receives each particle's `(x, y)` position in pixel coordinates and converts it to *normalized device coordinates* (NDC) — a range from -1 to +1 that OpenGL uses internally. The conversion is: `ndc = (pos / resolution) * 2.0 - 1.0`. It also sets each particle's point size to 8 pixels.
   - The **fragment shader** takes a color (passed as a uniform) and outputs it as the pixel color.

2. **Creates a Vertex Array Object (VAO)** — an OpenGL object that remembers how vertex data is laid out.

3. **Creates a Vertex Buffer Object (VBO)** — a block of GPU memory sized for 20 particles' worth of `Vec2` data (20 x 8 bytes = 160 bytes). It's allocated with `GL_DYNAMIC_DRAW` because the positions change every frame.

**`compile_shader`** and **`link_program`** are helper functions that handle the boilerplate of creating and error-checking OpenGL shader objects.

### Drawing Each Frame

**`ChainRenderer::draw`** runs every frame:

1. Uploads the latest particle positions to the VBO using `glBufferSubData`
2. Activates the shader and sets the window resolution uniform
3. Draws the chain as a **`GL_LINE_STRIP`** (continuous line through all particles) in a muted gray-blue color — this is the rope
4. Draws the same positions again as **`GL_POINTS`** (dots at each particle) in a bright yellow — these are the nodes

Two draw calls, same data, different visual styles.

### Cleanup

**`ChainRenderer::cleanup`** deletes the shader program, VBO, and VAO from GPU memory when the application exits.

---

## The Main Loop

`main.cpp` ties everything together. Here's the frame-by-frame flow:

```
1. Calculate dt (time since last frame)
       |
       |-- Clamped to 33ms max to prevent physics blowups
       |   after lag spikes or window pauses
       v
2. chain.update(dt, gravity, iterations)
       |
       |-- integrate():        move particles under gravity
       |-- solve_constraints(): fix distances (8 iterations)
       |-- sync_pos_cache():    copy positions to flat array
       v
3. Clear the screen (dark background)
       v
4. renderer.draw(chain.positions(), width, height)
       |
       |-- Upload positions to GPU
       |-- Draw lines (rope)
       |-- Draw points (nodes)
       v
5. Swap buffers (display the frame)
       v
6. Poll events (process mouse/keyboard input)
       |
       |-- Callbacks fire here if the user clicked or moved the mouse
       v
   (repeat)
```

### Simulation Parameters

These are defined as constants at the top of `main.cpp`:

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `kNumParticles` | 20 | Number of particles in the chain |
| `kSegmentLength` | 25.0 px | Rest distance between connected particles |
| `kGravity` | (0, -980) px/s^2 | Downward gravitational acceleration |
| `kConstraintIterations` | 8 | How many times constraints are enforced per frame |
| `kPickRadius` | 25.0 px | How close a click must be to grab a particle |
| `kMaxDt` | 0.033 s | Maximum timestep (prevents physics instability) |

### Coordinate System

The simulation uses pixel coordinates with **(0, 0) at the bottom-left** and Y increasing upward. This matches mathematical convention and makes gravity a simple negative Y value.

GLFW reports mouse coordinates with (0, 0) at the **top-left** and Y increasing downward (screen convention). The conversion is one line: `sim_y = window_height - glfw_y`.

---

## File-by-File Breakdown

### `vec2.h`

A minimal 2D vector type. Every arithmetic operation the simulation and renderer need lives here.

| Member | Purpose |
|--------|---------|
| `operator+`, `-`, `*` | Vector addition, subtraction, scalar multiplication |
| `operator+=`, `-=` | In-place versions of the above |
| `dot(Vec2)` | Dot product of two vectors |
| `length_sq()` | Squared length (avoids a square root — used for distance comparisons) |
| `length()` | Actual length (uses `std::sqrt`) |

### `chain.h` / `chain.cpp`

The simulation module. Contains the physics and all particle state.

**Data structures:**

| Type | Fields | Purpose |
|------|--------|---------|
| `Particle` | `pos`, `prev_pos`, `pinned` | One point in the chain |
| `Constraint` | `a`, `b`, `rest_length` | A link between two particles |

**`Chain` public methods:**

| Method | Purpose |
|--------|---------|
| `Chain(anchor, count, length)` | Constructor — builds a vertical chain with particle 0 pinned |
| `update(dt, gravity, iterations)` | Runs one simulation step: integrate, solve, sync |
| `positions()` | Returns a read-only view of particle positions for the renderer |
| `size()` | Returns the number of particles |
| `set_particle_pos(index, pos)` | Teleports a particle (sets `pos` and `prev_pos`, zeroing velocity) |
| `set_pinned(index, bool)` | Pins or unpins a particle |
| `is_pinned(index)` | Checks if a particle is pinned |
| `find_nearest(pos, max_dist)` | Finds the closest particle to a point within a radius |

**`Chain` private methods:**

| Method | Purpose |
|--------|---------|
| `integrate(dt, gravity)` | Verlet integration — the core physics step |
| `solve_constraints(iterations)` | Enforces distance constraints between connected particles |
| `sync_pos_cache()` | Copies particle positions into a contiguous array for GPU upload |

### `renderer.h` / `renderer.cpp`

The rendering module. Handles all OpenGL operations. Has no knowledge of particles or physics.

**`ChainRenderer` methods:**

| Method | Purpose |
|--------|---------|
| `init(max_particles)` | Compiles shaders, creates VAO/VBO |
| `draw(positions, width, height)` | Uploads positions and draws the chain (lines + points) |
| `cleanup()` | Frees all GPU resources |

**Static helpers (internal to `renderer.cpp`):**

| Function | Purpose |
|----------|---------|
| `compile_shader(type, src)` | Creates and compiles a single shader, logs errors |
| `link_program(vert, frag)` | Links vertex + fragment shaders into a program, logs errors |

### `main.cpp`

The entry point and orchestration layer. Creates the window, simulation, and renderer. Handles input via GLFW callbacks.

**`AppState` struct** — Holds a pointer to the chain plus all drag/input state. Attached to the GLFW window via `glfwSetWindowUserPointer` so callbacks can access it.

**GLFW callbacks:**

| Callback | Purpose |
|----------|---------|
| `framebuffer_size_callback` | Updates `glViewport` when the window is resized |
| `window_size_callback` | Tracks window dimensions for coordinate conversion |
| `mouse_button_callback` | Starts/stops particle dragging on left click/release |
| `cursor_position_callback` | Moves the dragged particle to follow the mouse |
