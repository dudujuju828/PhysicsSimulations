#pragma once
#include "vec2.h"
#include "aabb.h"
#include "shape.h"
#include "bvh.h"
#include "ui.h"
#include <glad/gl.h>
#include <cstddef>
#include <span>
#include <vector>
#include <utility>
#include <string>
#include <set>

class Renderer {
public:
    void init();
    void cleanup();

    // Low-level primitives
    void draw_line_strip(std::span<const Vec2> pts,
                         float r, float g, float b, float a,
                         int win_w, int win_h);
    void draw_lines(std::span<const Vec2> pts,
                    float r, float g, float b, float a,
                    int win_w, int win_h);
    void draw_text(const char* text, float x, float y, float scale,
                   float r, float g, float b,
                   int win_w, int win_h);

    // Shape rendering
    void draw_filled_circle(Vec2 center, float radius, float r, float g, float b, float a,
                            int win_w, int win_h);
    void draw_circle_outline(Vec2 center, float radius, float r, float g, float b, float a,
                             int win_w, int win_h);
    void draw_filled_polygon(std::span<const Vec2> verts, float r, float g, float b, float a,
                             int win_w, int win_h);
    void draw_polygon_outline(std::span<const Vec2> verts, float r, float g, float b, float a,
                              int win_w, int win_h);
    void draw_rect_outline(const AABB& box, float r, float g, float b, float a,
                           int win_w, int win_h);
    void draw_filled_rect(const AABB& box, float r, float g, float b, float a,
                          int win_w, int win_h);

    // High-level scene rendering
    void render_frame(const std::vector<Shape>& shapes,
                      const std::vector<AABB>& aabbs,
                      const BVH& bvh,
                      const std::vector<std::pair<int,int>>& broad_pairs,
                      const std::vector<std::pair<int,int>>& brute_pairs,
                      const std::set<std::pair<int,int>>& collision_set,
                      const std::set<std::pair<int,int>>& false_positive_set,
                      const std::vector<TraversalStep>& query_steps,
                      const UIState& ui,
                      float slider_count_val,
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

    static constexpr std::size_t kMaxGeoVerts   = 8192;
    static constexpr std::size_t kMaxTextQuads   = 4096;

    // Helpers
    void draw_shape_fill(const Shape& s, float r, float g, float b, float a, int win_w, int win_h);
    void draw_shape_outline(const Shape& s, float r, float g, float b, float a, int win_w, int win_h);

    void render_pair_lines(const std::vector<Shape>& shapes,
                           const std::vector<std::pair<int,int>>& pairs,
                           float r, float g, float b, float a,
                           int win_w, int win_h);

    void render_bvh_boxes(const BVH& bvh, const UIState& ui, int win_w, int win_h);
    void render_aabb_overlays(const std::vector<AABB>& aabbs,
                              const std::vector<std::pair<int,int>>& broad_pairs,
                              int win_w, int win_h);
    void render_query_vis(const BVH& bvh,
                          const std::vector<TraversalStep>& steps,
                          const UIState& ui,
                          int win_w, int win_h);
    void render_tree_diagram(const BVH& bvh, const UIState& ui, int win_w, int win_h);
    void render_sliders(const UIState& ui, float slider_count_val, int win_w, int win_h);
    void render_stats(const UIState& ui, int win_w, int win_h);
    void render_controls_hint(int win_w, int win_h);
    void render_layer_toggles(const UIState& ui, int win_w, int win_h);

    void upload_and_draw(std::span<const Vec2> pts, GLenum mode,
                         float r, float g, float b, float a,
                         int win_w, int win_h);
};
