#pragma once
#include "vec2.h"
#include <cstddef>
#include <span>
#include <vector>

struct Particle {
    Vec2 pos;
    Vec2 prev_pos;
    bool pinned = false;
};

struct Constraint {
    std::size_t a;
    std::size_t b;
    float rest_length;
};

class Chain {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    Chain(Vec2 anchor_pos, int num_particles, float segment_length);

    void update(float dt, Vec2 gravity, int constraint_iterations);

    std::span<const Vec2> positions() const;
    std::size_t size() const;

    void set_particle_pos(std::size_t index, Vec2 pos);
    void set_pinned(std::size_t index, bool pinned);
    bool is_pinned(std::size_t index) const;

    std::size_t find_nearest(Vec2 pos, float max_dist) const;

private:
    std::vector<Particle> particles_;
    std::vector<Constraint> constraints_;
    std::vector<Vec2> pos_cache_;

    void integrate(float dt, Vec2 gravity);
    void solve_constraints(int iterations);
    void sync_pos_cache();
};
