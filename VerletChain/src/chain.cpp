#include "chain.h"

Chain::Chain(Vec2 anchor_pos, int num_particles, float segment_length) {
    particles_.reserve(num_particles);
    for (int i = 0; i < num_particles; ++i) {
        Particle p;
        p.pos = {anchor_pos.x, anchor_pos.y - i * segment_length};
        p.prev_pos = p.pos;
        p.pinned = (i == 0);
        particles_.push_back(p);
    }

    constraints_.reserve(num_particles - 1);
    for (int i = 0; i < num_particles - 1; ++i) {
        constraints_.push_back({
            static_cast<std::size_t>(i),
            static_cast<std::size_t>(i + 1),
            segment_length
        });
    }

    pos_cache_.resize(num_particles);
    sync_pos_cache();
}

void Chain::update(float dt, Vec2 gravity, int constraint_iterations) {
    integrate(dt, gravity);
    solve_constraints(constraint_iterations);
    sync_pos_cache();
}

std::span<const Vec2> Chain::positions() const {
    return pos_cache_;
}

std::size_t Chain::size() const {
    return particles_.size();
}

void Chain::set_particle_pos(std::size_t index, Vec2 pos) {
    particles_[index].pos = pos;
    particles_[index].prev_pos = pos;
}

void Chain::set_pinned(std::size_t index, bool pinned) {
    particles_[index].pinned = pinned;
}

bool Chain::is_pinned(std::size_t index) const {
    return particles_[index].pinned;
}

std::size_t Chain::find_nearest(Vec2 pos, float max_dist) const {
    std::size_t best = npos;
    float best_dist_sq = max_dist * max_dist;

    for (std::size_t i = 0; i < particles_.size(); ++i) {
        float dist_sq = (particles_[i].pos - pos).length_sq();
        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best = i;
        }
    }
    return best;
}

void Chain::integrate(float dt, Vec2 gravity) {
    Vec2 gravity_step = gravity * (dt * dt);
    for (auto& p : particles_) {
        if (p.pinned) continue;
        Vec2 displacement = p.pos - p.prev_pos;
        p.prev_pos = p.pos;
        p.pos = p.pos + displacement + gravity_step;
    }
}

void Chain::solve_constraints(int iterations) {
    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& [a, b, rest] : constraints_) {
            Vec2 delta = particles_[b].pos - particles_[a].pos;
            float dist = delta.length();
            if (dist < 1e-6f) continue;

            float error = (dist - rest) / dist;
            Vec2 correction = delta * (0.5f * error);

            if (!particles_[a].pinned) particles_[a].pos += correction;
            if (!particles_[b].pinned) particles_[b].pos -= correction;
        }
    }
}

void Chain::sync_pos_cache() {
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        pos_cache_[i] = particles_[i].pos;
    }
}
