#pragma once
#include "vec3.h"
#include <vector>
#include <cstddef>

struct SphereData {
    std::vector<vec3> lines;          // GL_LINES pairs (non-equator)
    std::vector<vec3> equator_lines;  // GL_LINES pairs (equator only)
    std::vector<vec3> axis_lines;     // GL_LINES pairs (world axes)
};

void generate_sphere(SphereData& out);
