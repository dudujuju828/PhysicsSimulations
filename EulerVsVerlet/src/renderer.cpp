#include "renderer.h"
#include <cstdio>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// ---- Shaders ----------------------------------------------------------------

static constexpr const char* kGeoVertSrc = R"glsl(
#version 460 core
layout(location = 0) in vec2 a_pos;
uniform vec2  u_resolution;
uniform float u_point_size;
void main() {
    vec2 ndc = (a_pos / u_resolution) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    gl_PointSize = u_point_size;
}
)glsl";

static constexpr const char* kGeoFragSrc = R"glsl(
#version 460 core
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)glsl";

// Text shader: input is y-down pixel coords (stb_easy_font native)
static constexpr const char* kTextVertSrc = R"glsl(
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

static constexpr const char* kTextFragSrc = R"glsl(
#version 460 core
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)glsl";

// ---- Helpers ----------------------------------------------------------------

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
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
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// ---- Renderer ---------------------------------------------------------------

void Renderer::init() {
    // --- Geometry pipeline ---
    {
        GLuint v = compile_shader(GL_VERTEX_SHADER,   kGeoVertSrc);
        GLuint f = compile_shader(GL_FRAGMENT_SHADER, kGeoFragSrc);
        geo_shader_ = link_program(v, f);

        geo_u_res_     = glGetUniformLocation(geo_shader_, "u_resolution");
        geo_u_color_   = glGetUniformLocation(geo_shader_, "u_color");
        geo_u_pt_size_ = glGetUniformLocation(geo_shader_, "u_point_size");

        glGenVertexArrays(1, &geo_vao_);
        glGenBuffers(1, &geo_vbo_);

        glBindVertexArray(geo_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, geo_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(kMaxGeoVerts * sizeof(Vec2)),
                     nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), nullptr);
        glBindVertexArray(0);
    }

    // --- Text pipeline ---
    {
        GLuint v = compile_shader(GL_VERTEX_SHADER,   kTextVertSrc);
        GLuint f = compile_shader(GL_FRAGMENT_SHADER, kTextFragSrc);
        text_shader_ = link_program(v, f);

        text_u_res_   = glGetUniformLocation(text_shader_, "u_resolution");
        text_u_color_ = glGetUniformLocation(text_shader_, "u_color");

        glGenVertexArrays(1, &text_vao_);
        glGenBuffers(1, &text_vbo_);
        glGenBuffers(1, &text_ebo_);

        // VBO: sized for max quads (4 verts * 16 bytes each)
        glBindVertexArray(text_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, text_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(kMaxTextQuads * 4 * 16),
                     nullptr, GL_DYNAMIC_DRAW);

        // Vertex attrib reads x,y as 2 floats with stride 16 (skips z + color)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);

        // EBO: pre-generate quad-to-triangle indices
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

void Renderer::draw_points(std::span<const Vec2> pts, float size,
                           float r, float g, float b,
                           int win_w, int win_h) {
    if (pts.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, geo_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size_bytes()), pts.data());
    glUseProgram(geo_shader_);
    glUniform2f(geo_u_res_, static_cast<float>(win_w), static_cast<float>(win_h));
    glUniform4f(geo_u_color_, r, g, b, 1.0f);
    glUniform1f(geo_u_pt_size_, size);
    glBindVertexArray(geo_vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

void Renderer::draw_line_strip(std::span<const Vec2> pts,
                               float r, float g, float b, float a,
                               int win_w, int win_h) {
    if (pts.size() < 2) return;
    glBindBuffer(GL_ARRAY_BUFFER, geo_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size_bytes()), pts.data());
    glUseProgram(geo_shader_);
    glUniform2f(geo_u_res_, static_cast<float>(win_w), static_cast<float>(win_h));
    glUniform4f(geo_u_color_, r, g, b, a);
    glUniform1f(geo_u_pt_size_, 1.0f);
    glBindVertexArray(geo_vao_);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

void Renderer::draw_lines(std::span<const Vec2> pts,
                          float r, float g, float b, float a,
                          int win_w, int win_h) {
    if (pts.size() < 2) return;
    glBindBuffer(GL_ARRAY_BUFFER, geo_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size_bytes()), pts.data());
    glUseProgram(geo_shader_);
    glUniform2f(geo_u_res_, static_cast<float>(win_w), static_cast<float>(win_h));
    glUniform4f(geo_u_color_, r, g, b, a);
    glUniform1f(geo_u_pt_size_, 1.0f);
    glBindVertexArray(geo_vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

void Renderer::draw_text(const char* text, float x, float y, float scale,
                         float r, float g, float b,
                         int win_w, int win_h) {
    // stb_easy_font outputs quads: 4 verts each, 16 bytes per vert (x,y,z,color)
    static char buffer[kMaxTextQuads * 4 * 16];
    int num_quads = stb_easy_font_print(0.0f, 0.0f, const_cast<char*>(text),
                                        nullptr, buffer, sizeof(buffer));
    if (num_quads <= 0) return;

    // Scale and translate vertex positions in-place
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
    if (geo_shader_)  glDeleteProgram(geo_shader_);
    if (geo_vbo_)     glDeleteBuffers(1, &geo_vbo_);
    if (geo_vao_)     glDeleteVertexArrays(1, &geo_vao_);
    if (text_shader_) glDeleteProgram(text_shader_);
    if (text_ebo_)    glDeleteBuffers(1, &text_ebo_);
    if (text_vbo_)    glDeleteBuffers(1, &text_vbo_);
    if (text_vao_)    glDeleteVertexArrays(1, &text_vao_);
    geo_shader_ = geo_vbo_ = geo_vao_ = 0;
    text_shader_ = text_ebo_ = text_vbo_ = text_vao_ = 0;
}
