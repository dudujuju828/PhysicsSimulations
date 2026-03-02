# PhysicsSimulations

A collection of standalone physics simulation experiments, each in its own folder.

## Projects

| Project | Description | Docs |
|---------|-------------|------|
| [VerletChain](VerletChain/) | 2D particle chain driven by Verlet integration with mouse interaction | [how it works](VerletChain/media/how_it_works.md) |
| [EulerVsVerlet](EulerVsVerlet/) | Side-by-side spring sim comparing Forward Euler and Verlet integration | [how it works](EulerVsVerlet/media/how_it_works.md) \| [spec](EulerVsVerlet/spec.md) |
| [QuaternionVis](QuaternionVis/) | 3D quaternion rotation visualizer comparing SLERP and LERP interpolation | [how it works](QuaternionVis/media/how_it_works.md) \| [spec](QuaternionVis/spec.md) |
| [ElectronOrbitals](ElectronOrbitals/) | Volumetric hydrogen orbital renderer using real spherical harmonics | [spec](ElectronOrbitals/spec.md) |
| [BroadPhase](BroadPhase/) | Interactive BVH/AABB broad-phase collision detection visualizer | [how it works](BroadPhase/media/how_it_works.md) |

## VerletChain

A rope/chain simulation where particles are connected by distance constraints and move under gravity. Click and drag any particle to interact with the chain in real time.

Built with C++20, OpenGL 4.6, and GLFW. See the [detailed breakdown](VerletChain/media/how_it_works.md) for a full explanation of the physics and code.

<video src="VerletChain/media/demo.mp4" width="600" controls></video>

## EulerVsVerlet

Two identical ball-on-spring systems run side by side — left uses forward Euler, right uses Stormer-Verlet. The simulation cycles through parameter presets (stiff springs, heavy damping, large timesteps) that make the differences progressively obvious: Euler gains energy and spirals outward while Verlet stays stable. A real-time energy graph and numeric readouts let you watch the divergence happen.

Built with C++20, OpenGL 4.6, and GLFW. See the [detailed breakdown](EulerVsVerlet/media/how_it_works.md) for a full explanation of the physics and code.

## QuaternionVis

Split-screen 3D visualization comparing SLERP and LERP (nlerp) quaternion interpolation. Both viewports show a coordinate frame rotating between the same two orientations on a wireframe unit sphere. Time markers placed at equal parameter intervals reveal the speed distortion: evenly spaced on the SLERP side, bunched at endpoints on the LERP side. Five presets range from 180-degree rotations (maximum distortion) to 10-degree rotations (where LERP is effectively perfect).

Built with C++20, OpenGL 4.6, and GLFW. See the [detailed breakdown](QuaternionVis/media/how_it_works.md) for a full explanation of the physics and code.

## ElectronOrbitals

Real-time volumetric renderer of hydrogen electron orbital probability clouds. GPU ray marching evaluates the full wave function — associated Laguerre polynomials and real spherical harmonics — entirely in the fragment shader. Supports all orbitals from 1s through 4f (30 orbitals total). HDR bloom, two-tone phase coloring, and animated density perturbation give the clouds a bioluminescent, nebula-like appearance. Arrow keys cycle through orbitals, mouse orbits the camera.

Built with C++20, OpenGL 4.6, and GLFW.

## BroadPhase

Interactive visualizer for broad-phase collision detection using Bounding Volume Hierarchies. Circles, triangles, and polygons bounce around while a BVH is rebuilt each frame to find overlapping AABB pairs, followed by narrow-phase SAT tests for exact collision. Five toggleable layers let you inspect AABBs, the BVH tree structure, step-by-step query traversal, brute-force comparison, and narrow-phase results. Click a shape and press N to walk through the BVH traversal node by node.

Built with C++20, OpenGL 4.6, and GLFW. See the [detailed breakdown](BroadPhase/media/how_it_works.md) for a full explanation of the physics and code.
