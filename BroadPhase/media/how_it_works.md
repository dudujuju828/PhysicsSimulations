# How BroadPhase Works

This simulation visualizes **broad-phase collision detection** using Bounding Volume Hierarchies (BVH) and Axis-Aligned Bounding Boxes (AABB). Circles, triangles, and polygons bounce around the screen while the simulation builds a BVH each frame, finds overlapping AABB pairs, then runs narrow-phase SAT tests to confirm real collisions. Five toggleable visualization layers let you see the AABBs, BVH tree structure, query traversal, brute-force comparison, and narrow-phase results individually.

## Table of Contents

- [The Problem: N-Body Collision Detection](#the-problem-n-body-collision-detection)
- [Axis-Aligned Bounding Boxes](#axis-aligned-bounding-boxes)
- [The Separating Axis Theorem (SAT)](#the-separating-axis-theorem-sat)
  - [Circle vs Circle](#circle-vs-circle)
  - [Polygon vs Polygon](#polygon-vs-polygon)
  - [Circle vs Polygon](#circle-vs-polygon)
- [Bounding Volume Hierarchies](#bounding-volume-hierarchies)
- [Building the BVH](#building-the-bvh)
- [Querying the BVH](#querying-the-bvh)
  - [Single-Shape Query](#single-shape-query)
  - [Self-Query for All Pairs](#self-query-for-all-pairs)
  - [A Concrete Traversal Example](#a-concrete-traversal-example)
- [Broad Phase vs Narrow Phase](#broad-phase-vs-narrow-phase)
- [The Physics](#the-physics)
- [Rendering and Visualization Layers](#rendering-and-visualization-layers)
- [File-by-File Breakdown](#file-by-file-breakdown)

---

## The Problem: N-Body Collision Detection

Given `n` objects, how do you find which pairs are colliding? The brute-force approach tests every pair:

```
for i in 0..n:
    for j in i+1..n:
        test(shape[i], shape[j])
```

This performs `n(n-1)/2` tests. For 15 shapes, that's 105 tests. For 200 shapes, it's 19,900. The cost grows quadratically — doubling the shape count quadruples the work. Worse, each test (SAT for polygons) is itself non-trivial, involving projections along multiple axes.

The key insight: most shape pairs aren't even close to each other. If two shapes are on opposite sides of the screen, there's no point running an expensive intersection test between them. Broad-phase collision detection exists to cheaply eliminate these obvious non-collisions, leaving only plausible pairs for the expensive narrow phase.

---

## Axis-Aligned Bounding Boxes

An AABB is the tightest rectangle that encloses a shape while keeping its edges parallel to the coordinate axes. It's defined by two corners:

```
struct AABB {
    Vec2 min;  // bottom-left
    Vec2 max;  // top-right
};
```

For a circle centered at `pos` with radius `r`, the AABB is:

```
min = (pos.x - r, pos.y - r)
max = (pos.x + r, pos.y + r)
```

For a polygon, it's the component-wise min and max of all world-space vertices:

```cpp
Vec2 lo = world_verts[0], hi = world_verts[0];
for (size_t i = 1; i < world_verts.size(); ++i) {
    lo = Vec2::min(lo, world_verts[i]);
    hi = Vec2::max(hi, world_verts[i]);
}
return {lo, hi};
```

Two AABBs overlap if and only if they overlap on **both** axes simultaneously:

```cpp
bool overlaps(const AABB& o) const {
    return min.x <= o.max.x && max.x >= o.min.x &&
           min.y <= o.max.y && max.y >= o.min.y;
}
```

That's 4 comparisons. If any one fails, the boxes are separated — no collision possible.

**Why axis-aligned?** Oriented bounding boxes (OBBs) fit shapes more tightly, but their overlap test requires projections along multiple axes. AABBs trade tightness for speed: the overlap test is just 4 comparisons with no multiplications, making them ideal for a broad phase that runs on every pair the BVH produces.

---

## The Separating Axis Theorem (SAT)

The Separating Axis Theorem states: two convex shapes do **not** intersect if and only if there exists an axis along which their projections don't overlap. If projections overlap on every candidate axis, the shapes must be intersecting.

For each candidate axis, you project both shapes onto it and check:

```cpp
bool overlap_on_axis(float lo1, float hi1, float lo2, float hi2) {
    return lo1 <= hi2 && lo2 <= hi1;
}
```

The set of candidate axes to test depends on the shape combination.

### Circle vs Circle

No SAT needed — just a distance check:

```cpp
float dist_sq = (a.pos - b.pos).length_sq();
float r_sum = a.radius + b.radius;
return dist_sq <= r_sum * r_sum;
```

Two circles intersect if the distance between their centers is less than or equal to the sum of their radii. Comparing squared values avoids a square root.

### Polygon vs Polygon

The candidate axes are the **edge normals** of both polygons. For each edge `(v[i], v[(i+1) % n])`, the perpendicular axis is:

```cpp
Vec2 edge = verts[(i + 1) % n] - verts[i];
Vec2 axis = edge.perp().normalized();  // perp() returns (-y, x)
```

To project a polygon onto an axis, dot each vertex with the axis and take the min/max:

```cpp
void project_polygon(const vector<Vec2>& verts, Vec2 axis, float& lo, float& hi) {
    lo = hi = verts[0].dot(axis);
    for (size_t i = 1; i < verts.size(); ++i) {
        float d = verts[i].dot(axis);
        if (d < lo) lo = d;
        if (d > hi) hi = d;
    }
}
```

If any axis shows a gap, the polygons are separated. If all axes show overlap, they're colliding. A triangle has 3 edge normals, a hexagon has 6, so a triangle-vs-hexagon test checks 9 axes.

### Circle vs Polygon

The candidate axes are:
1. All edge normals of the polygon (same as polygon-vs-polygon)
2. The axis from the circle center to the **closest vertex** of the polygon

The closest-vertex axis catches the case where the circle slips past a polygon corner without being caught by any edge normal. The circle is projected as `[center.dot(axis) - radius, center.dot(axis) + radius]`.

---

## Bounding Volume Hierarchies

A BVH is a binary tree where each node holds an AABB:

- **Leaf nodes** store a single shape index. Their AABB is that shape's bounding box.
- **Internal nodes** store an AABB that encloses both children. They don't correspond to any shape directly.

```cpp
struct BVHNode {
    AABB bounds;
    int  left = -1;         // child index (-1 for leaf)
    int  right = -1;
    int  shape_index = -1;  // >= 0 only for leaves
    int  depth = 0;
    int  subtree_size = 1;  // leaf count in subtree
};
```

The root node's AABB encloses everything. Each level of the tree subdivides space further. To check whether shape X overlaps anything, you start at the root:

- If X's AABB doesn't overlap this node's AABB, **prune** the entire subtree. None of its descendants can overlap X.
- If it does overlap and this is a leaf, you have a candidate pair.
- If it does overlap and this is an internal node, recurse into both children.

This turns `O(n)` brute-force scanning into `O(log n)` for each query when shapes are spatially distributed. Finding all pairs goes from `O(n^2)` brute force to `O(n log n)` average case.

---

## Building the BVH

The simulation rebuilds the BVH every frame using a top-down median-split strategy:

```cpp
int BVH::build_recursive(vector<int>& indices, const vector<AABB>& aabbs, int depth) {
    // 1. Compute AABB enclosing all shapes in this subset
    AABB bounds = aabbs[indices[0]];
    for (size_t i = 1; i < indices.size(); ++i)
        bounds = bounds.merged(aabbs[indices[i]]);

    // 2. If one shape, create a leaf
    if (indices.size() == 1) {
        // store bounds, shape_index, return
    }

    // 3. Choose split axis: the longest extent of the bounding box
    bool split_x = (bounds.width() >= bounds.height());

    // 4. Sort by centroid along that axis
    sort(indices.begin(), indices.end(), [&](int a, int b) {
        if (split_x) return aabbs[a].center().x < aabbs[b].center().x;
        else         return aabbs[a].center().y < aabbs[b].center().y;
    });

    // 5. Split at the median
    size_t mid = indices.size() / 2;

    // 6. Recurse on left half and right half
    int left_idx  = build_recursive(left_indices, aabbs, depth + 1);
    int right_idx = build_recursive(right_indices, aabbs, depth + 1);
}
```

**Why longest axis?** Splitting along the longest extent of the current bounding box tends to produce children with minimal overlap, since it divides the most spread-out dimension. This isn't optimal (surface area heuristic would be better), but it's simple and fast.

**Why median split?** Splitting at the median of sorted centroids produces balanced subtrees. With `n` shapes, the tree depth is `O(log n)`. The build involves sorting at each level, giving `O(n log^2 n)` total — or `O(n log n)` with nth_element instead of a full sort.

The node array is stored in a flat `vector<BVHNode>`. Children are referenced by index rather than pointers. Memory is reserved for `2n` nodes up front (a binary tree with `n` leaves has at most `2n - 1` nodes).

---

## Querying the BVH

### Single-Shape Query

To find all shapes whose AABBs overlap shape X, walk the tree with a stack-based DFS:

```cpp
vector<int> BVH::query(const AABB& query_box, int exclude_index) const {
    vector<int> results;
    vector<int> stack = {0};  // start at root

    while (!stack.empty()) {
        int idx = stack.back();
        stack.pop_back();

        const BVHNode& n = nodes_[idx];
        if (!n.bounds.overlaps(query_box)) continue;  // prune

        if (n.shape_index >= 0) {
            if (n.shape_index != exclude_index)
                results.push_back(n.shape_index);      // leaf hit
        } else {
            stack.push_back(n.left);                    // descend
            stack.push_back(n.right);
        }
    }
    return results;
}
```

Each AABB overlap test either prunes an entire subtree or descends one level. In a balanced tree with 200 shapes, the tree is ~8 levels deep. A query that prunes early might only visit 10-15 nodes instead of all 200.

### Self-Query for All Pairs

Rather than querying each shape individually (which would give `O(n log n)` but with duplicates to filter), the simulation uses a **recursive self-query** that traverses two subtrees simultaneously:

```cpp
void BVH::self_query(int a, int b, vector<pair<int,int>>& pairs) const {
    if (!nodes_[a].bounds.overlaps(nodes_[b].bounds)) return;

    if (both are leaves) {
        if (a != b) pairs.push_back({a, b});
        return;
    }

    if (a == b) {
        // Same subtree: check left-left, right-right, left-right
        self_query(left, left, pairs);
        self_query(right, right, pairs);
        self_query(left, right, pairs);
    } else {
        // Different subtrees: descend the larger one
        // This balances the recursion
    }
}
```

Called as `self_query(root, root, pairs)`, this finds all overlapping leaf pairs without duplicates. The "descend the larger subtree" heuristic keeps the recursion balanced.

### A Concrete Traversal Example

Suppose we have 6 shapes arranged like this (shapes 0-2 on the left, 3-5 on the right):

```
+---+---+---+---+---+---+---+---+
|       |       |       |       |
|  [0]  |  [1]  |       |  [3]  |
|       |       |       |       |
+---+---+---+---+---+---+---+---+
|       |       |       |       |
|       |  [2]  |       | [4][5]|
|       |       |       |       |
+---+---+---+---+---+---+---+---+
```

The BVH might look like:

```
         [Root: all shapes]
           /            \
    [Left: 0,1,2]    [Right: 3,4,5]
      /       \         /       \
   [0,1]     [2]    [3]      [4,5]
   /   \                     /   \
 [0]   [1]                 [4]   [5]
```

Now query shape 4's AABB against the tree:

| Step | Node | Action | Reason |
|------|------|--------|--------|
| 1 | Root | **Visit** | Root AABB overlaps shape 4's AABB (it overlaps everything) |
| 2 | Left (0,1,2) | **Prune** | Left subtree's AABB is entirely on the left side — no overlap with shape 4 on the right. Skips 3 shapes in one test! |
| 3 | Right (3,4,5) | **Visit** | Right subtree overlaps shape 4 |
| 4 | Leaf 3 | **Leaf test** | AABB overlap check → candidate pair (3,4) |
| 5 | Node (4,5) | **Visit** | Overlaps |
| 6 | Leaf 5 | **Leaf test** | AABB overlap check → candidate pair (4,5) |

Total: 6 node visits instead of testing all 5 other shapes. The prune at step 2 eliminated half the tree in one comparison.

---

## Broad Phase vs Narrow Phase

The full collision pipeline has two stages:

1. **Broad phase** — Find all pairs of shapes whose AABBs overlap. This uses the BVH (or brute force, toggled with `B`). The output is a list of candidate pairs.
2. **Narrow phase** — For each candidate pair, run the exact SAT intersection test. This confirms whether the actual shapes (not just their boxes) collide.

The broad phase produces **false positives**: pairs where the AABBs overlap but the actual shapes don't touch. Consider a tall thin triangle and a short wide rectangle — their AABBs can overlap even when the shapes are clearly apart. But it never produces false negatives: if two shapes are colliding, their AABBs must overlap.

```
Broad phase candidates:  [(0,3), (1,2), (4,5), (2,4)]
After narrow phase:      [(1,2), (4,5)]  ← actual collisions
False positives:         [(0,3), (2,4)]  ← AABBs overlapped but shapes didn't
```

The stats panel shows this breakdown in real time. With the narrow-phase layer (key `5`) enabled, colliding shapes turn green and false-positive shapes turn yellow.

---

## The Physics

Each shape has a position, velocity, and rotation angle. The simulation uses **forward Euler integration**:

```cpp
s.pos += s.vel * (dt * speed_mult);
s.rotation += 0.5f * (dt * speed_mult);
```

Every shape rotates at a constant 0.5 radians/second regardless of its velocity. Polygons and triangles recompute their world-space vertices after each position/rotation update:

```cpp
// Rotate local vertices by the current angle and translate to world position
float c = cos(rotation), s = sin(rotation);
world_verts[i] = { pos.x + v.x*c - v.y*s,
                   pos.y + v.x*s + v.y*c };
```

**Wall bounce** uses the shape's AABB half-extents rather than a fixed radius, so non-circular shapes bounce correctly even as they rotate and their AABB dimensions change:

```cpp
AABB box = s.compute_aabb();
float half_w = (box.max.x - box.min.x) * 0.5f;
float half_h = (box.max.y - box.min.y) * 0.5f;

if (s.pos.x - half_w < margin) { s.pos.x = margin + half_w; s.vel.x = abs(s.vel.x); }
// ... same for right, bottom, top walls
```

Shapes are created with random type (circle, triangle, or 4-7 sided polygon), random size (radius 12-30 pixels), random pastel color, and random velocity (30-120 pixels/second in a random direction).

---

## Rendering and Visualization Layers

The simulation has five toggleable layers (keys `1` through `5`), each adding a different visualization on top of the base scene:

**Base scene** (always visible): Filled shapes with dimmed outlines on a dark background. Selected shapes get a white outline. Hovered shapes brighten slightly.

| Key | Layer | What It Shows |
|-----|-------|---------------|
| `1` | **AABB Overlay** | Draws each shape's axis-aligned bounding box. Blue boxes for shapes not in any broad-phase pair. Orange boxes for shapes whose AABBs overlap at least one other. |
| `2` | **BVH Tree** | Draws the internal nodes of the BVH as depth-colored rectangles (red at root, cycling through orange, yellow, green, cyan, blue at leaves). Also shows a node-link tree diagram in the upper-right corner. Press `R` to replay the build animation node by node. |
| `3` | **Query Visualization** | Click a shape to select it, then see how the BVH queries for that shape's neighbors. Green outlines = visited nodes. Red boxes = pruned subtrees. Green filled boxes = leaf hits. When paused, press `N` to step through the traversal one node at a time with annotations. |
| `4` | **Brute Force Comparison** | Draws lines between all brute-force AABB overlap pairs (dim gray) overlaid with the BVH's pairs (bright white). Verifies that BVH produces the exact same pair set. Shows how many tests the BVH saves as a percentage. |
| `5` | **Narrow Phase** | Colors shapes by their collision status: green = actually colliding (SAT confirmed), yellow = false positive (AABB overlapped but shapes don't intersect). Stats panel shows collision count and false-positive count. |

Additional controls: `B` toggles between BVH and brute-force mode. `Space` pauses. `+`/`-` adjust speed. Right-click spawns a new shape. Sliders control object count (5-200) and speed multiplier (0-3x).

---

## File-by-File Breakdown

### `vec2.h`

2-component float vector with arithmetic operators.

| Method | Purpose |
|--------|---------|
| `operator+`, `-`, `*`, `/` | Vector arithmetic |
| `dot(o)` | Dot product |
| `cross(o)` | 2D cross product (scalar) |
| `length_sq()` / `length()` | Squared and actual length |
| `normalized()` | Unit vector (returns zero for zero-length) |
| `perp()` | Perpendicular vector `(-y, x)` |
| `Vec2::min(a, b)` / `Vec2::max(a, b)` | Component-wise min/max |

### `aabb.h`

Axis-aligned bounding box defined by min/max corners.

| Method | Purpose |
|--------|---------|
| `width()` / `height()` | Box dimensions |
| `center()` | Midpoint of the box |
| `area()` | Width times height |
| `overlaps(o)` | 4-comparison overlap test |
| `contains(p)` | Point-in-box test |
| `merged(o)` | Smallest AABB enclosing both |
| `expanded(margin)` | Grow box by margin on all sides |

### `shape.h` / `shape.cpp`

Shape definition, factory functions, and SAT collision detection.

| Method / Function | Purpose |
|-------------------|---------|
| `Shape::update_world_verts()` | Rotate local vertices by current angle, translate to world position |
| `Shape::compute_aabb()` | Compute the AABB from radius (circle) or world vertices (polygon) |
| `Shape::contains_point(p)` | Hit test for mouse picking (distance check for circles, winding number for polygons) |
| `make_circle(...)` | Factory: create a circle shape |
| `make_triangle(...)` | Factory: create an equilateral triangle from 3 vertices spaced 120 degrees apart |
| `make_polygon(...)` | Factory: create a regular polygon with 4-7 sides |
| `make_random_shape(...)` | Factory: random type, size, velocity, and pastel color |
| `shapes_intersect(a, b)` | Dispatch to the correct SAT routine based on shape types |

### `bvh.h` / `bvh.cpp`

BVH construction, self-query, and single-shape query.

| Method / Function | Purpose |
|-------------------|---------|
| `BVH::build(aabbs)` | Build the full tree from a vector of AABBs |
| `BVH::build_recursive(...)` | Top-down median-split recursive builder |
| `BVH::find_all_pairs()` | Find all overlapping AABB pairs via `self_query(root, root)` |
| `BVH::self_query(a, b, pairs)` | Dual-tree traversal: simultaneously descend two subtrees, prune when bounds don't overlap |
| `BVH::query(box, exclude)` | Stack-based DFS: find all shapes overlapping a given AABB |
| `BVH::query_with_steps(...)` | Same as `query` but records each visit/prune/leaf-test for the step-through visualization |
| `brute_force_pairs(aabbs)` | O(n^2) all-pairs AABB overlap test for comparison |

### `physics.h` / `physics.cpp`

Physics world: integration and wall bounce.

| Method | Purpose |
|--------|---------|
| `PhysicsWorld::update(dt, speed, w, h)` | Euler integration, rotation, wall bounce using AABB half-extents |
| `PhysicsWorld::spawn_shape(x, y)` | Create a random shape at a given position |
| `PhysicsWorld::remove_shape(index)` | Remove a shape by index |
| `PhysicsWorld::ensure_count(target, w, h)` | Add or remove shapes to match the slider value |

### `ui.h`

UI state: layer toggles, selection, slider state, and per-frame statistics.

| Field Group | Contents |
|-------------|----------|
| Layer toggles | `show_aabb_overlay`, `show_bvh_tree`, `show_query_vis`, `show_brute_compare`, `show_narrow_phase` |
| Mode flags | `use_bvh`, `paused`, `step_mode` |
| Selection | `selected_shape`, `hovered_shape`, `dragged_shape`, `drag_offset` |
| Animation | `step_index`, `build_anim_active`, `build_anim_step` |
| Stats | `broad_phase_pairs`, `brute_force_pairs`, `narrow_phase_tests`, `actual_collisions`, `false_positives`, `bvh_node_count`, `fps`, `bvh_mismatch` |

### `renderer.h` / `renderer.cpp`

OpenGL renderer with geometry and text pipelines.

| Method | Purpose |
|--------|---------|
| `init()` / `cleanup()` | Create and destroy GPU resources (shaders, VAOs, VBOs) |
| `draw_filled_circle(...)` | Triangle fan circle |
| `draw_circle_outline(...)` | Line strip circle |
| `draw_filled_polygon(...)` / `draw_polygon_outline(...)` | Triangle fan / line strip polygon |
| `draw_rect_outline(...)` / `draw_filled_rect(...)` | AABB rectangle primitives |
| `draw_text(...)` | Screen-space text via `stb_easy_font` |
| `render_frame(...)` | Main entry point: draws all layers in order |
| `render_bvh_boxes(...)` | Depth-colored internal node rectangles (rainbow gradient: root=red, leaves=blue) |
| `render_aabb_overlays(...)` | Per-shape AABB boxes colored by overlap status |
| `render_query_vis(...)` | Traversal step visualization with step-through annotations |
| `render_tree_diagram(...)` | Node-link BVH tree diagram in the upper-right corner |
| `render_pair_lines(...)` | Lines between shape centers for brute-force/BVH pair comparison |
| `render_sliders(...)` | Object count and speed sliders |
| `render_stats(...)` | FPS, shape count, mode, pair counts, BVH verification |

### `main.cpp`

Entry point: GLFW window, input callbacks, main loop orchestration.

| Component | Purpose |
|-----------|---------|
| `AppState` | Holds `PhysicsWorld`, `BVH`, `Renderer`, `UIState`, per-frame computed data |
| `SliderDef` / `slider_hit` / `slider_value` | Slider geometry and hit testing |
| `key_callback` | Keys 1-5 toggle layers, B toggles mode, Space pauses, N steps, R replays build, +/- adjust speed |
| `mouse_button_callback` | Left-click: slider drag or shape selection. Right-click: spawn shape |
| `cursor_pos_callback` | Slider dragging, shape dragging, hover detection |
| Main loop | Each frame: ensure shape count, physics update, compute AABBs, build BVH, broad phase, brute force (if layer 4), narrow phase, query steps (if selection + layer 3), render |
