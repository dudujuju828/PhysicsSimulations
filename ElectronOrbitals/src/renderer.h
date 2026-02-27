#pragma once
#include "mat4.h"
#include <glad/gl.h>
#include <cstddef>

struct RaymarchUniforms {
    mat4  inv_view_proj;
    vec3  camera_pos;
    int   n, l, m;
    float radial_norm;
    float angular_norm;
    float bounding_radius;
    float density_scale;
    int   max_steps;
    float time;
    float anim_speed;
};

class Renderer {
public:
    void init();
    void cleanup();

    void resize_fbos(int width, int height);
    void draw_raymarch(const RaymarchUniforms& u);
    void draw_bloom();
    void draw_composite(float bloom_intensity);

    // 2D text overlay
    void draw_text(const char* text, float x, float y, float scale,
                   float r, float g, float b,
                   int win_w, int win_h);

private:
    int fb_width_  = 0;
    int fb_height_ = 0;

    // Empty VAO for fullscreen triangle
    GLuint empty_vao_ = 0;

    // Ray march program
    GLuint raymarch_prog_    = 0;
    GLint  rm_inv_vp_        = -1;
    GLint  rm_camera_pos_    = -1;
    GLint  rm_n_             = -1;
    GLint  rm_l_             = -1;
    GLint  rm_m_             = -1;
    GLint  rm_radial_norm_   = -1;
    GLint  rm_angular_norm_  = -1;
    GLint  rm_bounding_r_    = -1;
    GLint  rm_density_scale_ = -1;
    GLint  rm_max_steps_     = -1;
    GLint  rm_time_          = -1;
    GLint  rm_anim_speed_    = -1;

    // HDR FBO (full resolution)
    GLuint hdr_fbo_ = 0;
    GLuint hdr_tex_ = 0;

    // Bright pass program
    GLuint bright_prog_      = 0;
    GLint  bright_scene_     = -1;
    GLint  bright_threshold_ = -1;

    // Blur program
    GLuint blur_prog_      = 0;
    GLint  blur_tex_       = -1;
    GLint  blur_direction_ = -1;
    GLint  blur_texel_     = -1;

    // Bloom FBOs (half resolution, ping-pong)
    GLuint bloom_fbo_a_ = 0;
    GLuint bloom_tex_a_ = 0;
    GLuint bloom_fbo_b_ = 0;
    GLuint bloom_tex_b_ = 0;

    // Composite program
    GLuint composite_prog_       = 0;
    GLint  comp_scene_           = -1;
    GLint  comp_bloom_           = -1;
    GLint  comp_bloom_intensity_ = -1;
    GLint  comp_resolution_      = -1;

    // Text pipeline
    GLuint text_shader_  = 0;
    GLuint text_vao_     = 0;
    GLuint text_vbo_     = 0;
    GLuint text_ebo_     = 0;
    GLint  text_u_res_   = -1;
    GLint  text_u_color_ = -1;

    static constexpr std::size_t kMaxTextQuads = 4096;

    void draw_fullscreen_triangle();
};
