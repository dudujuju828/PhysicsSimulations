#pragma once
#include <cmath>

struct vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr vec3 operator+(vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr vec3 operator-(vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr vec3 operator-() const { return {-x, -y, -z}; }

    constexpr vec3& operator+=(vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr vec3& operator-=(vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

constexpr vec3 operator*(float s, vec3 v) { return {s * v.x, s * v.y, s * v.z}; }

constexpr float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

constexpr vec3 cross(vec3 a, vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float length(vec3 v) { return std::sqrt(dot(v, v)); }

inline vec3 normalize(vec3 v) {
    float len = length(v);
    if (len < 1e-8f) return {0.0f, 0.0f, 0.0f};
    return v / len;
}
