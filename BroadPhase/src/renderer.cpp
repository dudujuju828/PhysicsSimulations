#include "renderer.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

static constexpr float kPI = 3.14159265f;
static constexpr int   kCircleSegments = 32;
static constexpr float kTextScale = 2.0f;

// --- Depth-based colors for BVH visualization ---
struct Color3 { float r, g, b; };

static Color3 depth_color(int depth, int max_depth) {
    // Rainbow: red -> orange -> yellow -> green -> cyan -> blue
    float t = (max_depth > 0) ? static_cast<float>(depth) / max_depth : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float r, g, b;
    if (t < 0.2f)      { float s = t / 0.2f; r = 1.0f;     g = s * 0.5f;   b = 0.0f; }
    else if (t < 0.4f) { float s = (t-0.2f)/0.2f; r = 1.0f-s*0.2f; g = 0.5f+s*0.5f; b = 0.0f; }
    else if (t < 0.6f) { float s = (t-0.4f)/0.2f; r = 0.8f-s*0.8f; g = 1.0f;         b = 0.0f; }
    else if (t < 0.8f) { float s = (t-0.6f)/0.2f; r = 0.0f;        g = 1.0f-s*0.3f;  b = s*0.8f; }
    else               { float s = (t-0.8f)/0.2f; r = 0.0f;        g = 0.7f-s*0.4f;  b = 0.8f+s*0.2f; }

    return {r, g, b};
}

// --- Shaders ---

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

// --- Shader helpers ---

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

// --- Renderer ---

void Renderer::init() {
    // Geometry pipeline
    {
        GLuint v = compile_shader(GL_VERTEX_SHADER, kGeoVertSrc);
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

    // Text pipeline
    {
        GLuint v = compile_shader(GL_VERTEX_SHADER, kTextVertSrc);
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

void Renderer::cleanup() {
    if (geo_shader_)  glDeleteProgram(geo_shader_);
    if (geo_vbo_)     glDeleteBuffers(1, &geo_vbo_);
    if (geo_vao_)     glDeleteVertexArrays(1, &geo_vao_);
    if (text_shader_) glDeleteProgram(text_shader_);
    if (text_ebo_)    glDeleteBuffers(1, &text_ebo_);
    if (text_vbo_)    glDeleteBuffers(1, &text_vbo_);
    if (text_vao_)    glDeleteVertexArrays(1, &text_vao_);
}

// --- Low-level drawing ---

void Renderer::upload_and_draw(std::span<const Vec2> pts, GLenum mode,
                                float r, float g, float b, float a,
                                int win_w, int win_h) {
    if (pts.empty() || pts.size() > kMaxGeoVerts) return;
    glBindBuffer(GL_ARRAY_BUFFER, geo_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size_bytes()), pts.data());
    glUseProgram(geo_shader_);
    glUniform2f(geo_u_res_, static_cast<float>(win_w), static_cast<float>(win_h));
    glUniform4f(geo_u_color_, r, g, b, a);
    glUniform1f(geo_u_pt_size_, 1.0f);
    glBindVertexArray(geo_vao_);
    glDrawArrays(mode, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

void Renderer::draw_line_strip(std::span<const Vec2> pts,
                                float r, float g, float b, float a,
                                int win_w, int win_h) {
    upload_and_draw(pts, GL_LINE_STRIP, r, g, b, a, win_w, win_h);
}

void Renderer::draw_lines(std::span<const Vec2> pts,
                           float r, float g, float b, float a,
                           int win_w, int win_h) {
    upload_and_draw(pts, GL_LINES, r, g, b, a, win_w, win_h);
}

void Renderer::draw_text(const char* text, float x, float y, float scale,
                          float r, float g, float b,
                          int win_w, int win_h) {
    static char buffer[4096 * 4 * 16];
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

// --- Shape primitives ---

void Renderer::draw_filled_circle(Vec2 center, float radius, float r, float g, float b, float a,
                                   int win_w, int win_h) {
    Vec2 verts[kCircleSegments + 2];
    verts[0] = center;
    for (int i = 0; i <= kCircleSegments; ++i) {
        float angle = static_cast<float>(i) * 2.0f * kPI / kCircleSegments;
        verts[i + 1] = {center.x + radius * std::cos(angle),
                        center.y + radius * std::sin(angle)};
    }
    upload_and_draw({verts, kCircleSegments + 2}, GL_TRIANGLE_FAN, r, g, b, a, win_w, win_h);
}

void Renderer::draw_circle_outline(Vec2 center, float radius, float r, float g, float b, float a,
                                    int win_w, int win_h) {
    Vec2 verts[kCircleSegments + 1];
    for (int i = 0; i <= kCircleSegments; ++i) {
        float angle = static_cast<float>(i) * 2.0f * kPI / kCircleSegments;
        verts[i] = {center.x + radius * std::cos(angle),
                    center.y + radius * std::sin(angle)};
    }
    upload_and_draw({verts, kCircleSegments + 1}, GL_LINE_STRIP, r, g, b, a, win_w, win_h);
}

void Renderer::draw_filled_polygon(std::span<const Vec2> verts, float r, float g, float b, float a,
                                    int win_w, int win_h) {
    if (verts.size() < 3) return;
    // Triangle fan from first vertex
    upload_and_draw(verts, GL_TRIANGLE_FAN, r, g, b, a, win_w, win_h);
}

void Renderer::draw_polygon_outline(std::span<const Vec2> verts, float r, float g, float b, float a,
                                     int win_w, int win_h) {
    if (verts.size() < 3) return;
    Vec2 loop[128];
    size_t n = std::min(verts.size(), (size_t)127);
    for (size_t i = 0; i < n; ++i) loop[i] = verts[i];
    loop[n] = verts[0]; // Close the loop
    upload_and_draw({loop, n + 1}, GL_LINE_STRIP, r, g, b, a, win_w, win_h);
}

void Renderer::draw_rect_outline(const AABB& box, float r, float g, float b, float a,
                                  int win_w, int win_h) {
    Vec2 verts[5] = {
        {box.min.x, box.min.y},
        {box.max.x, box.min.y},
        {box.max.x, box.max.y},
        {box.min.x, box.max.y},
        {box.min.x, box.min.y},
    };
    upload_and_draw({verts, 5}, GL_LINE_STRIP, r, g, b, a, win_w, win_h);
}

void Renderer::draw_filled_rect(const AABB& box, float r, float g, float b, float a,
                                 int win_w, int win_h) {
    Vec2 verts[4] = {
        {box.min.x, box.min.y},
        {box.max.x, box.min.y},
        {box.max.x, box.max.y},
        {box.min.x, box.max.y},
    };
    upload_and_draw({verts, 4}, GL_TRIANGLE_FAN, r, g, b, a, win_w, win_h);
}

// --- Shape drawing helpers ---

void Renderer::draw_shape_fill(const Shape& s, float r, float g, float b, float a,
                                int win_w, int win_h) {
    if (s.type == ShapeType::Circle) {
        draw_filled_circle(s.pos, s.radius, r, g, b, a, win_w, win_h);
    } else {
        draw_filled_polygon(s.world_verts, r, g, b, a, win_w, win_h);
    }
}

void Renderer::draw_shape_outline(const Shape& s, float r, float g, float b, float a,
                                   int win_w, int win_h) {
    if (s.type == ShapeType::Circle) {
        draw_circle_outline(s.pos, s.radius, r, g, b, a, win_w, win_h);
    } else {
        draw_polygon_outline(s.world_verts, r, g, b, a, win_w, win_h);
    }
}

// --- High-level rendering ---

void Renderer::render_pair_lines(const std::vector<Shape>& shapes,
                                  const std::vector<std::pair<int,int>>& pairs,
                                  float r, float g, float b, float a,
                                  int win_w, int win_h) {
    if (pairs.empty()) return;
    // Draw in batches to not exceed VBO
    std::vector<Vec2> pts;
    pts.reserve(pairs.size() * 2);
    for (auto& [i, j] : pairs) {
        if (i < 0 || i >= (int)shapes.size() || j < 0 || j >= (int)shapes.size()) continue;
        pts.push_back(shapes[i].pos);
        pts.push_back(shapes[j].pos);
        if (pts.size() >= kMaxGeoVerts - 2) {
            upload_and_draw(pts, GL_LINES, r, g, b, a, win_w, win_h);
            pts.clear();
        }
    }
    if (!pts.empty())
        upload_and_draw(pts, GL_LINES, r, g, b, a, win_w, win_h);
}

void Renderer::render_bvh_boxes(const BVH& bvh, const UIState& ui, int win_w, int win_h) {
    auto& nodes = bvh.nodes();
    int max_d = bvh.max_depth();
    int limit = ui.build_anim_active ? ui.build_anim_step + 1 : (int)nodes.size();

    for (int i = 0; i < limit && i < (int)nodes.size(); ++i) {
        auto& n = nodes[i];
        if (n.shape_index >= 0) continue; // Skip leaf nodes
        Color3 c = depth_color(n.depth, max_d);
        draw_rect_outline(n.bounds, c.r, c.g, c.b, 0.4f, win_w, win_h);
    }
}

void Renderer::render_aabb_overlays(const std::vector<AABB>& aabbs,
                                     const std::vector<std::pair<int,int>>& broad_pairs,
                                     int win_w, int win_h) {
    // Collect indices that are in an overlapping pair
    std::set<int> overlapping;
    for (auto& [i, j] : broad_pairs) {
        overlapping.insert(i);
        overlapping.insert(j);
    }

    for (int i = 0; i < (int)aabbs.size(); ++i) {
        if (overlapping.count(i)) {
            draw_filled_rect(aabbs[i], 0.9f, 0.5f, 0.1f, 0.15f, win_w, win_h);
            draw_rect_outline(aabbs[i], 0.9f, 0.5f, 0.1f, 0.5f, win_w, win_h);
        } else {
            draw_filled_rect(aabbs[i], 0.2f, 0.4f, 0.8f, 0.1f, win_w, win_h);
            draw_rect_outline(aabbs[i], 0.3f, 0.5f, 0.9f, 0.35f, win_w, win_h);
        }
    }
}

void Renderer::render_query_vis(const BVH& bvh,
                                 const std::vector<TraversalStep>& steps,
                                 const UIState& ui,
                                 int win_w, int win_h) {
    auto& nodes = bvh.nodes();
    int limit = (ui.step_mode && ui.step_index >= 0) ? ui.step_index + 1 : (int)steps.size();

    for (int i = 0; i < limit && i < (int)steps.size(); ++i) {
        auto& step = steps[i];
        if (step.node_index < 0 || step.node_index >= (int)nodes.size()) continue;
        const AABB& box = nodes[step.node_index].bounds;

        bool is_current = (i == limit - 1 && ui.step_mode);
        float alpha = is_current ? 0.8f : 0.4f;

        switch (step.action) {
        case TraversalAction::Visit:
            draw_rect_outline(box, 0.2f, 0.9f, 0.3f, alpha, win_w, win_h);
            break;
        case TraversalAction::Prune:
            draw_filled_rect(box, 0.9f, 0.2f, 0.2f, 0.08f, win_w, win_h);
            draw_rect_outline(box, 0.9f, 0.2f, 0.2f, alpha, win_w, win_h);
            break;
        case TraversalAction::LeafTest:
            draw_filled_rect(box, 0.2f, 0.9f, 0.3f, 0.15f, win_w, win_h);
            draw_rect_outline(box, 0.2f, 1.0f, 0.3f, alpha, win_w, win_h);
            break;
        }
    }

    // Show step annotation
    if (ui.step_mode && ui.step_index >= 0 && ui.step_index < (int)steps.size()) {
        auto& step = steps[ui.step_index];
        char buf[128];
        const char* action_str =
            step.action == TraversalAction::Visit ? "VISIT" :
            step.action == TraversalAction::Prune ? "PRUNE" :
            "LEAF TEST";
        int subtree = (step.node_index < (int)nodes.size()) ?
                      nodes[step.node_index].subtree_size : 0;

        if (step.action == TraversalAction::Prune)
            std::snprintf(buf, sizeof(buf), "Step %d: %s node %d (skipped %d shapes)",
                         ui.step_index + 1, action_str, step.node_index, subtree);
        else if (step.action == TraversalAction::LeafTest)
            std::snprintf(buf, sizeof(buf), "Step %d: %s node %d -> shape %d",
                         ui.step_index + 1, action_str, step.node_index, step.partner_shape);
        else
            std::snprintf(buf, sizeof(buf), "Step %d: %s node %d (descending...)",
                         ui.step_index + 1, action_str, step.node_index);

        // Draw at bottom-center
        float tw = stb_easy_font_width(buf) * kTextScale;
        draw_text(buf, win_w * 0.5f - tw * 0.5f, win_h - 60.0f, kTextScale,
                  1.0f, 1.0f, 0.5f, win_w, win_h);
    }
}

void Renderer::render_tree_diagram(const BVH& bvh, const UIState& ui, int win_w, int win_h) {
    auto& nodes = bvh.nodes();
    if (nodes.empty()) return;

    // Tree diagram in right portion of screen
    float panel_x = win_w * 0.75f;
    float panel_w = win_w * 0.24f;
    float panel_y_top = 60.0f;  // screen-down coords for text
    float panel_h = win_h * 0.45f;
    int max_d = bvh.max_depth();
    if (max_d <= 0) max_d = 1;

    // Draw panel background
    // (screen y-down for text, but y-up for geo — convert)
    AABB panel_bg = {{panel_x - 5, (float)win_h - panel_y_top - panel_h - 5},
                     {panel_x + panel_w + 5, (float)win_h - panel_y_top + 5}};
    draw_filled_rect(panel_bg, 0.08f, 0.08f, 0.12f, 0.85f, win_w, win_h);

    // BFS to assign positions
    struct NodePos { int idx; float x, y; };
    std::vector<NodePos> positions;
    positions.reserve(nodes.size());

    // Simple recursive layout
    struct LayoutHelper {
        const std::vector<BVHNode>& nodes;
        std::vector<NodePos>& positions;
        float panel_x, panel_w, panel_y_top, panel_h;
        int max_depth;
        int limit;

        void layout(int idx, float x_min, float x_max, int depth) {
            if (idx < 0 || idx >= (int)nodes.size() || idx >= limit) return;
            float x = (x_min + x_max) * 0.5f;
            float y = panel_y_top + 15.0f + (panel_h - 30.0f) * depth / std::max(max_depth, 1);
            positions.push_back({idx, x, y});

            auto& n = nodes[idx];
            if (n.left >= 0 && n.left < limit)
                layout(n.left, x_min, x, depth + 1);
            if (n.right >= 0 && n.right < limit)
                layout(n.right, x, x_max, depth + 1);
        }
    };

    int limit = ui.build_anim_active ? ui.build_anim_step + 1 : (int)nodes.size();
    LayoutHelper helper{nodes, positions, panel_x, panel_w, panel_y_top, panel_h, max_d, limit};
    helper.layout(0, panel_x, panel_x + panel_w, 0);

    // Draw edges (in y-up coords)
    for (auto& np : positions) {
        auto& n = nodes[np.idx];
        // Find children positions
        for (auto& cp : positions) {
            if (cp.idx == n.left || cp.idx == n.right) {
                // Convert text y-down to geo y-up
                Vec2 parent = {np.x, (float)win_h - np.y};
                Vec2 child = {cp.x, (float)win_h - cp.y};
                Vec2 line[2] = {parent, child};
                draw_lines({line, 2}, 0.4f, 0.4f, 0.5f, 0.6f, win_w, win_h);
            }
        }
    }

    // Draw nodes
    for (auto& np : positions) {
        auto& n = nodes[np.idx];
        Color3 c = depth_color(n.depth, max_d);
        float node_r = 4.0f;
        Vec2 center = {np.x, (float)win_h - np.y};

        if (n.shape_index >= 0) {
            // Leaf: filled circle
            draw_filled_circle(center, node_r, c.r, c.g, c.b, 0.9f, win_w, win_h);
        } else {
            draw_circle_outline(center, node_r, c.r, c.g, c.b, 0.9f, win_w, win_h);
        }
    }

    // Title
    draw_text("BVH Tree", panel_x, panel_y_top - 15.0f, kTextScale,
              0.7f, 0.7f, 0.8f, win_w, win_h);
}

void Renderer::render_sliders(const UIState& ui, float slider_count_val,
                               int win_w, int win_h) {
    float sx = 15.0f;
    float sy = 15.0f;
    float sw = 150.0f;
    float sh = 8.0f;

    // Object count slider
    {
        // Track (y-up coords)
        float track_y = win_h - sy - sh;
        AABB track = {{sx, track_y}, {sx + sw, track_y + sh}};
        draw_filled_rect(track, 0.2f, 0.2f, 0.25f, 0.8f, win_w, win_h);

        // Handle
        float t = (slider_count_val - 5.0f) / 195.0f;
        float hx = sx + t * sw;
        AABB handle = {{hx - 4, track_y - 2}, {hx + 4, track_y + sh + 2}};
        draw_filled_rect(handle, 0.5f, 0.7f, 1.0f, 0.9f, win_w, win_h);

        // Label
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Objects: %d", ui.target_count);
        draw_text(buf, sx, sy + sh + 5.0f, kTextScale * 0.8f, 0.7f, 0.7f, 0.7f, win_w, win_h);
    }

    // Speed slider
    {
        float sy2 = sy + 40.0f;
        float track_y = win_h - sy2 - sh;
        AABB track = {{sx, track_y}, {sx + sw, track_y + sh}};
        draw_filled_rect(track, 0.2f, 0.2f, 0.25f, 0.8f, win_w, win_h);

        float t = ui.speed_mult / 3.0f;
        float hx = sx + t * sw;
        AABB handle = {{hx - 4, track_y - 2}, {hx + 4, track_y + sh + 2}};
        draw_filled_rect(handle, 0.5f, 0.7f, 1.0f, 0.9f, win_w, win_h);

        char buf[32];
        std::snprintf(buf, sizeof(buf), "Speed: %.1fx", ui.speed_mult);
        draw_text(buf, sx, sy2 + sh + 5.0f, kTextScale * 0.8f, 0.7f, 0.7f, 0.7f, win_w, win_h);
    }
}

void Renderer::render_stats(const UIState& ui, int win_w, int win_h) {
    float x = win_w - 220.0f;
    float y = 15.0f;
    float line_h = 18.0f;
    float s = kTextScale * 0.8f;

    // Background
    AABB bg = {{x - 10, (float)win_h - y - 10 * line_h - 5},
               {(float)win_w - 5.0f, (float)win_h - y + 10}};
    draw_filled_rect(bg, 0.05f, 0.05f, 0.1f, 0.85f, win_w, win_h);

    char buf[64];

    std::snprintf(buf, sizeof(buf), "FPS: %.0f", ui.fps);
    draw_text(buf, x, y, s, 0.6f, 0.8f, 0.6f, win_w, win_h); y += line_h;

    std::snprintf(buf, sizeof(buf), "Shapes: %d", ui.target_count);
    draw_text(buf, x, y, s, 0.7f, 0.7f, 0.7f, win_w, win_h); y += line_h;

    std::snprintf(buf, sizeof(buf), "Mode: %s", ui.use_bvh ? "BVH" : "Brute Force");
    draw_text(buf, x, y, s, 0.7f, 0.7f, 0.9f, win_w, win_h); y += line_h;

    std::snprintf(buf, sizeof(buf), "BVH nodes: %d", ui.bvh_node_count);
    draw_text(buf, x, y, s, 0.7f, 0.7f, 0.7f, win_w, win_h); y += line_h;

    std::snprintf(buf, sizeof(buf), "Broad pairs: %d", ui.broad_phase_pairs);
    draw_text(buf, x, y, s, 0.8f, 0.7f, 0.5f, win_w, win_h); y += line_h;

    if (ui.show_brute_compare) {
        int total = ui.target_count * (ui.target_count - 1) / 2;
        std::snprintf(buf, sizeof(buf), "Brute tests: %d", total);
        draw_text(buf, x, y, s, 0.6f, 0.6f, 0.6f, win_w, win_h); y += line_h;

        if (total > 0) {
            float ratio = 100.0f * ui.broad_phase_pairs / total;
            std::snprintf(buf, sizeof(buf), "BVH saves: %.0f%%", 100.0f - ratio);
            draw_text(buf, x, y, s, 0.4f, 0.9f, 0.4f, win_w, win_h); y += line_h;
        }

        if (ui.bvh_mismatch) {
            draw_text("BVH MISMATCH!", x, y, s, 1.0f, 0.2f, 0.2f, win_w, win_h); y += line_h;
        } else if (ui.use_bvh) {
            draw_text("BVH verified OK", x, y, s, 0.3f, 0.7f, 0.3f, win_w, win_h); y += line_h;
        }
    }

    if (ui.show_narrow_phase) {
        std::snprintf(buf, sizeof(buf), "Collisions: %d", ui.actual_collisions);
        draw_text(buf, x, y, s, 0.3f, 0.9f, 0.3f, win_w, win_h); y += line_h;

        std::snprintf(buf, sizeof(buf), "False pos: %d", ui.false_positives);
        draw_text(buf, x, y, s, 0.9f, 0.9f, 0.3f, win_w, win_h); y += line_h;
    }
}

void Renderer::render_controls_hint(int win_w, int win_h) {
    const char* hint = "1-5: layers  B: mode  SPACE: pause  N: step  R: rebuild  +/-: speed  Right-click: spawn";
    float tw = stb_easy_font_width(const_cast<char*>(hint)) * kTextScale * 0.7f;
    draw_text(hint, win_w * 0.5f - tw * 0.5f, win_h - 20.0f, kTextScale * 0.7f,
              0.35f, 0.35f, 0.4f, win_w, win_h);
}

void Renderer::render_layer_toggles(const UIState& ui, int win_w, int win_h) {
    float x = 15.0f;
    float y = 90.0f;
    float s = kTextScale * 0.75f;
    float line_h = 16.0f;

    struct LayerInfo { bool on; const char* name; };
    LayerInfo layers[] = {
        {ui.show_aabb_overlay,  "1: AABB Overlay"},
        {ui.show_bvh_tree,      "2: BVH Tree"},
        {ui.show_query_vis,     "3: Query Vis"},
        {ui.show_brute_compare, "4: Brute Force"},
        {ui.show_narrow_phase,  "5: Narrow Phase"},
    };

    for (auto& l : layers) {
        float r = l.on ? 0.4f : 0.25f;
        float g = l.on ? 0.9f : 0.25f;
        float b = l.on ? 0.4f : 0.25f;
        draw_text(l.name, x, y, s, r, g, b, win_w, win_h);
        y += line_h;
    }
}

// --- Main render entry point ---

void Renderer::render_frame(const std::vector<Shape>& shapes,
                             const std::vector<AABB>& aabbs,
                             const BVH& bvh,
                             const std::vector<std::pair<int,int>>& broad_pairs,
                             const std::vector<std::pair<int,int>>& brute_pairs,
                             const std::set<std::pair<int,int>>& collision_set,
                             const std::set<std::pair<int,int>>& false_positive_set,
                             const std::vector<TraversalStep>& query_steps,
                             const UIState& ui,
                             float slider_count_val,
                             int win_w, int win_h) {
    // 1. Pair comparison lines (layer 4)
    if (ui.show_brute_compare) {
        render_pair_lines(shapes, brute_pairs, 0.3f, 0.3f, 0.3f, 0.15f, win_w, win_h);
        render_pair_lines(shapes, broad_pairs, 0.8f, 0.8f, 0.8f, 0.3f, win_w, win_h);
    }

    // 2. BVH internal boxes (layer 2)
    if (ui.show_bvh_tree)
        render_bvh_boxes(bvh, ui, win_w, win_h);

    // 3. AABB overlays (layer 1)
    if (ui.show_aabb_overlay)
        render_aabb_overlays(aabbs, broad_pairs, win_w, win_h);

    // 4. Shape fills
    for (int i = 0; i < (int)shapes.size(); ++i) {
        auto& s = shapes[i];
        float r = s.color[0], g = s.color[1], b = s.color[2];
        float a = 0.8f;

        if (ui.show_narrow_phase) {
            auto make_key = [](int a, int b) {
                return a < b ? std::pair(a,b) : std::pair(b,a);
            };
            bool is_colliding = false;
            bool is_false_pos = false;
            for (int j = 0; j < (int)shapes.size(); ++j) {
                if (i == j) continue;
                auto key = make_key(i, j);
                if (collision_set.count(key)) is_colliding = true;
                if (false_positive_set.count(key)) is_false_pos = true;
            }
            if (is_colliding) { r = 0.2f; g = 0.9f; b = 0.3f; }
            else if (is_false_pos) { r = 0.9f; g = 0.9f; b = 0.2f; }
        }

        // Highlight selected/hovered
        if (i == ui.selected_shape) a = 1.0f;
        if (i == ui.hovered_shape && i != ui.selected_shape) a = 0.9f;

        draw_shape_fill(s, r, g, b, a, win_w, win_h);
    }

    // 5. Shape outlines
    for (int i = 0; i < (int)shapes.size(); ++i) {
        auto& s = shapes[i];
        float dim = 0.5f;
        float r = s.color[0] * dim, g = s.color[1] * dim, b = s.color[2] * dim;
        if (i == ui.selected_shape) { r = 1.0f; g = 1.0f; b = 1.0f; }
        draw_shape_outline(s, r, g, b, 1.0f, win_w, win_h);
    }

    // 6. Query traversal highlights (layer 3)
    if (ui.show_query_vis && !query_steps.empty())
        render_query_vis(bvh, query_steps, ui, win_w, win_h);

    // 7. Tree diagram (layer 2)
    if (ui.show_bvh_tree)
        render_tree_diagram(bvh, ui, win_w, win_h);

    // 8. UI
    render_sliders(ui, slider_count_val, win_w, win_h);
    render_layer_toggles(ui, win_w, win_h);
    render_stats(ui, win_w, win_h);
    render_controls_hint(win_w, win_h);
}
