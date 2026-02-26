# VerletChain

## Overview

A 2D particle chain simulation driven by Verlet integration. A series of particles are linked together to form a rope-like chain that swings and settles under gravity, with real-time mouse interaction.

## Core Elements

### Particle Chain
A sequence of particles connected by fixed-length distance constraints. The chain behaves like a flexible rope or string — particles influence their neighbors but the overall structure can bend and swing freely.

### Verlet Integration
All particle motion is computed using Verlet integration. Each particle's next position is derived from its current position, previous position, and applied forces — no explicit velocity storage.

### Gravity
A constant downward gravitational force acts on every particle in the chain, pulling it toward the bottom of the simulation area.

### Mouse Interaction
The user can click and drag any particle in the chain. While held, the grabbed particle follows the mouse cursor, and the rest of the chain reacts accordingly. Releasing the mouse lets the chain resume free motion.
