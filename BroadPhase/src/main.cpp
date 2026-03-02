#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "vec2.h"
#include "aabb.h"
#include "shape.h"
#include "bvh.h"
#include "physics.h"
#include "ui.h"
#include "renderer.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <set>
#include <algorithm>

constexpr int   kInitialWidth  = 1400;
constexpr int   kInitialHeight = 800;
constexpr float kMaxFrameDt    = 0.1f;

struct AppState {
    PhysicsWorld world;
    BVH          bvh;
    Renderer     renderer;
    UIState      ui;

    int   win_width  = kInitialWidth;
    int   win_height = kInitialHeight;

    // Slider backing values (float for smooth dragging)
    float slider_count_val = 30.0f;

    // Computed per frame
    std::vector<AABB>              aabbs;
    std::vector<std::pair<int,int>> broad_pairs;
    std::vector<std::pair<int,int>> brute_pairs;
    std::set<std::pair<int,int>>    collision_set;
    std::set<std::pair<int,int>>    false_positive_set;
    std::vector<TraversalStep>      query_steps;

    // FPS tracking
    float fps_timer = 0.0f;
    int   fps_frames = 0;
};

// --- Slider hit testing ---

struct SliderDef {
    float x, y, w;  // in screen-down coords
    float min_val, max_val;
};

static constexpr SliderDef kCountSlider = {15.0f, 15.0f, 150.0f, 5.0f, 200.0f};
static constexpr SliderDef kSpeedSlider = {15.0f, 55.0f, 150.0f, 0.0f, 3.0f};

static bool slider_hit(const SliderDef& s, float screen_x, float screen_y) {
    return screen_x >= s.x - 10 && screen_x <= s.x + s.w + 10 &&
           screen_y >= s.y - 10 && screen_y <= s.y + 30;
}

static float slider_value(const SliderDef& s, float screen_x) {
    float t = (screen_x - s.x) / s.w;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return s.min_val + t * (s.max_val - s.min_val);
}

// --- GLFW callbacks ---

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

static void window_size_callback(GLFWwindow* window, int w, int h) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    app->win_width  = w;
    app->win_height = h;
}

static void key_callback(GLFWwindow* window, int key, int /*scancode*/,
                          int action, int /*mods*/) {
    if (action != GLFW_PRESS) return;
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    auto& ui = app->ui;

    switch (key) {
    case GLFW_KEY_1: ui.show_aabb_overlay  = !ui.show_aabb_overlay;  break;
    case GLFW_KEY_2: ui.show_bvh_tree      = !ui.show_bvh_tree;      break;
    case GLFW_KEY_3: ui.show_query_vis     = !ui.show_query_vis;     break;
    case GLFW_KEY_4: ui.show_brute_compare = !ui.show_brute_compare; break;
    case GLFW_KEY_5: ui.show_narrow_phase  = !ui.show_narrow_phase;  break;

    case GLFW_KEY_B:
        ui.use_bvh = !ui.use_bvh;
        break;

    case GLFW_KEY_SPACE:
        ui.paused = !ui.paused;
        if (!ui.paused) {
            ui.step_mode = false;
            ui.step_index = -1;
        }
        break;

    case GLFW_KEY_N:
        if (ui.paused) {
            if (ui.build_anim_active) {
                ui.build_anim_step++;
                if (ui.build_anim_step >= app->bvh.nodes().size())
                    ui.build_anim_active = false;
            } else if (ui.selected_shape >= 0 && !app->query_steps.empty()) {
                ui.step_mode = true;
                ui.step_index++;
                if (ui.step_index >= (int)app->query_steps.size())
                    ui.step_index = (int)app->query_steps.size() - 1;
            }
        }
        break;

    case GLFW_KEY_R:
        // Trigger build animation
        ui.build_anim_active = true;
        ui.build_anim_step = 0;
        if (!ui.paused) ui.paused = true;
        break;

    case GLFW_KEY_EQUAL: // +
    case GLFW_KEY_KP_ADD:
        ui.speed_mult = std::min(ui.speed_mult + 0.25f, 3.0f);
        break;
    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT:
        ui.speed_mult = std::max(ui.speed_mult - 0.25f, 0.0f);
        break;

    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    auto& ui = app->ui;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    float screen_x = static_cast<float>(mx);
    float screen_y = static_cast<float>(my);
    float world_x = screen_x;
    float world_y = app->win_height - screen_y;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // Check sliders first
        if (slider_hit(kCountSlider, screen_x, screen_y)) {
            ui.active_slider = 0;
            app->slider_count_val = slider_value(kCountSlider, screen_x);
            ui.target_count = static_cast<int>(app->slider_count_val + 0.5f);
            return;
        }
        if (slider_hit(kSpeedSlider, screen_x, screen_y)) {
            ui.active_slider = 1;
            ui.speed_mult = slider_value(kSpeedSlider, screen_x);
            return;
        }

        // Check shape hit
        for (int i = 0; i < (int)app->world.shapes.size(); ++i) {
            if (app->world.shapes[i].contains_point({world_x, world_y})) {
                ui.selected_shape = i;
                ui.dragged_shape = i;
                ui.drag_offset = app->world.shapes[i].pos - Vec2{world_x, world_y};
                ui.step_mode = false;
                ui.step_index = -1;
                return;
            }
        }
        // Clicked empty space
        ui.selected_shape = -1;
        ui.dragged_shape = -1;
        ui.step_mode = false;
        ui.step_index = -1;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        ui.dragged_shape = -1;
        ui.active_slider = -1;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        app->world.spawn_shape(world_x, world_y);
        app->slider_count_val = static_cast<float>(app->world.shapes.size());
        ui.target_count = static_cast<int>(app->world.shapes.size());
    }
}

static void cursor_pos_callback(GLFWwindow* window, double mx, double my) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    auto& ui = app->ui;
    float screen_x = static_cast<float>(mx);
    float screen_y = static_cast<float>(my);
    float world_x = screen_x;
    float world_y = app->win_height - screen_y;

    // Slider dragging
    if (ui.active_slider == 0) {
        app->slider_count_val = slider_value(kCountSlider, screen_x);
        ui.target_count = static_cast<int>(app->slider_count_val + 0.5f);
        return;
    }
    if (ui.active_slider == 1) {
        ui.speed_mult = slider_value(kSpeedSlider, screen_x);
        return;
    }

    // Shape dragging
    if (ui.dragged_shape >= 0 && ui.dragged_shape < (int)app->world.shapes.size()) {
        auto& s = app->world.shapes[ui.dragged_shape];
        s.pos = Vec2{world_x, world_y} + ui.drag_offset;
        if (s.type != ShapeType::Circle) s.update_world_verts();
        return;
    }

    // Hover detection
    ui.hovered_shape = -1;
    for (int i = 0; i < (int)app->world.shapes.size(); ++i) {
        if (app->world.shapes[i].contains_point({world_x, world_y})) {
            ui.hovered_shape = i;
            break;
        }
    }
}

// --- Main ---

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(kInitialWidth, kInitialHeight,
                                          "BroadPhase: BVH/AABB Collision Detection", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    int version = gladLoadGL(glfwGetProcAddress);
    if (!version) {
        std::fprintf(stderr, "Failed to load OpenGL\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    std::printf("OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version),
                                   GLAD_VERSION_MINOR(version));

    glfwSwapInterval(1);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    AppState app;
    app.renderer.init();
    app.world.ensure_count(app.ui.target_count,
                           static_cast<float>(app.win_width),
                           static_cast<float>(app.win_height));

    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    double prev_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float frame_dt = static_cast<float>(now - prev_time);
        prev_time = now;
        if (frame_dt > kMaxFrameDt) frame_dt = kMaxFrameDt;

        // FPS
        app.fps_timer += frame_dt;
        app.fps_frames++;
        if (app.fps_timer >= 0.5f) {
            app.ui.fps = app.fps_frames / app.fps_timer;
            app.fps_timer = 0.0f;
            app.fps_frames = 0;
        }

        float ww = static_cast<float>(app.win_width);
        float wh = static_cast<float>(app.win_height);

        // Ensure shape count matches slider
        app.world.ensure_count(app.ui.target_count, ww, wh);

        // Physics update
        if (!app.ui.paused) {
            app.world.update(frame_dt, app.ui.speed_mult, ww, wh);
        }

        // Compute AABBs
        int n = static_cast<int>(app.world.shapes.size());
        app.aabbs.resize(n);
        for (int i = 0; i < n; ++i)
            app.aabbs[i] = app.world.shapes[i].compute_aabb();

        // Build BVH
        app.bvh.build(app.aabbs);
        app.ui.bvh_node_count = static_cast<int>(app.bvh.nodes().size());

        // Broad phase
        if (app.ui.use_bvh) {
            app.broad_pairs = app.bvh.find_all_pairs();
        } else {
            app.broad_pairs = brute_force_pairs(app.aabbs);
        }
        app.ui.broad_phase_pairs = static_cast<int>(app.broad_pairs.size());

        // Brute force (for comparison layer)
        if (app.ui.show_brute_compare) {
            app.brute_pairs = brute_force_pairs(app.aabbs);
            app.ui.brute_force_pairs = static_cast<int>(app.brute_pairs.size());
        }

        // Narrow phase
        app.collision_set.clear();
        app.false_positive_set.clear();
        app.ui.actual_collisions = 0;
        app.ui.false_positives = 0;
        for (auto& [i, j] : app.broad_pairs) {
            if (i < 0 || i >= n || j < 0 || j >= n) continue;
            auto key = i < j ? std::pair(i,j) : std::pair(j,i);
            if (shapes_intersect(app.world.shapes[i], app.world.shapes[j])) {
                app.collision_set.insert(key);
                app.ui.actual_collisions++;
            } else {
                app.false_positive_set.insert(key);
                app.ui.false_positives++;
            }
        }
        app.ui.narrow_phase_tests = static_cast<int>(app.broad_pairs.size());

        // Query steps for selected shape
        app.query_steps.clear();
        if (app.ui.selected_shape >= 0 && app.ui.selected_shape < n && app.ui.show_query_vis) {
            app.query_steps = app.bvh.query_with_steps(
                app.aabbs[app.ui.selected_shape], app.ui.selected_shape);
        }

        // Clamp selection if shapes were removed
        if (app.ui.selected_shape >= n) app.ui.selected_shape = -1;
        if (app.ui.hovered_shape >= n)  app.ui.hovered_shape = -1;
        if (app.ui.dragged_shape >= n)  app.ui.dragged_shape = -1;

        // Render
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        app.renderer.render_frame(
            app.world.shapes,
            app.aabbs,
            app.bvh,
            app.broad_pairs,
            app.brute_pairs,
            app.collision_set,
            app.false_positive_set,
            app.query_steps,
            app.ui,
            app.slider_count_val,
            app.win_width,
            app.win_height
        );

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    app.renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
