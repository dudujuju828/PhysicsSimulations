#pragma once
#include "vec2.h"
#include <glad/gl.h>
#include <cstddef>
#include <span>

class ChainRenderer {
public:
    void init(std::size_t max_particles);
    void draw(std::span<const Vec2> positions, int win_width, int win_height);
    void cleanup();

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shader_ = 0;
    GLint u_resolution_ = -1;
    GLint u_color_ = -1;
    std::size_t max_particles_ = 0;
};
