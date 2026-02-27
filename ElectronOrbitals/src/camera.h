#pragma once
#include "vec3.h"
#include "mat4.h"
#include <cmath>
#include <algorithm>

constexpr float kCamPi     = 3.14159265358979323846f;
constexpr float kCamDeg2Rad = kCamPi / 180.0f;

struct Camera {
    float azimuth   = 30.0f;   // degrees
    float elevation = 20.0f;   // degrees
    float distance  = 20.0f;   // current distance
    vec3  target    = {0, 0, 0}; // look-at target (pan shifts this)

    // Smooth distance interpolation
    float distance_from = 20.0f;
    float distance_to   = 20.0f;
    float interp_t      = 1.0f; // 1 = done
    float interp_speed  = 1.0f / 0.3f; // reach target in 0.3s

    // Bounds (updated per orbital)
    float min_distance = 4.0f;
    float max_distance = 64.0f;

    void set_distance_target(float new_dist, float bounding_radius) {
        distance_from = distance;
        distance_to   = new_dist;
        interp_t      = 0.0f;
        min_distance   = bounding_radius * 0.5f;
        max_distance   = bounding_radius * 8.0f;
    }

    void update(float dt) {
        if (interp_t < 1.0f) {
            interp_t += dt * interp_speed;
            if (interp_t > 1.0f) interp_t = 1.0f;
            // Smooth-step
            float t = interp_t * interp_t * (3.0f - 2.0f * interp_t);
            distance = distance_from + (distance_to - distance_from) * t;
        }
    }

    void orbit(float dx, float dy) {
        azimuth   += dx * 0.3f;
        elevation -= dy * 0.3f;
        elevation  = std::clamp(elevation, -89.0f, 89.0f);
    }

    void pan(float dx, float dy) {
        float az = azimuth * kCamDeg2Rad;
        float el = elevation * kCamDeg2Rad;

        // Camera right vector (in world space)
        vec3 right = { std::cos(az), 0.0f, -std::sin(az) };
        // Camera up vector (approximate, ignoring elevation for simplicity)
        vec3 up = { -std::sin(el) * std::sin(az), std::cos(el), -std::sin(el) * std::cos(az) };

        float scale = distance * 0.002f;
        target = target - right * (dx * scale) + up * (dy * scale);
    }

    void zoom(float scroll_y) {
        distance *= (1.0f - scroll_y * 0.1f);
        distance  = std::clamp(distance, min_distance, max_distance);
        // Also update the interpolation target so we don't snap back
        distance_to = distance;
        interp_t    = 1.0f;
    }

    vec3 eye_position() const {
        float az = azimuth * kCamDeg2Rad;
        float el = elevation * kCamDeg2Rad;
        return target + vec3{
            distance * std::cos(el) * std::sin(az),
            distance * std::sin(el),
            distance * std::cos(el) * std::cos(az)
        };
    }

    mat4 view_matrix() const {
        return mat4::look_at(eye_position(), target, {0, 1, 0});
    }

    mat4 projection_matrix(float aspect) const {
        return mat4::perspective(45.0f * kCamDeg2Rad, aspect, 0.1f, 1000.0f);
    }
};
