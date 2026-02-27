# ElectronOrbitals

A real-time volumetric renderer of hydrogen electron orbital probability clouds. GPU ray marching evaluates the full hydrogen wave function ψ(n,l,m) — associated Laguerre polynomials and real spherical harmonics — entirely in the fragment shader. Supports all orbitals from 1s through 4f (30 orbitals total). HDR bloom, two-tone phase coloring, and animated density perturbation give the clouds a bioluminescent, nebula-like appearance.

## Layout

Single full-window 3D viewport with an orbital cloud centered at the origin.

```
 ┌──────────────────────────────────────────┐
 │  3d₂₂ (n=3 l=2 m=2)          [top-left] │
 │                                           │
 │                                           │
 │            (orbital cloud                 │
 │             with bloom halo)              │
 │                                           │
 │                                           │
 │  density: 1.0  bloom: 0.5     [bot-left]  │
 │  SPACE: pause  ←/→: orbital   [bot-center]│
 └──────────────────────────────────────────┘
```

- **Orbital label:** Top-left. Shows spectroscopic name and quantum numbers, e.g. `2p_z (n=2 l=1 m=0)`.
- **Parameter readout:** Bottom-left. Current density scale, bloom intensity, step count.
- **Controls hint:** Bottom-center.
- **Window size:** 1920x1080 (16:9). Resizable.
- **Background:** Near-black (0.01, 0.01, 0.02) with a subtle radial vignette — center slightly lighter (0.03).

## Physics

### Hydrogen Wave Function

The wave function in spherical coordinates (r, θ, φ) with a₀ = 1 (atomic units):

```
ψ(n,l,m)(r,θ,φ) = R_nl(r) · Y_lm(θ,φ)
```

**Radial part:**

```
ρ = 2r / n
R_nl(r) = -N_nl · e^(-ρ/2) · ρ^l · L^(2l+1)_(n-l-1)(ρ)
```

Where `N_nl` is the normalization constant, precomputed on the CPU per orbital and passed as a uniform:

```
N_nl = sqrt((2/n)³ · (n-l-1)! / (2n · ((n+l)!)³))
```

**Associated Laguerre polynomials** `L^α_k(x)` computed iteratively in the shader:

```
L^α_0(x) = 1
L^α_1(x) = 1 + α - x
L^α_{k+1}(x) = ((2k + 1 + α - x) · L^α_k(x) - (k + α) · L^α_{k-1}(x)) / (k + 1)
```

With `k = n - l - 1` (max 3 for n≤4) and `α = 2l + 1`.

**Real spherical harmonics** `Y_lm(θ,φ)` — use Cartesian form in the shader to avoid atan2/acos. With `(u, v, w) = (x/r, y/r, z/r)`:

| l | m | Name | Formula (unnormalized) |
|---|---|------|----------------------|
| 0 | 0 | s | 1 |
| 1 | -1 | p_y | y/r |
| 1 | 0 | p_z | z/r |
| 1 | 1 | p_x | x/r |
| 2 | -2 | d_xy | xy/r² |
| 2 | -1 | d_yz | yz/r² |
| 2 | 0 | d_z² | (3z²-r²)/r² |
| 2 | 1 | d_xz | xz/r² |
| 2 | 2 | d_x²-y² | (x²-y²)/r² |
| 3 | -3 | f_y(3x²-y²) | y(3x²-y²)/r³ |
| 3 | -2 | f_xyz | xyz/r³ |
| 3 | -1 | f_yz² | y(5z²-r²)/r³ |
| 3 | 0 | f_z³ | z(5z²-3r²)/r³ |
| 3 | 1 | f_xz² | x(5z²-r²)/r³ |
| 3 | 2 | f_z(x²-y²) | z(x²-y²)/r³ |
| 3 | 3 | f_x(x²-3y²) | x(x²-3y²)/r³ |

Each `Y_lm` has a normalization constant precomputed on the CPU and passed as a uniform.

### Probability Density

The rendered quantity is the probability density `|ψ|²`. The sign of `ψ` is preserved for two-tone phase coloring: positive and negative lobes get different color palettes.

### Orbital Catalog

All valid (n, l, m) combinations for n = 1 to 4:

| n | Subshell | m values | Count |
|---|----------|----------|-------|
| 1 | 1s | 0 | 1 |
| 2 | 2s, 2p | 0; -1,0,1 | 4 |
| 3 | 3s, 3p, 3d | 0; -1,0,1; -2,-1,0,1,2 | 9 |
| 4 | 4s, 4p, 4d, 4f | 0; -1,0,1; -2..2; -3..3 | 16 |

**Total: 30 orbitals.** Cycle through them with Left/Right arrow keys.

### Bounding Radius

Each orbital's probability density decays exponentially. The bounding sphere radius (beyond which density is negligible) scales roughly as `n²`. Precomputed per orbital on the CPU:

| n | Approximate r_max (Bohr radii) |
|---|-------------------------------|
| 1 | 8 |
| 2 | 20 |
| 3 | 38 |
| 4 | 60 |

The camera distance adjusts proportionally when switching orbitals so the cloud fills a consistent visual size.

## Rendering

### Pipeline Overview

Three-pass rendering:

1. **Ray march pass** — Full-screen quad. Fragment shader marches rays through the bounding sphere, accumulating density and color. Output to an RGBA16F HDR framebuffer.
2. **Bloom pass** — Extract bright pixels (threshold), separable Gaussian blur (ping-pong between two half-res FBOs), multiple iterations for wide bloom.
3. **Composite pass** — Full-screen quad. Tonemap the HDR scene, add bloom additively, apply vignette. Output to the default framebuffer. Then draw text overlay.

### Shader Programs

All shaders `#version 460 core`.

**1. Ray march shader** (`fullscreen_vs` + `raymarch_fs`):

Vertex: Fullscreen triangle (3 vertices, no VBO). `gl_Position` covers [-1,1]². Pass `v_uv` to fragment.

Fragment uniforms:
- `mat4 u_inv_view_proj` — inverse of view * projection matrix
- `vec3 u_camera_pos`
- `int u_n, u_l, u_m` — quantum numbers
- `float u_radial_norm` — precomputed `N_nl`
- `float u_angular_norm` — precomputed spherical harmonic normalization
- `float u_bounding_radius` — orbital-specific
- `float u_density_scale` — user-adjustable brightness
- `int u_max_steps` — ray march quality (64/128/256)
- `float u_time` — for animated perturbation
- `float u_anim_speed` — perturbation speed multiplier

Fragment algorithm:
1. Reconstruct ray origin and direction from `v_uv` and `u_inv_view_proj`.
2. Intersect ray with bounding sphere (analytic ray-sphere intersection).
3. March from entry to exit point in uniform steps.
4. At each sample:
   a. Convert position to spherical: `r = length(pos)`, `(u,v,w) = pos/r`.
   b. Evaluate `R_nl(r)` using Laguerre recurrence.
   c. Evaluate `Y_lm` using Cartesian formula.
   d. `psi = R * Y`. `density = psi * psi * u_density_scale`.
   e. Apply animated perturbation: `density *= 1.0 + 0.06 * sin(u_time * u_anim_speed + r * 4.0 + dot(pos, vec3(1.7, 2.3, 3.1)))`.
   f. Map sign of `psi` to color palette (see Color section).
   g. Front-to-back alpha compositing: `color += (1 - alpha) * sample_color * sample_alpha`.
5. Add nucleus glow: `nucleus = vec3(1.0, 0.9, 0.7) * exp(-min_dist_to_origin² * 500.0)`.
6. Output `vec4(color + nucleus, 1.0)` to HDR buffer.

**2. Bright pass shader** (`fullscreen_vs` + `bright_fs`):

Extracts pixels where `luminance(color) > threshold`. Threshold = 0.8. Output: bright regions only; dark elsewhere.

**3. Gaussian blur shader** (`fullscreen_vs` + `blur_fs`):

Separable 9-tap Gaussian. Uniform `vec2 u_direction` selects horizontal or vertical. Two passes (H then V) per iteration, 3 iterations for wide bloom.

**4. Composite shader** (`fullscreen_vs` + `composite_fs`):

Uniforms:
- `sampler2D u_scene` — HDR ray march result
- `sampler2D u_bloom` — blurred bright regions
- `float u_bloom_intensity` — user-adjustable (default 0.5)
- `vec2 u_resolution`

Algorithm:
1. Sample scene and bloom.
2. `hdr = scene + bloom * u_bloom_intensity`.
3. Tonemap with ACES: `color = (hdr * (2.51 * hdr + 0.03)) / (hdr * (2.43 * hdr + 0.59) + 0.14)`.
4. Apply vignette: `vignette = 1.0 - 0.4 * length(uv - 0.5)²`.
5. Gamma correct: `pow(color, 1/2.2)`.

**5. Text shader** — same stb_easy_font pipeline as QuaternionVis/EulerVsVerlet (y-down pixel coords, quad EBO).

### Color Palette

Two-tone phase coloring based on the sign of ψ:

**Positive ψ (blues/teals):**
- Low density: deep blue (0.05, 0.15, 0.4)
- Mid density: teal (0.1, 0.6, 0.8)
- High density: cyan-white (0.7, 0.95, 1.0)
- Peak: white (1.0, 1.0, 1.0)

**Negative ψ (magentas/golds):**
- Low density: deep magenta (0.4, 0.05, 0.3)
- Mid density: coral (0.9, 0.4, 0.3)
- High density: gold-white (1.0, 0.85, 0.6)
- Peak: white (1.0, 1.0, 1.0)

Color mapping in the shader:
```glsl
float intensity = density;
vec3 base = (psi > 0.0) ? mix(deep_blue, teal, min(intensity * 2.0, 1.0))
                         : mix(deep_magenta, coral, min(intensity * 2.0, 1.0));
vec3 color = base + vec3(1.0) * max(0.0, intensity - 0.5) * 3.0;
```

High-density regions naturally exceed 1.0 in the HDR buffer, producing the white-hot cores that bloom outward.

### Nucleus

A tiny incandescent point at the origin. Rendered inside the ray march pass by tracking the closest approach of the ray to the origin. Warm white color (1.0, 0.9, 0.7), intensity falls off as `exp(-dist² * 500)`. Bright enough to trigger bloom.

### Framebuffer Objects

- **HDR FBO:** RGBA16F color attachment, no depth. Full resolution.
- **Bloom FBO A:** RGBA16F, half resolution (w/2 × h/2).
- **Bloom FBO B:** RGBA16F, half resolution (ping-pong target).

Recreate on window resize.

## Camera

Orbit camera with pan and zoom:

- **Azimuth:** Initially 30°. Mouse drag horizontal → `azimuth += dx * 0.3`.
- **Elevation:** Initially 20°, clamped to (-89°, +89°). Mouse drag vertical → `elevation -= dy * 0.3`.
- **Distance:** Initially `bounding_radius * 2.5`. Scroll wheel → `distance *= (1 - scroll_y * 0.1)`, clamped to [bounding_radius * 0.5, bounding_radius * 8.0].
- **Pan:** Right-mouse drag shifts the look-at target in the camera's local XY plane.
- **Projection:** 45° vertical FOV, window aspect ratio, near 0.1, far 1000.

When switching orbitals, the camera distance smoothly interpolates to the new orbital's default distance over 0.3s.

## Interaction

| Key | Action |
|-----|--------|
| Left / Right arrow | Previous / next orbital |
| Up / Down arrow | Increase / decrease density scale (×1.5 per press) |
| B / Shift+B | Increase / decrease bloom intensity (±0.1) |
| S / Shift+S | Increase / decrease ray march steps (64 → 128 → 256 → 64) |
| A / Shift+A | Increase / decrease animation speed (×2 per press) |
| Space | Pause/resume animation |
| R | Reset parameters to defaults |
| Escape | Quit |
| Left mouse drag | Orbit camera |
| Right mouse drag | Pan camera |
| Scroll wheel | Zoom in/out |

## Animation

The density perturbation runs continuously. `u_time` increments by `frame_dt * anim_speed` each frame. Default `anim_speed = 1.0`. The perturbation is subtle — a slow shimmer/breathing effect, not a visible oscillation. Pausing freezes `u_time`.

## Project Structure

```
ElectronOrbitals/
  spec.md
  CMakeLists.txt
  glad_gen/              (copy from existing projects)
  src/
    main.cpp             Window, input, render loop, orbital cycling
    camera.h             Orbit/pan/zoom camera (header-only)
    orbital.h            Orbital catalog: (n,l,m), names, norms, bounds (header-only)
    renderer.h           Shader programs, FBOs, draw calls
    renderer.cpp         Shader source strings, compilation, ray march + bloom + composite
    stb_easy_font.h      Vendored (same as other projects)
```

## Build

Same CMake pattern as other projects: FetchContent GLFW 3.4, glad static lib, single executable. C++20. No external math library — the vec3/mat4 types from QuaternionVis are sufficient for CPU-side camera math; all heavy math lives in GLSL.

## Performance Targets

- 60fps at 1920×1080 on a mid-range GPU (RTX 3060 / RX 6700 XT class).
- 128 ray march steps as default. 256 for quality screenshots.
- Half-resolution bloom keeps the post-processing cheap.
- The Laguerre/spherical-harmonic evaluation is ~20 arithmetic ops per sample; the bottleneck is memory bandwidth from the ray march loop, not ALU.
