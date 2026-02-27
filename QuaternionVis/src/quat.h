#pragma once
#include "vec3.h"
#include "mat4.h"
#include <cmath>

struct quat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    static quat identity() { return {1.0f, 0.0f, 0.0f, 0.0f}; }

    static quat from_axis_angle(vec3 axis, float angle_rad) {
        vec3 a = normalize(axis);
        float half = angle_rad * 0.5f;
        float s = std::sin(half);
        return {std::cos(half), a.x * s, a.y * s, a.z * s};
    }

    // Hamilton product
    friend quat operator*(quat a, quat b) {
        return {
            a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
            a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
            a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
            a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
        };
    }

    vec3 rotate_vec(vec3 v) const {
        // v' = q * (0,v) * q*
        // Expanded for efficiency:
        vec3 qv = {x, y, z};
        vec3 t = 2.0f * cross(qv, v);
        return v + w * t + cross(qv, t);
    }

    mat4 to_mat4() const {
        return mat4::from_quat(w, x, y, z);
    }
};

inline quat conjugate(quat q) { return {q.w, -q.x, -q.y, -q.z}; }

inline float dot(quat a, quat b) {
    return a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;
}

inline float length(quat q) { return std::sqrt(dot(q, q)); }

inline quat normalize(quat q) {
    float len = length(q);
    if (len < 1e-8f) return quat::identity();
    float inv = 1.0f / len;
    return {q.w * inv, q.x * inv, q.y * inv, q.z * inv};
}

// Component-wise linear interpolation, then normalize (nlerp).
inline quat lerp(quat a, quat b, float t) {
    // Short path
    if (dot(a, b) < 0.0f) {
        b = {-b.w, -b.x, -b.y, -b.z};
    }
    return normalize({
        a.w + (b.w - a.w) * t,
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    });
}

// Spherical linear interpolation.
inline quat slerp(quat a, quat b, float t) {
    float d = dot(a, b);

    // Short path
    if (d < 0.0f) {
        b = {-b.w, -b.x, -b.y, -b.z};
        d = -d;
    }

    // Fall back to lerp for near-parallel quaternions
    if (d > 0.9995f) {
        return lerp(a, b, t);
    }

    float theta = std::acos(d);
    float sin_theta = std::sin(theta);
    float wa = std::sin((1.0f - t) * theta) / sin_theta;
    float wb = std::sin(t * theta) / sin_theta;

    return {
        wa * a.w + wb * b.w,
        wa * a.x + wb * b.x,
        wa * a.y + wb * b.y,
        wa * a.z + wb * b.z
    };
}
