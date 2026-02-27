#pragma once
#include "vec2.h"

class SpringVerlet {
public:
    void reset(Vec2 anchor, Vec2 offset, float stiffness, float mass,
               float damping = 0.0f);
    void step(float dt);

    Vec2  pos()      const { return pos_; }
    Vec2  prev_pos() const { return prev_pos_; }
    Vec2  anchor()   const { return anchor_; }
    float energy(float dt) const;

    const Trail& trail() const { return trail_; }

private:
    Vec2  anchor_   = {};
    Vec2  pos_      = {};
    Vec2  prev_pos_ = {};
    float k_       = 0.0f;
    float mass_    = 1.0f;
    float damping_ = 0.0f;
    Trail trail_;
};
