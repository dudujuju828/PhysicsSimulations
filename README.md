# PhysicsSimulations

A collection of standalone physics simulation experiments, each in its own folder.

## Projects

| Project | Description |
|---------|-------------|
| [VerletChain](VerletChain/) | 2D particle chain driven by Verlet integration with mouse interaction |
| [EulerVsVerlet](EulerVsVerlet/) | Side-by-side spring sim comparing Forward Euler and Verlet integration |

## VerletChain

A rope/chain simulation where particles are connected by distance constraints and move under gravity. Click and drag any particle to interact with the chain in real time.

Built with C++20, OpenGL 4.6, and GLFW. See the [detailed breakdown](VerletChain/media/how_it_works.md) for a full explanation of the physics and code.

<video src="VerletChain/media/demo.mp4" width="600" controls></video>
