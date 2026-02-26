# How EulerVsVerlet Works

This simulation places two identical ball-on-spring systems side by side. Both start from the same position with the same spring. The only difference is the integration method: the left side uses **forward Euler**, the right side uses **Störmer-Verlet**. The simulation cycles through parameter presets that make the differences progressively obvious.

## Table of Contents

- [The Setup: A Ball on a Spring](#the-setup-a-ball-on-a-spring)
- [What Is an Integrator?](#what-is-an-integrator)
- [Forward Euler Integration](#forward-euler-integration)
- [Störmer-Verlet Integration](#störmer-verlet-integration)
- [Why Euler Gains Energy](#why-euler-gains-energy)
  - [A Concrete Example](#a-concrete-example)
  - [The Geometric Intuition](#the-geometric-intuition)
  - [Why Verlet Doesn't Have This Problem](#why-verlet-doesnt-have-this-problem)
  - [The Symplectic Euler Trap](#the-symplectic-euler-trap)
- [The Parameter Presets](#the-parameter-presets)
- [Fixed Timestep Accumulator](#fixed-timestep-accumulator)
- [Rendering and Text](#rendering-and-text)
- [File-by-File Breakdown](#file-by-file-breakdown)

---

## The Setup: A Ball on a Spring

Each side simulates a simple harmonic oscillator in 2D:

- A **fixed anchor** at the center of each half of the screen.
- A **ball** connected to the anchor by a spring.
- The spring obeys **Hooke's law**: `F = -k * (pos - anchor)`, where `k` is the spring stiffness. The force always points back toward the anchor and is proportional to how far the ball has stretched.
- There is **no damping** and **no gravity**. This is critical — damping would bleed energy out of the system and mask Euler's energy gain. Without it, any error in the integrator accumulates visibly.

The ball is displaced from the anchor at the start and released. In a perfect simulation, it would oscillate forever along the same path with constant total energy. In practice, the integrator's numerical errors cause the path to change over time.

---

## What Is an Integrator?

An integrator is the algorithm that advances the simulation forward by one timestep. Given the current state of the system (position, velocity or position history) and the forces acting on it, the integrator computes the state at the next moment in time.

The fundamental equation is Newton's second law: `F = m * a`, which gives us acceleration. From acceleration, we need to figure out where the ball ends up. Different integrators do this differently, and the choice has dramatic consequences for accuracy and stability.

---

## Forward Euler Integration

Forward Euler is the simplest possible integrator. It uses the current state to compute derivatives, then steps forward:

```
acceleration = -k / mass * (pos - anchor)
new_vel = vel + acceleration * dt
new_pos = pos + vel * dt
```

Both updates use the **old** values. The velocity update uses the old position (to compute acceleration). The position update uses the **old velocity** (not the just-computed new one). This is what makes it "forward" Euler — everything is evaluated at the start of the timestep.

In code (`spring_euler.cpp`):

```cpp
Vec2 displacement = pos_ - anchor_;
Vec2 accel = displacement * (-k_ / mass_);
Vec2 new_vel = vel_ + accel * dt;
Vec2 new_pos = pos_ + vel_ * dt;
vel_ = new_vel;
pos_ = new_pos;
```

---

## Störmer-Verlet Integration

Verlet integration doesn't store velocity at all. It stores the current position and the previous position. Velocity is implicit — it's the displacement between them.

```
displacement = pos - prev_pos
acceleration = -k / mass * (pos - anchor)
prev_pos = pos
pos = pos + displacement + acceleration * dt²
```

In code (`spring_verlet.cpp`):

```cpp
Vec2 displacement = pos_ - prev_pos_;
Vec2 d = pos_ - anchor_;
Vec2 accel = d * (-k_ / mass_);
prev_pos_ = pos_;
pos_ = pos_ + displacement + accel * (dt * dt);
```

The formula comes from adding two Taylor expansions:

```
x(t + dt) = x(t) + v*dt + ½a*dt² + ...
x(t - dt) = x(t) - v*dt + ½a*dt² - ...
```

Adding them cancels the velocity terms and gives:

```
x(t + dt) = 2*x(t) - x(t - dt) + a*dt²
```

Which rearranges to: `x + (x - x_prev) + a*dt²` — exactly the code above.

---

## Why Euler Gains Energy

This is the core of the demonstration. Forward Euler systematically adds energy to oscillating systems. Here's why.

### A Concrete Example

Consider a ball on a spring in 1D, currently at position `x = 10` with velocity `v = 0`, being pulled back toward the anchor at `x = 0`. Say `k = 1`, `m = 1`, `dt = 1` (large timestep for clarity).

The acceleration is `a = -k * x = -10`.

**Forward Euler step:**

```
new_vel = 0 + (-10) * 1 = -10
new_pos = 10 + 0 * 1 = 10       ← uses OLD velocity (0), not new (-10)
```

After one step: `pos = 10, vel = -10`.

Now the ball has velocity -10 but hasn't moved yet. Next step:

```
a = -k * 10 = -10
new_vel = -10 + (-10) * 1 = -20
new_pos = 10 + (-10) * 1 = 0
```

After two steps: `pos = 0, vel = -20`.

The ball is back at the anchor, but its speed is 20 — far more than it should have. In a perfect simulation, the ball would arrive at `x = 0` with speed 10 (conservation of energy: `½kx² = ½mv²` → `v = x * sqrt(k/m) = 10`). Forward Euler gave it twice the correct speed.

### The Geometric Intuition

Think of the ball's trajectory in **phase space** — a plot where the x-axis is position and the y-axis is velocity. For a perfect spring, this trajectory is an ellipse. The ball traces the same ellipse forever, and the area enclosed by the ellipse is proportional to its total energy.

Forward Euler approximates this ellipse with straight-line steps. At each step, it moves tangent to the ellipse. But a straight line tangent to an ellipse always lands **outside** the ellipse. Every single step pushes the state slightly outward. Over many steps, the trajectory spirals outward — the ellipse grows, and so does the energy.

The rate of energy gain per step is proportional to `ω² * dt²`, where `ω = sqrt(k/m)` is the natural frequency. This means:

- **Stiffer springs** (higher `k`) → faster energy gain
- **Larger timesteps** (higher `dt`) → faster energy gain
- **Lighter masses** (lower `m`) → faster energy gain

After `N` steps, the energy has multiplied by approximately `(1 + ω²dt²)^N`, which grows exponentially.

### Why Verlet Doesn't Have This Problem

Verlet integration is **symplectic** — it preserves the area in phase space. Its steps don't systematically land inside or outside the true ellipse. Instead, the trajectory traces a slightly different ellipse that wobbles around the true one but never grows. The total energy fluctuates slightly each step but doesn't drift over time.

This comes from the mathematical structure of the Verlet formula. By working with positions at three time levels (`t-dt`, `t`, `t+dt`) and never explicitly computing velocity, the first-order error terms cancel exactly. The remaining error is symmetric — it doesn't favor energy gain or loss.

### The Symplectic Euler Trap

There is a common mistake when implementing "Euler" integration:

```
vel += accel * dt    ← update velocity first
pos += vel * dt      ← use the NEW velocity
```

This looks like Euler, but it's actually **symplectic Euler** (also called semi-implicit Euler). By using the *new* velocity in the position update, the errors from the two updates partially cancel — one overshoots, the other undershoots. Symplectic Euler is energy-conserving and would look identical to Verlet in this demo.

True forward Euler must compute both updates from the **old** state:

```
new_vel = vel + accel * dt
new_pos = pos + vel * dt     ← uses OLD velocity
vel = new_vel
pos = new_pos
```

This distinction — which velocity you use for the position update — is the entire difference between an integrator that conserves energy and one that doesn't.

---

## The Parameter Presets

The simulation cycles through five presets, spending six seconds on each. They are ordered to build intuition progressively:

| # | Name | k | Offset | dt | What You'll See |
|---|------|---|--------|----|-----------------|
| 1 | Gentle Spring | 4 | (80, 0) | 1/60 | Both look nearly identical. Euler's orbit grows ~22% in amplitude over 6 seconds — subtle, but the energy readout will show the drift. |
| 2 | Stiff Spring | 50 | (80, 0) | 1/60 | Euler's orbit visibly grows. The trail spirals outward while Verlet's trail stays fixed. Energy readout diverges clearly. |
| 3 | Very Stiff Spring | 200 | (80, 0) | 1/60 | Euler explodes rapidly. The ball flies offscreen within seconds. Verlet remains perfectly stable. |
| 4 | Large Timestep | 20 | (80, 0) | 1/20 | Moderate stiffness but a coarse timestep (only 20 steps per second). Euler diverges fast because the energy gain compounds with `dt²`. |
| 5 | Diagonal Launch | 20 | (60, 60) | 1/60 | The ball starts displaced diagonally, producing a 2D orbital path. Euler's orbit spirals outward. Verlet traces a fixed ellipse. |

The energy readout below each ball shows `E = ½k|x|² + ½m|v|²`. For Verlet, velocity is derived as `(pos - prev_pos) / dt`. Watch Euler's number climb while Verlet's stays constant.

---

## Fixed Timestep Accumulator

Both integrators must receive exactly the same timestep and the same number of steps — otherwise you're not comparing integration methods, you're comparing timestep choices.

The simulation uses a fixed-timestep accumulator:

```cpp
accumulator += frame_dt;
while (accumulator >= preset.dt) {
    euler.step(preset.dt);
    verlet.step(preset.dt);
    accumulator -= preset.dt;
}
```

Real frame time varies (vsync jitter, OS interrupts), but the physics always steps in exact increments of `preset.dt`. If a frame takes longer than usual, multiple physics steps run to catch up. If it's shorter, the accumulator saves the leftover time for next frame.

This is especially important for preset 4 (Large Timestep), where `dt = 1/20`. The simulation only steps 20 times per second, which means fewer steps per frame and a coarser approximation — exactly what makes Euler fail harder.

---

## Rendering and Text

The renderer (`renderer.cpp`) has two pipelines:

**Geometry pipeline** — Draws points, lines, and line strips in pixel coordinates (y-up, origin at bottom-left). A vertex shader converts pixel positions to OpenGL's normalized device coordinates (NDC): `ndc = (pos / resolution) * 2.0 - 1.0`. Supports variable point sizes (small for anchors, large for balls) and alpha blending (faint trails, solid geometry).

**Text pipeline** — Uses `stb_easy_font`, a single-header library that generates text as triangle quads. No textures, no font files — characters are built from flat-shaded quads. The output is scaled 2x for legibility and positioned in screen-down coordinates (y-down, origin at top-left), which is handled by a separate vertex shader.

Each frame draws: the vertical divider, both trails as faded line strips, both spring lines, both anchor points, both balls, labels ("Euler" / "Verlet"), the current preset name, energy readouts, and a controls hint.

---

## File-by-File Breakdown

### `vec2.h`

2D vector type with arithmetic operators, dot product, and length. Also contains `Trail` — a circular buffer of 256 positions used to record the ball's recent path for rendering.

### `spring_euler.h` / `spring_euler.cpp`

Forward Euler spring simulation. Stores position, velocity, anchor, stiffness, mass, and a trail.

| Method | Purpose |
|--------|---------|
| `reset(anchor, offset, k, mass)` | Initialize state for a new preset |
| `step(dt)` | One forward Euler timestep |
| `energy()` | Total mechanical energy (KE + PE) |
| `trail()` | Access the position trail for rendering |

### `spring_verlet.h` / `spring_verlet.cpp`

Störmer-Verlet spring simulation. Stores position, previous position, anchor, stiffness, mass, and a trail.

| Method | Purpose |
|--------|---------|
| `reset(anchor, offset, k, mass)` | Initialize state (sets `prev_pos = pos` for zero initial velocity) |
| `step(dt)` | One Verlet timestep |
| `energy(dt)` | Total energy (derives velocity as `(pos - prev_pos) / dt` for KE) |
| `trail()` | Access the position trail for rendering |

### `renderer.h` / `renderer.cpp`

OpenGL renderer with geometry and text pipelines.

| Method | Purpose |
|--------|---------|
| `init()` | Compile shaders, create VAO/VBO for geometry and text, pre-generate index buffer for text quads |
| `draw_points(...)` | Draw filled circles at given positions |
| `draw_line_strip(...)` | Draw a connected line through positions (used for trails) |
| `draw_lines(...)` | Draw disconnected line segments (used for spring lines and the divider) |
| `draw_text(...)` | Render a string at a screen position using stb_easy_font |
| `cleanup()` | Free all GPU resources |

### `main.cpp`

Entry point and orchestration. Contains:

- **Preset definitions** — the five parameter sets as a constexpr array
- **AppState** — holds both springs, the renderer, preset index, timer, accumulator, and pause flag
- **reset_preset / next_preset** — reinitialize both springs from current preset parameters
- **GLFW callbacks** — keyboard input (Space, N/Right, R, Escape) and window resize
- **draw_scene** — extracts trails from circular buffers, draws all geometry and text
- **Main loop** — fixed-timestep accumulator, auto-cycling presets, clear/draw/swap
