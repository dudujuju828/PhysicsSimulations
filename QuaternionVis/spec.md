# QuaternionVis

A 3D quaternion rotation visualizer comparing SLERP (Spherical Linear Interpolation) and LERP (Linear Interpolation). Two side-by-side viewports each show a wireframe unit sphere with an animated coordinate frame rotating from orientation A to orientation B. The SLERP side follows the great-circle arc at constant angular speed. The LERP side takes the straight-line chord through the sphere interior with non-uniform speed, making the distortion immediately visible.

## Layout

The window is split vertically into two equal halves. Each half contains its own 3D viewport with a wireframe unit sphere.

```
 LERP                        |  SLERP
 (wireframe sphere,          |  (wireframe sphere,
  rotating coordinate frame, |   rotating coordinate frame,
  interpolation path,        |   interpolation path,
  time markers)              |   time markers)
```

- **Left viewport:** LERP (normalize-after-lerp). Label "LERP" at top-left.
- **Right viewport:** SLERP. Label "SLERP" at top-right.
- **Vertical divider:** Thin line at x = width/2.
- **Preset name:** Top center, spanning the divider.
- **Controls hint:** Bottom center: `SPACE: pause  N/Right: next  R: reset  Drag: orbit`
- **Window size:** 1200x675 (16:9).
- **Orbit camera** is shared between both viewports so the comparison is always from the same angle.

## Math

### 3D Types

All math is header-only. No external math library.

**`vec3.h`** — 3-component vector:
- `operator+`, `operator-`, `operator*` (scalar), `operator/` (scalar)
- `dot(a, b)`, `cross(a, b)`, `length(v)`, `normalize(v)`

**`mat4.h`** — 4x4 column-major matrix (OpenGL convention):
- `mat4::identity()`
- `mat4::perspective(fov_y_rad, aspect, near, far)`
- `mat4::look_at(eye, center, up)`
- `operator*` for mat4 * mat4 and mat4 * vec4
- `mat4::from_quat(q)` — convert unit quaternion to rotation matrix
- Stored as `float m[16]` column-major, suitable for `glUniformMatrix4fv`.

**`quat.h`** — unit quaternion (w, x, y, z):
- `quat::from_axis_angle(vec3 axis, float angle_rad)`
- `quat::identity()` returning (1, 0, 0, 0)
- `operator*` for quaternion multiplication (Hamilton product)
- `conjugate(q)`, `normalize(q)`, `dot(a, b)`, `length(q)`
- `lerp(a, b, t)` — component-wise linear interpolation, then normalize (nlerp). Same great-circle path as slerp but non-uniform speed.
- `slerp(a, b, t)` — `a * sin((1-t)*θ)/sin(θ) + b * sin(t*θ)/sin(θ)`. Fall back to lerp when θ ≈ 0. Always choose the short path (negate b if `dot(a,b) < 0`).
- `quat::rotate_vec(vec3 v)` — rotate a vector: `v' = q * (0,v) * q*`.

### Interpolation

Parameter `t` sweeps 0.0 → 1.0 over the preset duration. Both viewports receive the same `t`.

- **LERP side:** `q_current = normalize(lerp(q_start, q_end, t))`
- **SLERP side:** `q_current = slerp(q_start, q_end, t)`

### Camera

Orbit camera with spherical coordinates:
- `azimuth`: initially 30°. `elevation`: initially 20°, clamped to (-89°, +89°).
- `distance`: fixed at 3.5.
- Eye: `(d·cos(φ)·sin(θ), d·sin(φ), d·cos(φ)·cos(θ))`
- Look-at: origin. Up: (0, 1, 0).
- Projection: 45° vertical FOV, aspect = (width/2) / height, near 0.1, far 100.
- Mouse drag: `azimuth += dx * 0.3`, `elevation -= dy * 0.3` (degrees per pixel).

## Presets

Auto-cycle through presets. Between presets, `t` resets to 0.

| # | Name             | q_start                    | q_end                              | Duration | Purpose                                                    |
|---|------------------|----------------------------|-------------------------------------|----------|------------------------------------------------------------|
| 1 | 90° X-axis       | identity                   | 90° around (1,0,0)                  | 5s       | Simple single-axis. Subtle speed distortion in markers.    |
| 2 | 180° Y-axis      | identity                   | 180° around (0,1,0)                 | 6s       | Half turn. Maximum LERP distortion. Chord through center.  |
| 3 | Diagonal 120°    | identity                   | 120° around normalize(1,1,0)        | 5s       | Diagonal axis. Great-circle arc clearly curved vs chord.   |
| 4 | Twist combo      | 45° around (0,0,1)         | 90°×(1,0,0) then 90°×(0,1,0)       | 6s       | Both non-identity. Complex path makes LERP wobble visible. |
| 5 | Small angle      | identity                   | 10° around (0,1,0)                  | 4s       | Tiny rotation. LERP ≈ SLERP. Shows LERP is fine for small angles. |

Loop after last preset.

## Rendering

### Shader (3D)

Single shader pair for all 3D geometry:

**Vertex:** `#version 460 core` — takes `vec3 a_pos`, uniform `mat4 u_mvp` and `float u_point_size`.
**Fragment:** uniform `vec4 u_color` → `frag_color`.

Text uses the same 2D stb_easy_font approach from EulerVsVerlet, drawn in a final full-window pass.

### Split-Screen

1. Clear full framebuffer (color + depth).
2. `glViewport(0, 0, w/2, h)` → draw LERP scene.
3. `glViewport(w/2, 0, w/2, h)` → draw SLERP scene.
4. `glViewport(0, 0, w, h)` → draw 2D text overlay.

### Per-Viewport Drawing

All GL_LINES and GL_POINTS. No triangles, no fill, no textures.

1. **Wireframe unit sphere** — lat/lon grid.
   - 12 longitudes (every 30°), 5 latitudes (-60°, -30°, 0°, +30°, +60°), each ~36 segments.
   - Dim gray (0.25, 0.25, 0.25, 0.4). Equator slightly brighter (0.35).

2. **World axes** — thin lines along X/Y/Z from -1.3 to +1.3. Very dim (alpha 0.2).

3. **Rotating coordinate frame** — three colored axis lines from origin to `q.rotate_vec({1,0,0})` etc.
   - X: red (1.0, 0.3, 0.3). Y: green (0.3, 1.0, 0.3). Z: blue (0.3, 0.3, 1.0).

4. **Interpolation path** — line strip of 64 evenly-spaced t values, showing where the X-axis tip traces.
   - Yellow (1.0, 0.85, 0.2, 0.7).

5. **Time markers** — 20 points at t = 0.05, 0.10, ..., 1.00 along the X-axis tip path.
   - SLERP: evenly spaced. LERP: bunched near endpoints. **This is the key visual.**
   - Orange (1.0, 0.6, 0.1), size 6.0.

6. **Start/end markers** — size 8.0. Start: green. End: red.

7. **Current position** — white point, size 10.0.

8. **LERP chord line** (left viewport only) — straight line from start to end X-axis tip positions.
   - Dim red (0.8, 0.2, 0.2, 0.3). Visibly cuts through the sphere interior.

Enable `GL_DEPTH_TEST` for 3D. Disable before text.

## Animation

`t += frame_dt / duration` each frame, clamped to [0, 1]. No fixed-timestep accumulator needed (purely visual). Hold at t=1.0 for 0.5s, then advance preset.

Pre-compute per preset (static for the duration):
- Path polyline (64 points per method)
- Time marker positions (20 points per method)
- Chord endpoints

Only the current quaternion and position indicator update per frame.

## Interaction

- **Space:** Pause/resume.
- **N / Right arrow:** Next preset.
- **R:** Reset current preset (t = 0).
- **Escape:** Quit.
- **Left mouse drag:** Orbit camera (shared between viewports).

## Project Structure

```
QuaternionVis/
  spec.md
  CMakeLists.txt
  glad_gen/              (copy from existing projects)
  src/
    main.cpp
    vec3.h               3D vector (header-only)
    mat4.h               4x4 matrix (header-only)
    quat.h               Unit quaternion with slerp/lerp (header-only)
    renderer.h           3D line/point renderer + 2D text
    renderer.cpp
    sphere.h             Wireframe sphere mesh generation
    sphere.cpp
    stb_easy_font.h      Vendored (same as EulerVsVerlet)
```

## Build

Same CMake pattern: fetch GLFW 3.4, link glad static lib, single executable. C++20.
