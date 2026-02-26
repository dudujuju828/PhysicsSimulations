#pragma once
#include <cmath>
#include <cstddef>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }

    constexpr Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }

    constexpr float dot(Vec2 o) const { return x * o.x + y * o.y; }
    constexpr float length_sq() const { return dot(*this); }

    float length() const { return std::sqrt(length_sq()); }
};

struct Trail {
    static constexpr std::size_t kCapacity = 256;
    Vec2 points[kCapacity];
    std::size_t head  = 0;
    std::size_t count = 0;

    void push(Vec2 p) {
        points[head] = p;
        head = (head + 1) % kCapacity;
        if (count < kCapacity) ++count;
    }

    void clear() {
        head  = 0;
        count = 0;
    }

    std::size_t extract(Vec2* out) const {
        for (std::size_t i = 0; i < count; ++i) {
            std::size_t idx = (head + kCapacity - count + i) % kCapacity;
            out[i] = points[idx];
        }
        return count;
    }
};
