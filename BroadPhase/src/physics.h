#pragma once
#include "shape.h"
#include <vector>

class PhysicsWorld {
public:
    void update(float dt, float speed_mult, float world_w, float world_h);
    void spawn_shape(float x, float y);
    void remove_shape(int index);
    void ensure_count(int target, float world_w, float world_h);

    std::vector<Shape> shapes;
    int next_id = 0;
};
