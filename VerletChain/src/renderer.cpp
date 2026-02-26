#include "renderer.h"
#include <cstdio>

static constexpr const char* kVertSrc = R"glsl(
#version 460 core
layout(location = 0) in vec2 a_pos;
uniform vec2 u_resolution;
void main() {
    vec2 ndc = (a_pos / u_resolution) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    gl_PointSize = 8.0;
}
)glsl";

static constexpr const char* kFragSrc = R"glsl(
#version 460 core
uniform vec3 u_color;
out vec4 frag_color;
void main() {
    frag_color = vec4(u_color, 1.0);
}
)glsl";

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

void ChainRenderer::init(std::size_t max_particles) {
    max_particles_ = max_particles;

    GLuint vert = compile_shader(GL_VERTEX_SHADER, kVertSrc);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kFragSrc);
    shader_ = link_program(vert, frag);

    u_resolution_ = glGetUniformLocation(shader_, "u_resolution");
    u_color_ = glGetUniformLocation(shader_, "u_color");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(max_particles * sizeof(Vec2)),
                 nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), nullptr);

    glBindVertexArray(0);
}

void ChainRenderer::draw(std::span<const Vec2> positions, int win_width, int win_height) {
    auto count = static_cast<GLsizei>(positions.size());
    if (count == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(positions.size_bytes()),
                    positions.data());

    glUseProgram(shader_);
    glUniform2f(u_resolution_, static_cast<float>(win_width),
                               static_cast<float>(win_height));
    glBindVertexArray(vao_);

    glUniform3f(u_color_, 0.6f, 0.6f, 0.7f);
    glDrawArrays(GL_LINE_STRIP, 0, count);

    glUniform3f(u_color_, 1.0f, 0.9f, 0.3f);
    glDrawArrays(GL_POINTS, 0, count);

    glBindVertexArray(0);
}

void ChainRenderer::cleanup() {
    if (shader_) glDeleteProgram(shader_);
    if (vbo_)    glDeleteBuffers(1, &vbo_);
    if (vao_)    glDeleteVertexArrays(1, &vao_);
    shader_ = 0;
    vbo_ = 0;
    vao_ = 0;
}
