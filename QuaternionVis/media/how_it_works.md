# How QuaternionVis Works

This simulation places two identical wireframe unit spheres side by side. Both show a coordinate frame rotating from orientation A to orientation B. The only difference is the interpolation method: the left side uses **LERP** (normalize-after-lerp), the right side uses **SLERP** (spherical linear interpolation). The simulation cycles through rotation presets that make the speed distortion progressively obvious.

## Table of Contents

- [The Setup: Rotating Between Two Orientations](#the-setup-rotating-between-two-orientations)
- [What Is a Quaternion?](#what-is-a-quaternion)
- [SLERP: Spherical Linear Interpolation](#slerp-spherical-linear-interpolation)
- [LERP (nlerp): Normalize After Linear Interpolation](#lerp-nlerp-normalize-after-linear-interpolation)
- [Why LERP Has Non-Uniform Speed](#why-lerp-has-non-uniform-speed)
  - [The Chord vs. the Arc](#the-chord-vs-the-arc)
  - [A Concrete Example](#a-concrete-example)
  - [When LERP Is Good Enough](#when-lerp-is-good-enough)
- [The Rotation Presets](#the-rotation-presets)
- [The Visual Elements](#the-visual-elements)
- [Camera and Projection](#camera-and-projection)
- [Split-Screen Rendering](#split-screen-rendering)
- [File-by-File Breakdown](#file-by-file-breakdown)

---

## The Setup: Rotating Between Two Orientations

Each viewport shows:

- A **wireframe unit sphere** — the space of all possible directions a single axis can point.
- A **rotating coordinate frame** — three colored axis lines (red X, green Y, blue Z) emanating from the origin, representing the current orientation.
- A **path trace** — a yellow curve on the sphere surface showing where the X-axis tip travels during the full interpolation.
- **Time markers** — orange dots placed at equal increments of the parameter `t` along the path. These are the key visual: on the SLERP side they're evenly spaced, on the LERP side they bunch up near the endpoints.

Both sides receive the exact same parameter `t` at every frame. The only difference is how `t` maps to a rotation.

---

## What Is a Quaternion?

A quaternion is a four-component number `q = (w, x, y, z)` that can represent a 3D rotation. A **unit quaternion** (one with length 1) lives on the surface of a 4D hypersphere. Every 3D rotation corresponds to a point on this hypersphere, and interpolating between two rotations means finding a path between two points on it.

A quaternion is constructed from an axis and angle:

```
q = (cos(θ/2), sin(θ/2) * axis.x, sin(θ/2) * axis.y, sin(θ/2) * axis.z)
```

The identity rotation (no rotation at all) is `(1, 0, 0, 0)`. A 180° rotation around the Y-axis is `(0, 0, 1, 0)`.

To rotate a vector `v` by quaternion `q`:

```
v' = q * (0, v) * conjugate(q)
```

Or equivalently, using the optimized form: `v' = v + 2w(q_xyz × v) + 2(q_xyz × (q_xyz × v))`.

---

## SLERP: Spherical Linear Interpolation

SLERP finds the shortest great-circle arc between two quaternions on the 4D hypersphere and moves along it at constant angular velocity. The formula is:

```
slerp(a, b, t) = a * sin((1-t)*θ) / sin(θ) + b * sin(t*θ) / sin(θ)
```

where `θ = acos(dot(a, b))` is the angle between the two quaternions.

Key properties:

- **Constant angular speed** — equal steps in `t` produce equal angular steps. At `t = 0.25`, you've covered exactly 25% of the rotation angle.
- **Great-circle path** — follows the shortest arc on the hypersphere, which is the geodesic (the 4D equivalent of a straight line on a curved surface).
- **Requires a trig function per frame** — `sin` and `acos` are needed, making it slightly more expensive than LERP.

When `θ ≈ 0` (nearly identical orientations), `sin(θ)` approaches zero and causes numerical instability. The implementation falls back to LERP in this case, which is safe because LERP and SLERP are nearly identical for small angles.

---

## LERP (nlerp): Normalize After Linear Interpolation

LERP interpolates the four quaternion components linearly, then normalizes the result back to unit length:

```
lerp(a, b, t) = normalize(a * (1 - t) + b * t)
```

This is computationally cheap — just a weighted average and a normalization. No trig functions.

Key properties:

- **Same path as SLERP** — after normalization, the interpolated quaternion lands on the same great-circle arc. The path through 3D rotation space is identical.
- **Non-uniform speed** — equal steps in `t` do NOT produce equal angular steps. The rotation slows down near the endpoints and speeds up in the middle.
- **Cheap to compute** — just multiplication, addition, and a square root (for normalization).

The path is the same because normalizing a point on the chord projects it radially onto the sphere, which always lands on the great-circle arc connecting the two endpoints.

---

## Why LERP Has Non-Uniform Speed

### The Chord vs. the Arc

Imagine two points on a circle. SLERP walks along the **arc** at constant speed. LERP walks along the straight-line **chord** between them at constant speed, then projects each position onto the circle by normalizing.

The problem: the chord is closer to the circle near the endpoints and farther from it in the middle. When you project evenly-spaced chord points onto the arc:

- Near the endpoints, the chord is nearly tangent to the circle. A small step along the chord projects to a small step along the arc → **slow angular movement**.
- Near the middle, the chord is far from the circle. A small step along the chord projects to a large step along the arc → **fast angular movement**.

The result is that LERP starts slow, accelerates through the middle, and decelerates at the end. The time markers visually bunch up near the start and end positions.

### A Concrete Example

For the 180° Y-axis preset (`q_start = identity, q_end = 180° around Y`):

| t | SLERP angular progress | LERP angular progress |
|---|------------------------|-----------------------|
| 0.00 | 0° | 0° |
| 0.25 | 45° (25%) | ~37° (20.5%) |
| 0.50 | 90° (50%) | 90° (50%) |
| 0.75 | 135° (75%) | ~143° (79.5%) |
| 1.00 | 180° (100%) | 180° (100%) |

At `t = 0.25`, SLERP has covered exactly a quarter of the rotation. LERP has only covered about a fifth. By `t = 0.75`, LERP has overshot to nearly 80%. The midpoint always agrees (by symmetry), but everywhere else the speed differs.

For the 10° small-angle preset, the numbers differ by less than 0.01° — LERP is effectively perfect for small rotations.

### When LERP Is Good Enough

The speed distortion scales with the rotation angle. For angles under ~15°, LERP and SLERP are visually indistinguishable. This is why many game engines use nlerp for incremental frame-to-frame rotations (which are typically small) and reserve SLERP for long cinematic camera sweeps or animation blending across large angle differences.

LERP is also commutative and easier to extend to blending multiple quaternions (just average and normalize), while SLERP only works cleanly between two quaternions.

---

## The Rotation Presets

The simulation cycles through five presets, each highlighting a different aspect of the SLERP/LERP comparison:

| # | Name | Rotation | Duration | What You'll See |
|---|------|----------|----------|-----------------|
| 1 | 90° X-axis | Identity → 90° around (1,0,0) | 5s | Moderate single-axis rotation. Time markers show subtle but visible bunching on LERP side. |
| 2 | 180° Y-axis | Identity → 180° around (0,1,0) | 6s | Maximum distortion. The LERP chord cuts through the sphere interior. Markers bunch dramatically at endpoints. The `ang%` readout diverges most at t=25% and t=75%. |
| 3 | Diagonal 120° | Identity → 120° around (1,1,0) | 5s | Off-axis rotation. The path curves visibly across the sphere. From the right camera angle, the arc vs. chord distinction is clear. |
| 4 | Twist combo | 45° around Z → 90°×X then 90°×Y | 6s | Both orientations are non-identity. A complex compound rotation that makes the LERP wobble visible in the coordinate frame's motion. |
| 5 | Small angle | Identity → 10° around (0,1,0) | 4s | Tiny rotation. LERP ≈ SLERP. Markers are nearly identical. Demonstrates that LERP is fine for small angles. |

The simulation starts on preset 2 (180° Y-axis) for maximum visual impact, then cycles through all five.

---

## The Visual Elements

Each viewport draws the following, all as `GL_LINES` and `GL_POINTS` (no filled geometry):

1. **Wireframe unit sphere** — A latitude/longitude grid: 12 meridians (every 30°), 5 parallels at ±60°, ±30°, and the equator. The equator is drawn slightly brighter. This provides spatial reference for the rotation.

2. **World axes** — Very dim lines along X, Y, Z from -1.3 to +1.3. Fixed reference frame.

3. **Rotating coordinate frame** — Three bright colored lines from the origin to the rotated axis directions: X (red), Y (green), Z (blue). These move every frame as `t` progresses.

4. **Interpolation path** — A yellow line strip of 64 samples showing the full trajectory of the X-axis tip across the sphere surface. Pre-computed once per preset.

5. **Time markers** — 20 orange dots placed at `t = 0.05, 0.10, ..., 1.00` along the X-axis tip path. On SLERP, these are evenly spaced along the arc. On LERP, they bunch near the endpoints. **This is the key visual.**

6. **Start/end markers** — Larger dots: green at the starting X-axis position, red at the ending position.

7. **Current position** — A white dot tracking the X-axis tip in real time.

8. **LERP chord line** (left viewport only) — A straight red line from start to end X-axis positions. This line cuts through the sphere interior, showing the "shortcut" that LERP takes before normalization projects it back onto the surface.

The 2D text overlay (drawn after the 3D viewports) shows: "LERP" and "SLERP" labels, the preset name, `t` and angular progress percentages, and a controls hint.

---

## Camera and Projection

The camera uses spherical coordinates to orbit around the origin:

```
eye.x = distance * cos(elevation) * sin(azimuth)
eye.y = distance * sin(elevation)
eye.z = distance * cos(elevation) * cos(azimuth)
```

The look-at target is always the origin. The up vector is (0, 1, 0). The projection is a 45° vertical FOV perspective projection with aspect ratio = `(window_width / 2) / window_height` (since each viewport is half the window width).

Mouse dragging rotates the camera: horizontal drag changes azimuth, vertical drag changes elevation (clamped to ±89° to avoid gimbal flip). The camera is shared between both viewports so the comparison is always from the same angle.

---

## Split-Screen Rendering

Each frame follows this sequence:

1. Clear the full framebuffer (color + depth).
2. `glViewport(0, 0, w/2, h)` — set the left half as the render target. Enable depth testing. Draw the LERP scene.
3. `glViewport(w/2, 0, w/2, h)` — set the right half. Draw the SLERP scene.
4. `glViewport(0, 0, w, h)` — full window. Disable depth testing. Draw the 2D text overlay spanning both halves.

Both viewports use the same MVP matrix, so the wireframe spheres appear at the same position and angle. The only difference in the 3D content is the quaternion used to compute the rotating frame and current position.

---

## File-by-File Breakdown

### `vec3.h`

3-component float vector with arithmetic operators and free functions for `dot`, `cross`, `length`, and `normalize`.

### `mat4.h`

4x4 column-major matrix (OpenGL memory layout). Also defines a `vec4` struct.

| Method | Purpose |
|--------|---------|
| `identity()` | Returns the 4x4 identity matrix |
| `perspective(fov, aspect, near, far)` | Perspective projection matrix |
| `look_at(eye, center, up)` | View matrix from camera parameters |
| `from_quat(w, x, y, z)` | Convert unit quaternion to rotation matrix |
| `operator*(mat4, mat4)` | Matrix multiplication |
| `operator*(mat4, vec4)` | Matrix-vector multiplication |

### `quat.h`

Unit quaternion with interpolation methods.

| Method | Purpose |
|--------|---------|
| `identity()` | Returns (1, 0, 0, 0) — no rotation |
| `from_axis_angle(axis, angle)` | Construct from rotation axis and angle in radians |
| `operator*(quat, quat)` | Hamilton product (compose two rotations) |
| `rotate_vec(v)` | Rotate a 3D vector by this quaternion |
| `to_mat4()` | Convert to a 4x4 rotation matrix for OpenGL |
| `conjugate(q)` | Negate the vector part (inverse for unit quaternions) |
| `normalize(q)` | Scale to unit length |
| `dot(a, b)` | 4D dot product |
| `lerp(a, b, t)` | Component-wise interpolation + normalize (nlerp) |
| `slerp(a, b, t)` | Spherical linear interpolation with short-path handling |

### `sphere.h` / `sphere.cpp`

Wireframe sphere mesh generation. Produces three separate arrays of `GL_LINES` pairs:

| Output | Contents |
|--------|----------|
| `lines` | Non-equator latitude circles (±60°, ±30°) and all 12 meridians |
| `equator_lines` | Equator only (drawn brighter) |
| `axis_lines` | World reference axes along X, Y, Z |

### `renderer.h` / `renderer.cpp`

OpenGL renderer with 3D and text pipelines.

| Method | Purpose |
|--------|---------|
| `init()` | Compile shaders, create VAO/VBO for 3D geometry and text, pre-generate text index buffer |
| `draw_lines_3d(pts, mvp, color)` | Draw line segments in 3D (GL_LINES) |
| `draw_line_strip_3d(pts, mvp, color)` | Draw a connected path in 3D (GL_LINE_STRIP) |
| `draw_points_3d(pts, mvp, color, size)` | Draw points in 3D with a given size |
| `draw_text(text, x, y, scale, r, g, b, w, h)` | Render text at a screen position using stb_easy_font |
| `cleanup()` | Free all GPU resources |

### `main.cpp`

Entry point and orchestration. Contains:

- **Preset definitions** — five rotation pairs as a global array
- **PathData / compute_paths** — pre-computes the 64-sample path and 20 time markers for both methods at the start of each preset
- **AppState** — holds the renderer, sphere mesh, paths, camera state, animation state, and input state
- **Camera functions** — `build_view` and `build_projection` from spherical coordinates
- **GLFW callbacks** — keyboard (Space, N/Right, R, Escape), mouse drag for orbit, window resize
- **draw_viewport** — renders one complete 3D scene (sphere, axes, frame, path, markers, chord)
- **Main loop** — advance `t`, hold at 1.0 for 0.5s, auto-cycle presets, split-screen render, text overlay
