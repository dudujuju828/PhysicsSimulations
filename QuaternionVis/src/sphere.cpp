#include "sphere.h"
#include <cmath>

static constexpr float kPi = 3.14159265358979323846f;

static void add_circle(std::vector<vec3>& out,
                       float lat_rad, int segments) {
    float cos_lat = std::cos(lat_rad);
    float sin_lat = std::sin(lat_rad);
    float step = 2.0f * kPi / static_cast<float>(segments);

    for (int i = 0; i < segments; ++i) {
        float lon0 = static_cast<float>(i) * step;
        float lon1 = static_cast<float>(i + 1) * step;
        out.push_back({cos_lat * std::cos(lon0), sin_lat, cos_lat * std::sin(lon0)});
        out.push_back({cos_lat * std::cos(lon1), sin_lat, cos_lat * std::sin(lon1)});
    }
}

static void add_meridian(std::vector<vec3>& out,
                         float lon_rad, int segments) {
    float step = kPi / static_cast<float>(segments);

    for (int i = 0; i < segments; ++i) {
        float lat0 = -kPi * 0.5f + static_cast<float>(i) * step;
        float lat1 = -kPi * 0.5f + static_cast<float>(i + 1) * step;
        out.push_back({std::cos(lat0) * std::cos(lon_rad), std::sin(lat0), std::cos(lat0) * std::sin(lon_rad)});
        out.push_back({std::cos(lat1) * std::cos(lon_rad), std::sin(lat1), std::cos(lat1) * std::sin(lon_rad)});
    }
}

void generate_sphere(SphereData& out) {
    out.lines.clear();
    out.equator_lines.clear();
    out.axis_lines.clear();

    constexpr int kSegments = 36;

    // Latitudes: -60, -30, 0 (equator), +30, +60
    float latitudes[] = {-60.0f, -30.0f, 30.0f, 60.0f};
    for (float lat_deg : latitudes) {
        add_circle(out.lines, lat_deg * kPi / 180.0f, kSegments);
    }

    // Equator separately
    add_circle(out.equator_lines, 0.0f, kSegments);

    // 12 longitudes (every 30 degrees)
    for (int i = 0; i < 12; ++i) {
        float lon_deg = static_cast<float>(i) * 30.0f;
        add_meridian(out.lines, lon_deg * kPi / 180.0f, kSegments);
    }

    // World axes: -1.3 to +1.3 on each axis
    constexpr float kAxisLen = 1.3f;
    out.axis_lines.push_back({-kAxisLen, 0.0f, 0.0f});
    out.axis_lines.push_back({ kAxisLen, 0.0f, 0.0f});
    out.axis_lines.push_back({0.0f, -kAxisLen, 0.0f});
    out.axis_lines.push_back({0.0f,  kAxisLen, 0.0f});
    out.axis_lines.push_back({0.0f, 0.0f, -kAxisLen});
    out.axis_lines.push_back({0.0f, 0.0f,  kAxisLen});
}
