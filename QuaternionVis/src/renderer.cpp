#include "renderer.h"
#include <cstdio>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// ---- Shaders ----------------------------------------------------------------

static constexpr const char* kGeo3dVertSrc = R"glsl(
#version 460 core
layout(location = 0) in vec3 a_pos;
uniform mat4  u_mvp;
uniform float u_point_size;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    gl_PointSize = u_point_size;
}
)glsl";

static constexpr const char* kGeo3dFragSrc = R"glsl(
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
    // --- 3D pipeline ---
    {
        GLuint v = compile_shader(GL_VERTEX_SHADER,   kGeo3dVertSrc);
        GLuint f = compile_shader(GL_FRAGMENT_SHADER, kGeo3dFragSrc);
        geo3d_shader_ = link_program(v, f);

        geo3d_u_mvp_     = glGetUniformLocation(geo3d_shader_, "u_mvp");
        geo3d_u_color_   = glGetUniformLocation(geo3d_shader_, "u_color");
        geo3d_u_pt_size_ = glGetUniformLocation(geo3d_shader_, "u_point_size");

        glGenVertexArrays(1, &geo3d_vao_);
        glGenBuffers(1, &geo3d_vbo_);

        glBindVertexArray(geo3d_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, geo3d_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(kMaxGeo3dVerts * sizeof(vec3)),
                     nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr);
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

// ---- 3D drawing -------------------------------------------------------------

void Renderer::draw_lines_3d(std::span<const vec3> pts, const mat4& mvp, Color4 color) {
    if (pts.size() < 2) return;
    glBindBuffer(GL_ARRAY_BUFFER, geo3d_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size_bytes()), pts.data());
    glUseProgram(geo3d_shader_);
    glUniformMatrix4fv(geo3d_u_mvp_, 1, GL_FALSE, mvp.data());
    glUniform4f(geo3d_u_color_, color.r, color.g, color.b, color.a);
    glUniform1f(geo3d_u_pt_size_, 1.0f);
    glBindVertexArray(geo3d_vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

void Renderer::draw_line_strip_3d(std::span<const vec3> pts, const mat4& mvp, Color4 color) {
    if (pts.size() < 2) return;
    glBindBuffer(GL_ARRAY_BUFFER, geo3d_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size_bytes()), pts.data());
    glUseProgram(geo3d_shader_);
    glUniformMatrix4fv(geo3d_u_mvp_, 1, GL_FALSE, mvp.data());
    glUniform4f(geo3d_u_color_, color.r, color.g, color.b, color.a);
    glUniform1f(geo3d_u_pt_size_, 1.0f);
    glBindVertexArray(geo3d_vao_);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

void Renderer::draw_points_3d(std::span<const vec3> pts, const mat4& mvp,
                               Color4 color, float size) {
    if (pts.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, geo3d_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size_bytes()), pts.data());
    glUseProgram(geo3d_shader_);
    glUniformMatrix4fv(geo3d_u_mvp_, 1, GL_FALSE, mvp.data());
    glUniform4f(geo3d_u_color_, color.r, color.g, color.b, color.a);
    glUniform1f(geo3d_u_pt_size_, size);
    glBindVertexArray(geo3d_vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

// ---- Text -------------------------------------------------------------------

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

// ---- Cleanup ----------------------------------------------------------------

void Renderer::cleanup() {
    if (geo3d_shader_) glDeleteProgram(geo3d_shader_);
    if (geo3d_vbo_)    glDeleteBuffers(1, &geo3d_vbo_);
    if (geo3d_vao_)    glDeleteVertexArrays(1, &geo3d_vao_);
    if (text_shader_)  glDeleteProgram(text_shader_);
    if (text_ebo_)     glDeleteBuffers(1, &text_ebo_);
    if (text_vbo_)     glDeleteBuffers(1, &text_vbo_);
    if (text_vao_)     glDeleteVertexArrays(1, &text_vao_);
}
