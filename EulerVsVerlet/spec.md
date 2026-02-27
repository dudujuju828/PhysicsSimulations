# EulerVsVerlet

A 2x2 grid simulation of a ball on a spring comparing Forward Euler (left column) and Verlet integration (right column), with and without damping (top and bottom rows). The simulation cycles through parameter presets that expose the behavioral differences between the two methods.

## Layout

The window is split into a 2x2 grid by a vertical and horizontal divider.

```
 Top-left: Forward Euler (no damping)  |  Top-right: Verlet (no damping)
 ──────────────────────────────────────────────────────────────────────
 Bottom-left: Forward Euler (damped)   |  Bottom-right: Verlet (damped)
```

- **Top row (No Damping):** Pure spring oscillation. Euler's energy gain is clearly visible — its orbit grows over time while Verlet stays stable.
- **Bottom row (With Damping):** Same physics with velocity-proportional damping applied. Both methods look similar because damping dissipates energy faster than Euler can add it. This demonstrates why damping hides Euler's flaws.
- **Column labels:** "Forward Euler" on the left, "Verlet" on the right.
- **Row labels:** "No Damping" at top, "With Damping" below the horizontal divider.
- **Current preset name** displayed at the top center.
- **Energy readout** in each quadrant showing total mechanical energy (kinetic + potential).

All four simulations start from identical initial conditions and receive identical parameters. The only differences are the integration method and whether damping is applied.

## Physics

Each side simulates a 2D ball-on-spring (simple harmonic oscillator):

- A **fixed anchor** at the center of each half.
- A **ball** connected to the anchor by a spring.
- **Spring force:** Hooke's law, `F = -k * (pos - anchor)`, where `k` is stiffness.
- **Damping (bottom row only):** Velocity-proportional force `F_d = -c * v`. The top row has no damping, making Euler's energy gain stark. The bottom row applies damping to show how it masks the difference.
- **Gravity off.** Pure spring oscillation keeps things clean and makes energy analysis simple.

### Euler Integration (explicit/forward Euler)

```
vel += acceleration * dt
pos += vel * dt
```

### Verlet Integration (Störmer-Verlet)

```
displacement = pos - prev_pos
damp = 1 - c/m * dt
prev_pos = pos
pos = pos + displacement * damp + acceleration * dt * dt
```

When damping is zero, `damp = 1` and this reduces to standard Störmer-Verlet.

## Parameter Presets

The simulation cycles through these presets automatically, spending ~6 seconds on each. Between presets, both balls reset to the same initial conditions.

| # | Name              | Stiffness (k) | Initial Offset      | Timestep (dt)   | Damping (c) | Purpose                                                                 |
|---|-------------------|----------------|----------------------|------------------|-------------|-------------------------------------------------------------------------|
| 1 | Gentle Spring     | 4.0            | (80, 0) from anchor  | 1/60 (~0.0167)   | 0.8         | Baseline. Both methods look nearly identical. Establishes trust.        |
| 2 | Stiff Spring      | 50.0           | (80, 0) from anchor  | 1/60             | 1.5         | Euler starts gaining energy visibly. Ball orbit grows. Verlet is stable.|
| 3 | Large Timestep    | 20.0           | (80, 0) from anchor  | 1/20 (0.05)      | 1.2         | Coarse timestep. Euler diverges fast. Verlet stays bounded.             |
| 4 | Diagonal Launch   | 20.0           | (60, 60) from anchor | 1/60             | 1.2         | 2D orbit. Euler's orbit grows into a spiral. Verlet traces a fixed path.|

Damping values are only applied to the bottom row. The top row always uses `c = 0`.

After the last preset, loop back to the first.

## Rendering

Same tech stack as VerletChain: OpenGL 4.6, GLFW, glad.

- **Anchor:** Small filled circle (or point), fixed in place.
- **Ball:** Larger filled circle. Euler ball and Verlet ball should be different colors.
- **Spring line:** A straight line from anchor to ball.
- **Trail:** Faint line showing the ball's recent path (last ~3 seconds). This makes orbits and energy drift immediately visible.
- **Energy bar or number:** Total energy `E = 0.5 * k * |pos - anchor|^2 + 0.5 * mass * |vel|^2` displayed numerically below each ball. For Verlet, derive velocity as `(pos - prev_pos) / dt` for the energy readout only.

## Fixed Timestep Simulation

Both integrators use a fixed `dt` from the preset (not the real frame delta). The simulation steps once per `dt` period using an accumulator:

```
accumulator += frame_delta
while accumulator >= dt:
    step_euler(dt)
    step_verlet(dt)
    step_euler_damped(dt)
    step_verlet_damped(dt)
    accumulator -= dt
```

This ensures all four simulations receive exactly the same timestep and the same number of steps, making the comparison fair. It also means the Large Timestep preset actually uses fewer simulation steps per second, which is the point.

## Interaction

- **Space bar:** Pause/resume.
- **Right arrow or N:** Skip to next preset immediately.
- **R:** Reset current preset (re-center both balls).
- **No mouse interaction needed.**

## Project Structure

```
EulerVsVerlet/
  CMakeLists.txt
  src/
    main.cpp
    vec2.h          (copy or shared — same as VerletChain)
    spring_euler.h
    spring_euler.cpp
    spring_verlet.h
    spring_verlet.cpp
    renderer.h
    renderer.cpp
```

## Build

Same CMake pattern as VerletChain: fetch GLFW and glad, build a single executable.
