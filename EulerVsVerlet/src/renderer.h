#pragma once
#include "vec2.h"
#include <glad/gl.h>
#include <cstddef>
#include <span>

class Renderer {
public:
    void init();
    void cleanup();

    void draw_points(std::span<const Vec2> pts, float size,
                     float r, float g, float b,
                     int win_w, int win_h);

    void draw_line_strip(std::span<const Vec2> pts,
                         float r, float g, float b, float a,
                         int win_w, int win_h);

    void draw_lines(std::span<const Vec2> pts,
                    float r, float g, float b, float a,
                    int win_w, int win_h);

    // x, y in screen-down pixel coords (origin top-left).
    void draw_text(const char* text, float x, float y, float scale,
                   float r, float g, float b,
                   int win_w, int win_h);

private:
    // Geometry rendering (y-up pixel coords)
    GLuint geo_shader_      = 0;
    GLuint geo_vao_         = 0;
    GLuint geo_vbo_         = 0;
    GLint  geo_u_res_       = -1;
    GLint  geo_u_color_     = -1;
    GLint  geo_u_pt_size_   = -1;

    // Text rendering (y-down pixel coords)
    GLuint text_shader_     = 0;
    GLuint text_vao_        = 0;
    GLuint text_vbo_        = 0;
    GLuint text_ebo_        = 0;
    GLint  text_u_res_      = -1;
    GLint  text_u_color_    = -1;

    static constexpr std::size_t kMaxGeoVerts  = 1024;
    static constexpr std::size_t kMaxTextQuads  = 4096;
};
