#pragma once
#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }

    constexpr Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    constexpr float dot(Vec2 o) const { return x * o.x + y * o.y; }
    constexpr float cross(Vec2 o) const { return x * o.y - y * o.x; }
    constexpr float length_sq() const { return dot(*this); }
    float length() const { return std::sqrt(length_sq()); }

    Vec2 normalized() const {
        float l = length();
        return l > 0.0f ? Vec2{x / l, y / l} : Vec2{0.0f, 0.0f};
    }

    constexpr Vec2 perp() const { return {-y, x}; }

    static constexpr Vec2 min(Vec2 a, Vec2 b) {
        return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y};
    }
    static constexpr Vec2 max(Vec2 a, Vec2 b) {
        return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y};
    }
};

inline constexpr Vec2 operator*(float s, Vec2 v) { return {s * v.x, s * v.y}; }
