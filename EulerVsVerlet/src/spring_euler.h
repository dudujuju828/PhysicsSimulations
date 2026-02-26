#pragma once
#include "vec2.h"

class SpringEuler {
public:
    void reset(Vec2 anchor, Vec2 offset, float stiffness, float mass);
    void step(float dt);

    Vec2  pos()    const { return pos_; }
    Vec2  vel()    const { return vel_; }
    Vec2  anchor() const { return anchor_; }
    float energy() const;

    const Trail& trail() const { return trail_; }

private:
    Vec2  anchor_ = {};
    Vec2  pos_    = {};
    Vec2  vel_    = {};
    float k_      = 0.0f;
    float mass_   = 1.0f;
    Trail trail_;
};
