# EulerVsVerlet

A side-by-side simulation of a ball on a spring comparing Euler integration (left) and Verlet integration (right). The simulation cycles through parameter presets that expose the behavioral differences between the two methods.

## Layout

The window is split vertically down the middle.

- **Left half:** Euler integration. A ball attached by a spring to a fixed anchor point.
- **Right half:** Verlet integration. An identical ball-and-spring setup with the same parameters.
- **Divider:** A thin vertical line separating the two halves.
- **Labels:** "Euler" above the left side, "Verlet" above the right side.
- **Current preset name** displayed at the top center (e.g. "Gentle Spring", "Stiff Spring").
- **Energy readout** below each ball showing total mechanical energy (kinetic + potential). This is the clearest way to see Euler gaining energy while Verlet conserves it.

Both simulations start from identical initial conditions and receive identical parameters. The only difference is the integration method.

## Physics

Each side simulates a 2D ball-on-spring (simple harmonic oscillator):

- A **fixed anchor** at the center of each half.
- A **ball** connected to the anchor by a spring.
- **Spring force:** Hooke's law, `F = -k * (pos - anchor)`, where `k` is stiffness.
- **No damping.** This is critical — damping hides Euler's energy gain. Without it, the differences are stark.
- **Gravity off.** Pure spring oscillation keeps things clean and makes energy analysis simple.

### Euler Integration (explicit/forward Euler)

```
vel += acceleration * dt
pos += vel * dt
```

### Verlet Integration (Störmer-Verlet)

```
displacement = pos - prev_pos
prev_pos = pos
pos = pos + displacement + acceleration * dt * dt
```

## Parameter Presets

The simulation cycles through these presets automatically, spending ~6 seconds on each. Between presets, both balls reset to the same initial conditions.

| # | Name              | Stiffness (k) | Initial Offset      | Timestep (dt)   | Purpose                                                                 |
|---|-------------------|----------------|----------------------|------------------|-------------------------------------------------------------------------|
| 1 | Gentle Spring     | 4.0            | (80, 0) from anchor  | 1/60 (~0.0167)   | Baseline. Both methods look nearly identical. Establishes trust.        |
| 2 | Stiff Spring      | 50.0           | (80, 0) from anchor  | 1/60             | Euler starts gaining energy visibly. Ball orbit grows. Verlet is stable.|
| 3 | Very Stiff Spring | 200.0          | (80, 0) from anchor  | 1/60             | Euler spirals outward rapidly. Verlet holds a clean ellipse.            |
| 4 | Large Timestep    | 20.0           | (80, 0) from anchor  | 1/20 (0.05)      | Coarse timestep. Euler diverges fast. Verlet stays bounded.             |
| 5 | Diagonal Launch   | 20.0           | (60, 60) from anchor | 1/60             | 2D orbit. Euler's orbit grows into a spiral. Verlet traces a fixed path.|

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
    accumulator -= dt
```

This ensures both methods receive exactly the same timestep and the same number of steps, making the comparison fair. It also means preset 4 (large timestep) actually uses fewer simulation steps per second, which is the point.

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
