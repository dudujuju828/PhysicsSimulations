#pragma once
#include "vec3.h"
#include <cmath>
#include <cstring>

struct vec4 {
    float x, y, z, w;
};

// 4x4 column-major matrix (OpenGL convention).
// m[col*4 + row]
struct mat4 {
    float m[16]{};

    static mat4 identity() {
        mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static mat4 perspective(float fov_y_rad, float aspect, float near, float far) {
        float t = std::tan(fov_y_rad * 0.5f);
        mat4 r{};
        r.m[0]  = 1.0f / (aspect * t);
        r.m[5]  = 1.0f / t;
        r.m[10] = -(far + near) / (far - near);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * far * near) / (far - near);
        return r;
    }

    static mat4 look_at(vec3 eye, vec3 center, vec3 up) {
        vec3 f = normalize(center - eye);
        vec3 s = normalize(cross(f, up));
        vec3 u = cross(s, f);

        mat4 r = identity();
        r.m[0]  = s.x;  r.m[4]  = s.y;  r.m[8]  = s.z;
        r.m[1]  = u.x;  r.m[5]  = u.y;  r.m[9]  = u.z;
        r.m[2]  = -f.x; r.m[6]  = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] =  dot(f, eye);
        return r;
    }

    // General 4x4 matrix inverse via cofactor expansion.
    mat4 inverse() const {
        float inv[16];
        const float* a = m;

        inv[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15]
                  + a[9]*a[7]*a[14]  + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
        inv[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15]
                  - a[8]*a[7]*a[14]  - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
        inv[8]  =  a[4]*a[9]*a[15]  - a[4]*a[11]*a[13] - a[8]*a[5]*a[15]
                  + a[8]*a[7]*a[13]  + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
        inv[12] = -a[4]*a[9]*a[14]  + a[4]*a[10]*a[13] + a[8]*a[5]*a[14]
                  - a[8]*a[6]*a[13]  - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
        inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15]
                  - a[9]*a[3]*a[14]  - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
        inv[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15]
                  + a[8]*a[3]*a[14]  + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
        inv[9]  = -a[0]*a[9]*a[15]  + a[0]*a[11]*a[13] + a[8]*a[1]*a[15]
                  - a[8]*a[3]*a[13]  - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
        inv[13] =  a[0]*a[9]*a[14]  - a[0]*a[10]*a[13] - a[8]*a[1]*a[14]
                  + a[8]*a[2]*a[13]  + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
        inv[2]  =  a[1]*a[6]*a[15]  - a[1]*a[7]*a[14]  - a[5]*a[2]*a[15]
                  + a[5]*a[3]*a[14]  + a[13]*a[2]*a[7]  - a[13]*a[3]*a[6];
        inv[6]  = -a[0]*a[6]*a[15]  + a[0]*a[7]*a[14]  + a[4]*a[2]*a[15]
                  - a[4]*a[3]*a[14]  - a[12]*a[2]*a[7]  + a[12]*a[3]*a[6];
        inv[10] =  a[0]*a[5]*a[15]  - a[0]*a[7]*a[13]  - a[4]*a[1]*a[15]
                  + a[4]*a[3]*a[13]  + a[12]*a[1]*a[7]  - a[12]*a[3]*a[5];
        inv[14] = -a[0]*a[5]*a[14]  + a[0]*a[6]*a[13]  + a[4]*a[1]*a[14]
                  - a[4]*a[2]*a[13]  - a[12]*a[1]*a[6]  + a[12]*a[2]*a[5];
        inv[3]  = -a[1]*a[6]*a[11]  + a[1]*a[7]*a[10]  + a[5]*a[2]*a[11]
                  - a[5]*a[3]*a[10]  - a[9]*a[2]*a[7]   + a[9]*a[3]*a[6];
        inv[7]  =  a[0]*a[6]*a[11]  - a[0]*a[7]*a[10]  - a[4]*a[2]*a[11]
                  + a[4]*a[3]*a[10]  + a[8]*a[2]*a[7]   - a[8]*a[3]*a[6];
        inv[11] = -a[0]*a[5]*a[11]  + a[0]*a[7]*a[9]   + a[4]*a[1]*a[11]
                  - a[4]*a[3]*a[9]   - a[8]*a[1]*a[7]   + a[8]*a[3]*a[5];
        inv[15] =  a[0]*a[5]*a[10]  - a[0]*a[6]*a[9]   - a[4]*a[1]*a[10]
                  + a[4]*a[2]*a[9]   + a[8]*a[1]*a[6]   - a[8]*a[2]*a[5];

        float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
        if (std::fabs(det) < 1e-12f) return identity();
        float inv_det = 1.0f / det;

        mat4 result;
        for (int i = 0; i < 16; ++i) result.m[i] = inv[i] * inv_det;
        return result;
    }

    const float* data() const { return m; }

    friend mat4 operator*(const mat4& a, const mat4& b) {
        mat4 r{};
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = sum;
            }
        return r;
    }

    friend vec4 operator*(const mat4& a, vec4 v) {
        return {
            a.m[0]*v.x + a.m[4]*v.y + a.m[8]*v.z  + a.m[12]*v.w,
            a.m[1]*v.x + a.m[5]*v.y + a.m[9]*v.z  + a.m[13]*v.w,
            a.m[2]*v.x + a.m[6]*v.y + a.m[10]*v.z + a.m[14]*v.w,
            a.m[3]*v.x + a.m[7]*v.y + a.m[11]*v.z + a.m[15]*v.w
        };
    }
};
