#include "spring_euler.h"

void SpringEuler::reset(Vec2 anchor, Vec2 offset, float stiffness, float mass) {
    anchor_ = anchor;
    pos_    = anchor + offset;
    vel_    = {0.0f, 0.0f};
    k_      = stiffness;
    mass_   = mass;
    trail_.clear();
}

void SpringEuler::step(float dt) {
    Vec2 displacement = pos_ - anchor_;
    Vec2 accel = displacement * (-k_ / mass_);
    vel_ += accel * dt;
    pos_ += vel_ * dt;
    trail_.push(pos_);
}

float SpringEuler::energy() const {
    Vec2 d = pos_ - anchor_;
    float pe = 0.5f * k_ * d.length_sq();
    float ke = 0.5f * mass_ * vel_.length_sq();
    return pe + ke;
}
