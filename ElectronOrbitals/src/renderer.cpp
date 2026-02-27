#include "renderer.h"
#include <cstdio>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// ============================================================================
// Shader sources
// ============================================================================

static constexpr const char* kFullscreenVS = R"glsl(
#version 460 core
out vec2 v_uv;
void main() {
    // Fullscreen triangle: 3 vertices, no VBO
    vec2 pos = vec2(
        float((gl_VertexID & 1) << 2) - 1.0,
        float((gl_VertexID & 2) << 1) - 1.0
    );
    v_uv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Ray march fragment shader
// ---------------------------------------------------------------------------
static constexpr const char* kRaymarchFS = R"glsl(
#version 460 core
in vec2 v_uv;
out vec4 frag_color;

uniform mat4  u_inv_view_proj;
uniform vec3  u_camera_pos;
uniform int   u_n;
uniform int   u_l;
uniform int   u_m;
uniform float u_radial_norm;
uniform float u_angular_norm;
uniform float u_bounding_radius;
uniform float u_density_scale;
uniform int   u_max_steps;
uniform float u_time;
uniform float u_anim_speed;

// Reconstruct world ray from UV + inverse view-projection
void get_ray(out vec3 ro, out vec3 rd) {
    vec2 ndc = v_uv * 2.0 - 1.0;
    vec4 near_pt = u_inv_view_proj * vec4(ndc, -1.0, 1.0);
    vec4 far_pt  = u_inv_view_proj * vec4(ndc,  1.0, 1.0);
    near_pt /= near_pt.w;
    far_pt  /= far_pt.w;
    ro = u_camera_pos;
    rd = normalize(far_pt.xyz - near_pt.xyz);
}

// Ray-sphere intersection: returns (t_near, t_far), or (-1,-1) if miss
vec2 intersect_sphere(vec3 ro, vec3 rd, float radius) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0) return vec2(-1.0);
    float sq = sqrt(disc);
    return vec2(-b - sq, -b + sq);
}

// Associated Laguerre polynomial L^alpha_k(x) via recurrence
float laguerre(int k, float alpha, float x) {
    if (k == 0) return 1.0;
    float L0 = 1.0;
    float L1 = 1.0 + alpha - x;
    if (k == 1) return L1;
    for (int i = 1; i < k; ++i) {
        float L2 = ((2.0 * float(i) + 1.0 + alpha - x) * L1 - (float(i) + alpha) * L0) / float(i + 1);
        L0 = L1;
        L1 = L2;
    }
    return L1;
}

// Evaluate radial part R_nl(r)
float radial(float r, int n, int l, float norm) {
    float rho = 2.0 * r / float(n);
    float alpha = float(2 * l + 1);
    int k = n - l - 1;
    float L = laguerre(k, alpha, rho);
    return norm * exp(-rho * 0.5) * pow(rho, float(l)) * L;
}

// Evaluate real spherical harmonic Y_lm in Cartesian form
// pos must not be zero-length
float spherical_harmonic(vec3 pos, float r, int l, int m, float norm) {
    if (r < 1e-10) return 0.0;
    float x = pos.x, y = pos.y, z = pos.z;
    float r2 = r * r;
    float r3 = r2 * r;

    float angular = 0.0;

    // l=0
    if (l == 0) {
        angular = 1.0;
    }
    // l=1
    else if (l == 1) {
        if      (m == -1) angular = y / r;
        else if (m ==  0) angular = z / r;
        else              angular = x / r;
    }
    // l=2
    else if (l == 2) {
        if      (m == -2) angular = x * y / r2;
        else if (m == -1) angular = y * z / r2;
        else if (m ==  0) angular = (3.0 * z * z - r2) / r2;
        else if (m ==  1) angular = x * z / r2;
        else              angular = (x * x - y * y) / r2;
    }
    // l=3
    else if (l == 3) {
        if      (m == -3) angular = y * (3.0 * x * x - y * y) / r3;
        else if (m == -2) angular = x * y * z / r3;
        else if (m == -1) angular = y * (5.0 * z * z - r2) / r3;
        else if (m ==  0) angular = z * (5.0 * z * z - 3.0 * r2) / r3;
        else if (m ==  1) angular = x * (5.0 * z * z - r2) / r3;
        else if (m ==  2) angular = z * (x * x - y * y) / r3;
        else              angular = x * (x * x - 3.0 * y * y) / r3;
    }

    return norm * angular;
}

// Two-tone color palette
vec3 color_palette(float psi, float density) {
    vec3 deep_blue    = vec3(0.05, 0.15, 0.4);
    vec3 teal         = vec3(0.1, 0.6, 0.8);
    vec3 deep_magenta = vec3(0.4, 0.05, 0.3);
    vec3 coral        = vec3(0.9, 0.4, 0.3);

    float intensity = density;
    vec3 base;
    if (psi > 0.0) {
        base = mix(deep_blue, teal, min(intensity * 2.0, 1.0));
    } else {
        base = mix(deep_magenta, coral, min(intensity * 2.0, 1.0));
    }
    // Hot white core for high density
    base += vec3(1.0) * max(0.0, intensity - 0.5) * 3.0;
    return base;
}

void main() {
    vec3 ro, rd;
    get_ray(ro, rd);

    vec2 t_hit = intersect_sphere(ro, rd, u_bounding_radius);
    if (t_hit.x < 0.0) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float t_near = max(t_hit.x, 0.0);
    float t_far  = t_hit.y;
    float step_size = (t_far - t_near) / float(u_max_steps);

    vec3  accum_color = vec3(0.0);
    float accum_alpha = 0.0;
    float min_dist_sq = 1e10;

    for (int i = 0; i < u_max_steps; ++i) {
        if (accum_alpha > 0.99) break;

        float t = t_near + (float(i) + 0.5) * step_size;
        vec3  pos = ro + rd * t;
        float r = length(pos);

        // Track closest approach to origin for nucleus glow
        float d2 = dot(pos, pos);
        min_dist_sq = min(min_dist_sq, d2);

        if (r < 1e-6) continue;

        // Evaluate wave function
        float R = radial(r, u_n, u_l, u_radial_norm);
        float Y = spherical_harmonic(pos, r, u_l, u_m, u_angular_norm);
        float psi = R * Y;
        float density = psi * psi * u_density_scale;

        // Animated perturbation (subtle shimmer)
        density *= 1.0 + 0.06 * sin(u_time * u_anim_speed + r * 4.0
                         + dot(pos, vec3(1.7, 2.3, 3.1)));

        // Color from sign of psi
        vec3 sample_color = color_palette(psi, density);
        float sample_alpha = clamp(density * step_size * 0.5, 0.0, 1.0);

        // Front-to-back compositing
        accum_color += (1.0 - accum_alpha) * sample_color * sample_alpha;
        accum_alpha += (1.0 - accum_alpha) * sample_alpha;
    }

    // Nucleus glow
    vec3 nucleus = vec3(1.0, 0.9, 0.7) * exp(-min_dist_sq * 500.0);
    accum_color += (1.0 - accum_alpha) * nucleus;

    frag_color = vec4(accum_color, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Bright pass fragment shader
// ---------------------------------------------------------------------------
static constexpr const char* kBrightFS = R"glsl(
#version 460 core
in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_scene;
uniform float u_threshold;

void main() {
    vec3 color = texture(u_scene, v_uv).rgb;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (lum > u_threshold)
        frag_color = vec4(color, 1.0);
    else
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Gaussian blur fragment shader (separable 9-tap)
// ---------------------------------------------------------------------------
static constexpr const char* kBlurFS = R"glsl(
#version 460 core
in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_tex;
uniform vec2 u_direction;  // (1/w, 0) or (0, 1/h)
uniform vec2 u_texel_size;

void main() {
    vec2 step = u_direction * u_texel_size;
    // 9-tap Gaussian: weights for sigma ~= 2
    float w[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

    vec3 result = texture(u_tex, v_uv).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        result += texture(u_tex, v_uv + step * float(i)).rgb * w[i];
        result += texture(u_tex, v_uv - step * float(i)).rgb * w[i];
    }
    frag_color = vec4(result, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Composite fragment shader
// ---------------------------------------------------------------------------
static constexpr const char* kCompositeFS = R"glsl(
#version 460 core
in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_bloom_intensity;
uniform vec2  u_resolution;

// ACES tone mapping
vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3 scene = texture(u_scene, v_uv).rgb;
    vec3 bloom = texture(u_bloom, v_uv).rgb;

    vec3 hdr = scene + bloom * u_bloom_intensity;

    // Tone map
    vec3 color = aces(hdr);

    // Vignette
    vec2 centered = v_uv - 0.5;
    float vignette = 1.0 - 0.4 * dot(centered, centered);
    color *= vignette;

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Text shaders
// ---------------------------------------------------------------------------
static constexpr const char* kTextVS = R"glsl(
#version 460 core
layout(location = 0) in vec2 a_pos;
uniform vec2 u_resolution;
void main() {
    vec2 ndc = vec2(
        a_pos.x / u_resolution.x * 2.0 - 1.0,
        1.0 - a_pos.y / u_resolution.y * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)glsl";

static constexpr const char* kTextFS = R"glsl(
#version 460 core
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)glsl";

// ============================================================================
// Helpers
// ============================================================================

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
    }
    return shader;
}

static GLuint link_program(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

static GLuint build_program(const char* vs_src, const char* fs_src) {
    GLuint v = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    return link_program(v, f);
}

static void create_hdr_fbo(GLuint& fbo, GLuint& tex, int w, int h) {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (tex) { glDeleteTextures(1, &tex); tex = 0; }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "FBO incomplete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ============================================================================
// Renderer implementation
// ============================================================================

void Renderer::init() {
    // Empty VAO for fullscreen triangle
    glGenVertexArrays(1, &empty_vao_);

    // Build shader programs
    raymarch_prog_  = build_program(kFullscreenVS, kRaymarchFS);
    bright_prog_    = build_program(kFullscreenVS, kBrightFS);
    blur_prog_      = build_program(kFullscreenVS, kBlurFS);
    composite_prog_ = build_program(kFullscreenVS, kCompositeFS);

    // Cache ray march uniforms
    rm_inv_vp_        = glGetUniformLocation(raymarch_prog_, "u_inv_view_proj");
    rm_camera_pos_    = glGetUniformLocation(raymarch_prog_, "u_camera_pos");
    rm_n_             = glGetUniformLocation(raymarch_prog_, "u_n");
    rm_l_             = glGetUniformLocation(raymarch_prog_, "u_l");
    rm_m_             = glGetUniformLocation(raymarch_prog_, "u_m");
    rm_radial_norm_   = glGetUniformLocation(raymarch_prog_, "u_radial_norm");
    rm_angular_norm_  = glGetUniformLocation(raymarch_prog_, "u_angular_norm");
    rm_bounding_r_    = glGetUniformLocation(raymarch_prog_, "u_bounding_radius");
    rm_density_scale_ = glGetUniformLocation(raymarch_prog_, "u_density_scale");
    rm_max_steps_     = glGetUniformLocation(raymarch_prog_, "u_max_steps");
    rm_time_          = glGetUniformLocation(raymarch_prog_, "u_time");
    rm_anim_speed_    = glGetUniformLocation(raymarch_prog_, "u_anim_speed");

    // Bright pass uniforms
    bright_scene_     = glGetUniformLocation(bright_prog_, "u_scene");
    bright_threshold_ = glGetUniformLocation(bright_prog_, "u_threshold");

    // Blur uniforms
    blur_tex_       = glGetUniformLocation(blur_prog_, "u_tex");
    blur_direction_ = glGetUniformLocation(blur_prog_, "u_direction");
    blur_texel_     = glGetUniformLocation(blur_prog_, "u_texel_size");

    // Composite uniforms
    comp_scene_           = glGetUniformLocation(composite_prog_, "u_scene");
    comp_bloom_           = glGetUniformLocation(composite_prog_, "u_bloom");
    comp_bloom_intensity_ = glGetUniformLocation(composite_prog_, "u_bloom_intensity");
    comp_resolution_      = glGetUniformLocation(composite_prog_, "u_resolution");

    // Text pipeline
    {
        text_shader_ = build_program(kTextVS, kTextFS);
        text_u_res_   = glGetUniformLocation(text_shader_, "u_resolution");
        text_u_color_ = glGetUniformLocation(text_shader_, "u_color");

        glGenVertexArrays(1, &text_vao_);
        glGenBuffers(1, &text_vbo_);
        glGenBuffers(1, &text_ebo_);

        glBindVertexArray(text_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, text_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(kMaxTextQuads * 4 * 16),
                     nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);

        auto* indices = new GLuint[kMaxTextQuads * 6];
        for (std::size_t i = 0; i < kMaxTextQuads; ++i) {
            GLuint base = static_cast<GLuint>(i * 4);
            indices[i * 6 + 0] = base + 0;
            indices[i * 6 + 1] = base + 1;
            indices[i * 6 + 2] = base + 2;
            indices[i * 6 + 3] = base + 0;
            indices[i * 6 + 4] = base + 2;
            indices[i * 6 + 5] = base + 3;
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(kMaxTextQuads * 6 * sizeof(GLuint)),
                     indices, GL_STATIC_DRAW);
        delete[] indices;

        glBindVertexArray(0);
    }
}

void Renderer::resize_fbos(int width, int height) {
    if (width == fb_width_ && height == fb_height_) return;
    fb_width_  = width;
    fb_height_ = height;

    // Full-res HDR FBO
    create_hdr_fbo(hdr_fbo_, hdr_tex_, width, height);

    // Half-res bloom FBOs
    int hw = width / 2, hh = height / 2;
    if (hw < 1) hw = 1;
    if (hh < 1) hh = 1;
    create_hdr_fbo(bloom_fbo_a_, bloom_tex_a_, hw, hh);
    create_hdr_fbo(bloom_fbo_b_, bloom_tex_b_, hw, hh);
}

void Renderer::draw_fullscreen_triangle() {
    glBindVertexArray(empty_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Renderer::draw_raymarch(const RaymarchUniforms& u) {
    glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo_);
    glViewport(0, 0, fb_width_, fb_height_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(raymarch_prog_);
    glUniformMatrix4fv(rm_inv_vp_, 1, GL_FALSE, u.inv_view_proj.data());
    glUniform3f(rm_camera_pos_, u.camera_pos.x, u.camera_pos.y, u.camera_pos.z);
    glUniform1i(rm_n_, u.n);
    glUniform1i(rm_l_, u.l);
    glUniform1i(rm_m_, u.m);
    glUniform1f(rm_radial_norm_, u.radial_norm);
    glUniform1f(rm_angular_norm_, u.angular_norm);
    glUniform1f(rm_bounding_r_, u.bounding_radius);
    glUniform1f(rm_density_scale_, u.density_scale);
    glUniform1i(rm_max_steps_, u.max_steps);
    glUniform1f(rm_time_, u.time);
    glUniform1f(rm_anim_speed_, u.anim_speed);

    draw_fullscreen_triangle();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::draw_bloom() {
    int hw = fb_width_ / 2, hh = fb_height_ / 2;
    if (hw < 1) hw = 1;
    if (hh < 1) hh = 1;

    // Step 1: Bright pass — extract bright pixels from HDR into bloom FBO A
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_a_);
    glViewport(0, 0, hw, hh);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(bright_prog_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_tex_);
    glUniform1i(bright_scene_, 0);
    glUniform1f(bright_threshold_, 0.8f);
    draw_fullscreen_triangle();

    // Step 2: Ping-pong Gaussian blur — 3 iterations (H+V each)
    float texel_w = 1.0f / static_cast<float>(hw);
    float texel_h = 1.0f / static_cast<float>(hh);

    glUseProgram(blur_prog_);
    glUniform1i(blur_tex_, 0);

    for (int iter = 0; iter < 3; ++iter) {
        // Horizontal: A -> B
        glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_b_);
        glViewport(0, 0, hw, hh);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloom_tex_a_);
        glUniform2f(blur_direction_, 1.0f, 0.0f);
        glUniform2f(blur_texel_, texel_w, texel_h);
        draw_fullscreen_triangle();

        // Vertical: B -> A
        glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_a_);
        glViewport(0, 0, hw, hh);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloom_tex_b_);
        glUniform2f(blur_direction_, 0.0f, 1.0f);
        glUniform2f(blur_texel_, texel_w, texel_h);
        draw_fullscreen_triangle();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::draw_composite(float bloom_intensity) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fb_width_, fb_height_);

    glUseProgram(composite_prog_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_tex_);
    glUniform1i(comp_scene_, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloom_tex_a_);
    glUniform1i(comp_bloom_, 1);

    glUniform1f(comp_bloom_intensity_, bloom_intensity);
    glUniform2f(comp_resolution_, static_cast<float>(fb_width_),
                static_cast<float>(fb_height_));

    draw_fullscreen_triangle();
}

void Renderer::draw_text(const char* text, float x, float y, float scale,
                         float r, float g, float b,
                         int win_w, int win_h) {
    static char buffer[kMaxTextQuads * 4 * 16];
    int num_quads = stb_easy_font_print(0.0f, 0.0f, const_cast<char*>(text),
                                        nullptr, buffer, sizeof(buffer));
    if (num_quads <= 0) return;

    auto* verts = reinterpret_cast<float*>(buffer);
    int num_verts = num_quads * 4;
    for (int i = 0; i < num_verts; ++i) {
        verts[i * 4 + 0] = x + verts[i * 4 + 0] * scale;
        verts[i * 4 + 1] = y + verts[i * 4 + 1] * scale;
    }

    glBindBuffer(GL_ARRAY_BUFFER, text_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(num_verts * 16), buffer);

    glUseProgram(text_shader_);
    glUniform2f(text_u_res_, static_cast<float>(win_w), static_cast<float>(win_h));
    glUniform4f(text_u_color_, r, g, b, 1.0f);
    glBindVertexArray(text_vao_);
    glDrawElements(GL_TRIANGLES, num_quads * 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer::cleanup() {
    if (raymarch_prog_)  glDeleteProgram(raymarch_prog_);
    if (bright_prog_)    glDeleteProgram(bright_prog_);
    if (blur_prog_)      glDeleteProgram(blur_prog_);
    if (composite_prog_) glDeleteProgram(composite_prog_);
    if (text_shader_)    glDeleteProgram(text_shader_);

    if (empty_vao_)    glDeleteVertexArrays(1, &empty_vao_);
    if (text_vao_)     glDeleteVertexArrays(1, &text_vao_);
    if (text_vbo_)     glDeleteBuffers(1, &text_vbo_);
    if (text_ebo_)     glDeleteBuffers(1, &text_ebo_);

    if (hdr_fbo_)      glDeleteFramebuffers(1, &hdr_fbo_);
    if (hdr_tex_)      glDeleteTextures(1, &hdr_tex_);
    if (bloom_fbo_a_)  glDeleteFramebuffers(1, &bloom_fbo_a_);
    if (bloom_tex_a_)  glDeleteTextures(1, &bloom_tex_a_);
    if (bloom_fbo_b_)  glDeleteFramebuffers(1, &bloom_fbo_b_);
    if (bloom_tex_b_)  glDeleteTextures(1, &bloom_tex_b_);
}
