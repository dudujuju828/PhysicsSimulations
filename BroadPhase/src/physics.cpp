#include "physics.h"
#include <cstdlib>
#include <cmath>

void PhysicsWorld::update(float dt, float speed_mult, float world_w, float world_h) {
    float eff_dt = dt * speed_mult;
    float margin = 5.0f;

    for (auto& s : shapes) {
        s.pos += s.vel * eff_dt;
        s.rotation += 0.5f * eff_dt;

        // Compute shape extent for wall bounce
        AABB box = s.compute_aabb();
        float half_w = (box.max.x - box.min.x) * 0.5f;
        float half_h = (box.max.y - box.min.y) * 0.5f;

        if (s.pos.x - half_w < margin)        { s.pos.x = margin + half_w;        s.vel.x = std::abs(s.vel.x); }
        if (s.pos.x + half_w > world_w - margin) { s.pos.x = world_w - margin - half_w; s.vel.x = -std::abs(s.vel.x); }
        if (s.pos.y - half_h < margin)        { s.pos.y = margin + half_h;        s.vel.y = std::abs(s.vel.y); }
        if (s.pos.y + half_h > world_h - margin) { s.pos.y = world_h - margin - half_h; s.vel.y = -std::abs(s.vel.y); }

        // Update world verts after position change
        if (s.type != ShapeType::Circle)
            s.update_world_verts();
    }
}

void PhysicsWorld::spawn_shape(float x, float y) {
    Shape s = make_random_shape(1.0f, 1.0f, next_id++); // size doesn't matter, we override pos
    s.pos = {x, y};
    if (s.type != ShapeType::Circle) s.update_world_verts();
    shapes.push_back(std::move(s));
}

void PhysicsWorld::remove_shape(int index) {
    if (index >= 0 && index < static_cast<int>(shapes.size())) {
        shapes.erase(shapes.begin() + index);
    }
}

void PhysicsWorld::ensure_count(int target, float world_w, float world_h) {
    while (static_cast<int>(shapes.size()) < target) {
        shapes.push_back(make_random_shape(world_w, world_h, next_id++));
    }
    while (static_cast<int>(shapes.size()) > target) {
        shapes.pop_back();
    }
}
