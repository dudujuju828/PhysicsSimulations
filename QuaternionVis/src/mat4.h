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

    // Convert unit quaternion (w, x, y, z) to rotation matrix.
    // Forward-declared; implemented after quat is defined, but we can
    // do it inline here using raw floats.
    static mat4 from_quat(float qw, float qx, float qy, float qz) {
        mat4 r = identity();
        float xx = qx * qx, yy = qy * qy, zz = qz * qz;
        float xy = qx * qy, xz = qx * qz, yz = qy * qz;
        float wx = qw * qx, wy = qw * qy, wz = qw * qz;

        r.m[0]  = 1.0f - 2.0f * (yy + zz);
        r.m[1]  = 2.0f * (xy + wz);
        r.m[2]  = 2.0f * (xz - wy);

        r.m[4]  = 2.0f * (xy - wz);
        r.m[5]  = 1.0f - 2.0f * (xx + zz);
        r.m[6]  = 2.0f * (yz + wx);

        r.m[8]  = 2.0f * (xz + wy);
        r.m[9]  = 2.0f * (yz - wx);
        r.m[10] = 1.0f - 2.0f * (xx + yy);

        return r;
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
