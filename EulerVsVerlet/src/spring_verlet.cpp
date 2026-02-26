#include "spring_verlet.h"

void SpringVerlet::reset(Vec2 anchor, Vec2 offset, float stiffness, float mass) {
    anchor_   = anchor;
    pos_      = anchor + offset;
    prev_pos_ = pos_;
    k_        = stiffness;
    mass_     = mass;
    trail_.clear();
}

void SpringVerlet::step(float dt) {
    Vec2 displacement = pos_ - prev_pos_;
    Vec2 d = pos_ - anchor_;
    Vec2 accel = d * (-k_ / mass_);

    prev_pos_ = pos_;
    pos_ = pos_ + displacement + accel * (dt * dt);
    trail_.push(pos_);
}

float SpringVerlet::energy(float dt) const {
    Vec2 derived_vel = (pos_ - prev_pos_) * (1.0f / dt);
    Vec2 d = pos_ - anchor_;
    float pe = 0.5f * k_ * d.length_sq();
    float ke = 0.5f * mass_ * derived_vel.length_sq();
    return pe + ke;
}
