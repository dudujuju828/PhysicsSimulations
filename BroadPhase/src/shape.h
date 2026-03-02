#pragma once
#include "vec2.h"
#include "aabb.h"
#include <vector>
#include <array>
#include <cstdint>

enum class ShapeType { Circle, Triangle, Polygon };

struct Shape {
    ShapeType type = ShapeType::Circle;
    Vec2      pos{};
    Vec2      vel{};
    float     rotation = 0.0f;
    float     radius   = 0.0f;          // Circle only
    std::vector<Vec2> local_verts;       // Triangle/Polygon (relative to pos)
    std::vector<Vec2> world_verts;       // Transformed verts (pos + rotation)
    std::array<float, 3> color{};
    int       id = 0;

    void update_world_verts();
    AABB compute_aabb() const;
    bool contains_point(Vec2 p) const;
};

Shape make_circle(Vec2 pos, float radius, Vec2 vel, std::array<float,3> color, int id);
Shape make_triangle(Vec2 pos, float size, Vec2 vel, std::array<float,3> color, int id);
Shape make_polygon(Vec2 pos, float size, int sides, Vec2 vel, std::array<float,3> color, int id);
Shape make_random_shape(float world_w, float world_h, int id);

bool shapes_intersect(const Shape& a, const Shape& b);
