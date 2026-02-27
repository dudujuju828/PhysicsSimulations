#pragma once
#include "vec3.h"
#include "mat4.h"
#include <glad/gl.h>
#include <cstddef>
#include <span>

struct Color4 {
    float r, g, b, a;
};

class Renderer {
public:
    void init();
    void cleanup();

    // 3D drawing (MVP-based)
    void draw_lines_3d(std::span<const vec3> pts, const mat4& mvp, Color4 color);
    void draw_line_strip_3d(std::span<const vec3> pts, const mat4& mvp, Color4 color);
    void draw_points_3d(std::span<const vec3> pts, const mat4& mvp, Color4 color, float size);

    // 2D text (screen-space, y-down pixel coords)
    void draw_text(const char* text, float x, float y, float scale,
                   float r, float g, float b,
                   int win_w, int win_h);

private:
    // 3D pipeline
    GLuint geo3d_shader_    = 0;
    GLuint geo3d_vao_       = 0;
    GLuint geo3d_vbo_       = 0;
    GLint  geo3d_u_mvp_     = -1;
    GLint  geo3d_u_color_   = -1;
    GLint  geo3d_u_pt_size_ = -1;

    // Text pipeline
    GLuint text_shader_     = 0;
    GLuint text_vao_        = 0;
    GLuint text_vbo_        = 0;
    GLuint text_ebo_        = 0;
    GLint  text_u_res_      = -1;
    GLint  text_u_color_    = -1;

    static constexpr std::size_t kMaxGeo3dVerts = 4096;
    static constexpr std::size_t kMaxTextQuads  = 4096;
};
