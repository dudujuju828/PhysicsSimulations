#include "shape.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <limits>

static float randf() { return static_cast<float>(rand()) / RAND_MAX; }
static float randf(float lo, float hi) { return lo + randf() * (hi - lo); }

// --- Shape methods ---

void Shape::update_world_verts() {
    float c = std::cos(rotation);
    float s = std::sin(rotation);
    world_verts.resize(local_verts.size());
    for (size_t i = 0; i < local_verts.size(); ++i) {
        Vec2 v = local_verts[i];
        world_verts[i] = {pos.x + v.x * c - v.y * s,
                          pos.y + v.x * s + v.y * c};
    }
}

AABB Shape::compute_aabb() const {
    if (type == ShapeType::Circle) {
        return {{pos.x - radius, pos.y - radius},
                {pos.x + radius, pos.y + radius}};
    }
    Vec2 lo = world_verts[0], hi = world_verts[0];
    for (size_t i = 1; i < world_verts.size(); ++i) {
        lo = Vec2::min(lo, world_verts[i]);
        hi = Vec2::max(hi, world_verts[i]);
    }
    return {lo, hi};
}

bool Shape::contains_point(Vec2 p) const {
    if (type == ShapeType::Circle) {
        return (p - pos).length_sq() <= radius * radius;
    }
    // Point-in-polygon (winding number / cross product test)
    int n = static_cast<int>(world_verts.size());
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        Vec2 vi = world_verts[i], vj = world_verts[j];
        if ((vi.y > p.y) != (vj.y > p.y) &&
            p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y) + vi.x)
            inside = !inside;
    }
    return inside;
}

// --- Factory functions ---

Shape make_circle(Vec2 pos, float radius, Vec2 vel, std::array<float,3> color, int id) {
    Shape s;
    s.type = ShapeType::Circle;
    s.pos = pos;
    s.radius = radius;
    s.vel = vel;
    s.color = color;
    s.id = id;
    return s;
}

Shape make_triangle(Vec2 pos, float size, Vec2 vel, std::array<float,3> color, int id) {
    Shape s;
    s.type = ShapeType::Triangle;
    s.pos = pos;
    s.vel = vel;
    s.color = color;
    s.id = id;
    s.local_verts.resize(3);
    for (int i = 0; i < 3; ++i) {
        float angle = static_cast<float>(i) * 2.0f * 3.14159265f / 3.0f - 3.14159265f / 2.0f;
        s.local_verts[i] = {size * std::cos(angle), size * std::sin(angle)};
    }
    s.update_world_verts();
    return s;
}

Shape make_polygon(Vec2 pos, float size, int sides, Vec2 vel, std::array<float,3> color, int id) {
    Shape s;
    s.type = ShapeType::Polygon;
    s.pos = pos;
    s.vel = vel;
    s.color = color;
    s.id = id;
    s.local_verts.resize(sides);
    for (int i = 0; i < sides; ++i) {
        float angle = static_cast<float>(i) * 2.0f * 3.14159265f / sides - 3.14159265f / 2.0f;
        s.local_verts[i] = {size * std::cos(angle), size * std::sin(angle)};
    }
    s.update_world_verts();
    return s;
}

Shape make_random_shape(float world_w, float world_h, int id) {
    float margin = 60.0f;
    Vec2 pos = {randf(margin, world_w - margin), randf(margin, world_h - margin)};
    float speed = randf(30.0f, 120.0f);
    float angle = randf(0.0f, 2.0f * 3.14159265f);
    Vec2 vel = {speed * std::cos(angle), speed * std::sin(angle)};

    // Random pastel-ish color
    std::array<float,3> color = {randf(0.4f, 0.9f), randf(0.4f, 0.9f), randf(0.4f, 0.9f)};

    int choice = rand() % 3;
    if (choice == 0) {
        return make_circle(pos, randf(12.0f, 30.0f), vel, color, id);
    } else if (choice == 1) {
        return make_triangle(pos, randf(15.0f, 30.0f), vel, color, id);
    } else {
        int sides = 4 + rand() % 4; // 4-7 sides
        return make_polygon(pos, randf(15.0f, 28.0f), sides, vel, color, id);
    }
}

// --- Narrow-phase SAT collision detection ---

// Project a circle onto an axis, return [min, max]
static void project_circle(Vec2 center, float radius, Vec2 axis, float& lo, float& hi) {
    float d = center.dot(axis);
    lo = d - radius;
    hi = d + radius;
}

// Project polygon verts onto an axis, return [min, max]
static void project_polygon(const std::vector<Vec2>& verts, Vec2 axis, float& lo, float& hi) {
    lo = hi = verts[0].dot(axis);
    for (size_t i = 1; i < verts.size(); ++i) {
        float d = verts[i].dot(axis);
        if (d < lo) lo = d;
        if (d > hi) hi = d;
    }
}

static bool overlap_on_axis(float lo1, float hi1, float lo2, float hi2) {
    return lo1 <= hi2 && lo2 <= hi1;
}

// Get edge normals for polygon
static std::vector<Vec2> get_axes(const std::vector<Vec2>& verts) {
    std::vector<Vec2> axes;
    int n = static_cast<int>(verts.size());
    for (int i = 0; i < n; ++i) {
        Vec2 edge = verts[(i + 1) % n] - verts[i];
        axes.push_back(edge.perp().normalized());
    }
    return axes;
}

// Circle vs Circle
static bool circle_vs_circle(const Shape& a, const Shape& b) {
    float dist_sq = (a.pos - b.pos).length_sq();
    float r_sum = a.radius + b.radius;
    return dist_sq <= r_sum * r_sum;
}

// Polygon vs Polygon (SAT)
static bool polygon_vs_polygon(const std::vector<Vec2>& va, const std::vector<Vec2>& vb) {
    auto axes_a = get_axes(va);
    auto axes_b = get_axes(vb);

    for (auto& axis : axes_a) {
        float lo1, hi1, lo2, hi2;
        project_polygon(va, axis, lo1, hi1);
        project_polygon(vb, axis, lo2, hi2);
        if (!overlap_on_axis(lo1, hi1, lo2, hi2)) return false;
    }
    for (auto& axis : axes_b) {
        float lo1, hi1, lo2, hi2;
        project_polygon(va, axis, lo1, hi1);
        project_polygon(vb, axis, lo2, hi2);
        if (!overlap_on_axis(lo1, hi1, lo2, hi2)) return false;
    }
    return true;
}

// Circle vs Polygon (SAT)
static bool circle_vs_polygon(const Shape& circle, const std::vector<Vec2>& verts) {
    // Find closest vertex to circle center
    float best_dist_sq = std::numeric_limits<float>::max();
    Vec2 closest{};
    for (auto& v : verts) {
        float d = (v - circle.pos).length_sq();
        if (d < best_dist_sq) { best_dist_sq = d; closest = v; }
    }

    // Test axis from circle center to closest vertex
    Vec2 axis = (closest - circle.pos).normalized();
    {
        float lo1, hi1, lo2, hi2;
        project_circle(circle.pos, circle.radius, axis, lo1, hi1);
        project_polygon(verts, axis, lo2, hi2);
        if (!overlap_on_axis(lo1, hi1, lo2, hi2)) return false;
    }

    // Test polygon edge normals
    auto axes = get_axes(verts);
    for (auto& ax : axes) {
        float lo1, hi1, lo2, hi2;
        project_circle(circle.pos, circle.radius, ax, lo1, hi1);
        project_polygon(verts, ax, lo2, hi2);
        if (!overlap_on_axis(lo1, hi1, lo2, hi2)) return false;
    }
    return true;
}

bool shapes_intersect(const Shape& a, const Shape& b) {
    bool a_circle = (a.type == ShapeType::Circle);
    bool b_circle = (b.type == ShapeType::Circle);

    if (a_circle && b_circle)
        return circle_vs_circle(a, b);
    if (a_circle && !b_circle)
        return circle_vs_polygon(a, b.world_verts);
    if (!a_circle && b_circle)
        return circle_vs_polygon(b, a.world_verts);
    return polygon_vs_polygon(a.world_verts, b.world_verts);
}
