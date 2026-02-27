#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "vec3.h"
#include "mat4.h"
#include "quat.h"
#include "sphere.h"
#include "renderer.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>

#include "stb_easy_font.h"

// --- Constants ---------------------------------------------------------------

constexpr int   kInitialWidth  = 1200;
constexpr int   kInitialHeight = 675;
constexpr float kMaxFrameDt    = 0.1f;
constexpr float kHoldTime      = 0.5f;
constexpr float kTextScale     = 2.0f;
constexpr float kPi            = 3.14159265358979323846f;
constexpr float kDeg2Rad       = kPi / 180.0f;

constexpr int   kPathSamples   = 64;
constexpr int   kMarkerCount   = 20;

// --- Presets -----------------------------------------------------------------

struct Preset {
    const char* name;
    quat q_start;
    quat q_end;
    float duration;
};

static Preset make_presets() { return {}; } // placeholder

static Preset kPresets[] = {
    {"90 deg X-axis",
     quat::identity(),
     quat::from_axis_angle({1, 0, 0}, 90.0f * kDeg2Rad),
     5.0f},

    {"180 deg Y-axis",
     quat::identity(),
     quat::from_axis_angle({0, 1, 0}, 180.0f * kDeg2Rad),
     6.0f},

    {"Diagonal 120 deg",
     quat::identity(),
     quat::from_axis_angle(normalize(vec3{1, 1, 0}), 120.0f * kDeg2Rad),
     5.0f},

    {"Twist combo",
     quat::from_axis_angle({0, 0, 1}, 45.0f * kDeg2Rad),
     quat::from_axis_angle({1, 0, 0}, 90.0f * kDeg2Rad) * quat::from_axis_angle({0, 1, 0}, 90.0f * kDeg2Rad),
     6.0f},

    {"Small angle",
     quat::identity(),
     quat::from_axis_angle({0, 1, 0}, 10.0f * kDeg2Rad),
     4.0f},
};
constexpr int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

// --- Pre-computed path data --------------------------------------------------

struct PathData {
    vec3 lerp_path[kPathSamples];
    vec3 slerp_path[kPathSamples];
    vec3 lerp_markers[kMarkerCount];
    vec3 slerp_markers[kMarkerCount];
    vec3 start_pos;
    vec3 end_pos;
};

static PathData compute_paths(const Preset& p) {
    PathData d{};
    vec3 x_axis = {1, 0, 0};

    for (int i = 0; i < kPathSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kPathSamples - 1);
        d.lerp_path[i]  = lerp(p.q_start, p.q_end, t).rotate_vec(x_axis);
        d.slerp_path[i] = slerp(p.q_start, p.q_end, t).rotate_vec(x_axis);
    }

    for (int i = 0; i < kMarkerCount; ++i) {
        float t = static_cast<float>(i + 1) / static_cast<float>(kMarkerCount);
        d.lerp_markers[i]  = lerp(p.q_start, p.q_end, t).rotate_vec(x_axis);
        d.slerp_markers[i] = slerp(p.q_start, p.q_end, t).rotate_vec(x_axis);
    }

    d.start_pos = p.q_start.rotate_vec(x_axis);
    d.end_pos   = p.q_end.rotate_vec(x_axis);
    return d;
}

// --- Application state -------------------------------------------------------

struct AppState {
    Renderer   renderer;
    SphereData sphere;
    PathData   paths;

    int   win_width  = kInitialWidth;
    int   win_height = kInitialHeight;
    int   preset_index = 1;  // start on 180° Y-axis (most visible difference)
    float t            = 0.0f;
    float hold_timer   = 0.0f;
    bool  paused       = false;
    bool  holding      = false;

    // Camera
    float cam_azimuth   = 45.0f;   // degrees
    float cam_elevation = 25.0f;   // degrees
    float cam_distance  = 3.5f;

    // Mouse drag state
    bool  dragging  = false;
    double last_mx  = 0.0;
    double last_my  = 0.0;
};

static void reset_preset(AppState& app) {
    app.t = 0.0f;
    app.hold_timer = 0.0f;
    app.holding = false;
    app.paths = compute_paths(kPresets[app.preset_index]);
}

static void next_preset(AppState& app) {
    app.preset_index = (app.preset_index + 1) % kNumPresets;
    reset_preset(app);
}

// --- Camera ------------------------------------------------------------------

static mat4 build_view(const AppState& app) {
    float az = app.cam_azimuth * kDeg2Rad;
    float el = app.cam_elevation * kDeg2Rad;
    float d  = app.cam_distance;

    vec3 eye = {
        d * std::cos(el) * std::sin(az),
        d * std::sin(el),
        d * std::cos(el) * std::cos(az)
    };
    return mat4::look_at(eye, {0, 0, 0}, {0, 1, 0});
}

static mat4 build_projection(int half_width, int height) {
    float aspect = static_cast<float>(half_width) / static_cast<float>(height);
    return mat4::perspective(45.0f * kDeg2Rad, aspect, 0.1f, 100.0f);
}

// --- GLFW callbacks ----------------------------------------------------------

static void key_callback(GLFWwindow* window, int key, int /*scancode*/,
                          int action, int /*mods*/) {
    if (action != GLFW_PRESS) return;
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_SPACE) {
        app->paused = !app->paused;
    } else if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_N) {
        next_preset(*app);
    } else if (key == GLFW_KEY_R) {
        reset_preset(*app);
    } else if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            app->dragging = true;
            glfwGetCursorPos(window, &app->last_mx, &app->last_my);
        } else {
            app->dragging = false;
        }
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!app->dragging) return;

    double dx = xpos - app->last_mx;
    double dy = ypos - app->last_my;
    app->last_mx = xpos;
    app->last_my = ypos;

    app->cam_azimuth   += static_cast<float>(dx) * 0.3f;
    app->cam_elevation -= static_cast<float>(dy) * 0.3f;

    // Clamp elevation
    if (app->cam_elevation >  89.0f) app->cam_elevation =  89.0f;
    if (app->cam_elevation < -89.0f) app->cam_elevation = -89.0f;
}

static void window_size_callback(GLFWwindow* window, int w, int h) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    app->win_width  = w;
    app->win_height = h;
}

// --- Per-viewport drawing ----------------------------------------------------

static void draw_viewport(Renderer& r, const mat4& mvp,
                          const SphereData& sphere,
                          quat q_current,
                          const vec3* path, int path_count,
                          const vec3* markers, int marker_count,
                          vec3 start_pos, vec3 end_pos,
                          bool is_lerp) {
    // 1. Wireframe sphere (dim gray)
    r.draw_lines_3d(sphere.lines, mvp, {0.25f, 0.25f, 0.25f, 0.4f});

    // Equator (slightly brighter)
    r.draw_lines_3d(sphere.equator_lines, mvp, {0.35f, 0.35f, 0.35f, 0.4f});

    // 2. World axes (very dim)
    r.draw_lines_3d(sphere.axis_lines, mvp, {0.3f, 0.3f, 0.3f, 0.2f});

    // 3. Rotating coordinate frame
    vec3 ox = q_current.rotate_vec({1, 0, 0});
    vec3 oy = q_current.rotate_vec({0, 1, 0});
    vec3 oz = q_current.rotate_vec({0, 0, 1});
    vec3 origin = {0, 0, 0};

    vec3 frame_x[2] = {origin, ox};
    vec3 frame_y[2] = {origin, oy};
    vec3 frame_z[2] = {origin, oz};

    r.draw_lines_3d(frame_x, mvp, {1.0f, 0.3f, 0.3f, 1.0f});
    r.draw_lines_3d(frame_y, mvp, {0.3f, 1.0f, 0.3f, 1.0f});
    r.draw_lines_3d(frame_z, mvp, {0.3f, 0.3f, 1.0f, 1.0f});

    // 4. Interpolation path (yellow line strip)
    r.draw_line_strip_3d({path, static_cast<std::size_t>(path_count)},
                         mvp, {1.0f, 0.85f, 0.2f, 0.7f});

    // 5. Time markers (orange, large)
    r.draw_points_3d({markers, static_cast<std::size_t>(marker_count)},
                     mvp, {1.0f, 0.6f, 0.1f, 1.0f}, 10.0f);

    // 6. Start/end markers
    r.draw_points_3d({&start_pos, 1}, mvp, {0.2f, 1.0f, 0.2f, 1.0f}, 12.0f);
    r.draw_points_3d({&end_pos, 1},   mvp, {1.0f, 0.2f, 0.2f, 1.0f}, 12.0f);

    // 7. Current position (white)
    vec3 cur = q_current.rotate_vec({1, 0, 0});
    r.draw_points_3d({&cur, 1}, mvp, {1.0f, 1.0f, 1.0f, 1.0f}, 14.0f);

    // 8. LERP chord line (left viewport only) — bright so it's clearly visible
    if (is_lerp) {
        vec3 chord[2] = {start_pos, end_pos};
        r.draw_lines_3d(chord, mvp, {1.0f, 0.3f, 0.3f, 0.7f});
    }
}

// --- Main --------------------------------------------------------------------

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(kInitialWidth, kInitialHeight,
                                          "Quaternion SLERP vs LERP", nullptr, nullptr);
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
    generate_sphere(app.sphere);
    reset_preset(app);

    glfwSetWindowUserPointer(window, &app);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);

    double prev_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float frame_dt = static_cast<float>(now - prev_time);
        prev_time = now;
        if (frame_dt > kMaxFrameDt) frame_dt = kMaxFrameDt;

        // Get framebuffer size for glViewport
        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);

        const Preset& preset = kPresets[app.preset_index];

        // --- Animation update ---
        if (!app.paused) {
            if (app.holding) {
                app.hold_timer += frame_dt;
                if (app.hold_timer >= kHoldTime) {
                    next_preset(app);
                }
            } else {
                app.t += frame_dt / preset.duration;
                if (app.t >= 1.0f) {
                    app.t = 1.0f;
                    app.holding = true;
                    app.hold_timer = 0.0f;
                }
            }
        }

        // Current quaternions
        quat q_lerp  = lerp(preset.q_start, preset.q_end, app.t);
        quat q_slerp = slerp(preset.q_start, preset.q_end, app.t);

        // Build matrices
        mat4 view = build_view(app);
        mat4 proj = build_projection(fb_w / 2, fb_h);
        mat4 mvp  = proj * view;

        // --- Render ---
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Left viewport: LERP
        glViewport(0, 0, fb_w / 2, fb_h);
        draw_viewport(app.renderer, mvp, app.sphere,
                      q_lerp,
                      app.paths.lerp_path, kPathSamples,
                      app.paths.lerp_markers, kMarkerCount,
                      app.paths.start_pos, app.paths.end_pos,
                      true);

        // Right viewport: SLERP
        glViewport(fb_w / 2, 0, fb_w / 2, fb_h);
        draw_viewport(app.renderer, mvp, app.sphere,
                      q_slerp,
                      app.paths.slerp_path, kPathSamples,
                      app.paths.slerp_markers, kMarkerCount,
                      app.paths.start_pos, app.paths.end_pos,
                      false);

        // --- 2D text overlay ---
        glViewport(0, 0, fb_w, fb_h);
        glDisable(GL_DEPTH_TEST);

        int w = app.win_width;
        int h = app.win_height;
        float s = kTextScale;

        // Vertical divider (as text-layer line — draw with 3D pipeline in screen space)
        // We'll draw text labels instead; the viewport split already creates the visual divide.

        // "LERP" label (top-left area)
        {
            const char* label = "LERP";
            app.renderer.draw_text(label, 15.0f, 12.0f, s,
                                   0.8f, 0.4f, 0.4f, w, h);
        }

        // "SLERP" label (top-right area)
        {
            const char* label = "SLERP";
            float tw = stb_easy_font_width(const_cast<char*>(label)) * s;
            app.renderer.draw_text(label, static_cast<float>(w) - tw - 15.0f, 12.0f, s,
                                   0.4f, 0.8f, 0.4f, w, h);
        }

        // Preset name (top center)
        {
            float tw = stb_easy_font_width(const_cast<char*>(preset.name)) * s;
            app.renderer.draw_text(preset.name, w * 0.5f - tw * 0.5f, 12.0f, s,
                                   0.9f, 0.9f, 0.9f, w, h);
        }

        // t-value readout (shows speed difference numerically)
        {
            // Compute angular progress for each method
            vec3 x_axis = {1, 0, 0};
            vec3 lerp_pos  = q_lerp.rotate_vec(x_axis);
            vec3 slerp_pos = q_slerp.rotate_vec(x_axis);
            vec3 start_pos = preset.q_start.rotate_vec(x_axis);
            vec3 end_pos   = preset.q_end.rotate_vec(x_axis);

            float total_angle = std::acos(std::min(1.0f, std::max(-1.0f, dot(start_pos, end_pos))));
            float lerp_angle  = std::acos(std::min(1.0f, std::max(-1.0f, dot(start_pos, lerp_pos))));
            float slerp_angle = std::acos(std::min(1.0f, std::max(-1.0f, dot(start_pos, slerp_pos))));

            float lerp_pct  = (total_angle > 1e-6f) ? lerp_angle / total_angle * 100.0f : 0.0f;
            float slerp_pct = (total_angle > 1e-6f) ? slerp_angle / total_angle * 100.0f : 0.0f;

            char buf[64];

            std::snprintf(buf, sizeof(buf), "t=%.0f%%  ang=%.0f%%", app.t * 100.0f, lerp_pct);
            app.renderer.draw_text(buf, 15.0f, 38.0f, s,
                                   0.7f, 0.7f, 0.7f, w, h);

            std::snprintf(buf, sizeof(buf), "t=%.0f%%  ang=%.0f%%", app.t * 100.0f, slerp_pct);
            float tw2 = stb_easy_font_width(buf) * s;
            app.renderer.draw_text(buf, static_cast<float>(w) - tw2 - 15.0f, 38.0f, s,
                                   0.7f, 0.7f, 0.7f, w, h);
        }

        // Vertical divider line
        {
            // Draw a thin vertical line at center using the 3D pipeline with ortho-like MVP
            // Simple approach: draw a full-height line as two text-space points
            // We'll just use two draw_text dashes stacked — or use the 3D pipeline
            // Actually, easiest: draw a column of dots using text
            const char* divider = "|";
            for (float dy = 0.0f; dy < static_cast<float>(h); dy += 14.0f) {
                app.renderer.draw_text(divider, w * 0.5f - 2.0f, dy, 1.5f,
                                       0.3f, 0.3f, 0.3f, w, h);
            }
        }

        // Controls hint (bottom center)
        {
            const char* hint = "SPACE: pause  N/Right: next  R: reset  Drag: orbit";
            float tw = stb_easy_font_width(const_cast<char*>(hint)) * s;
            app.renderer.draw_text(hint, w * 0.5f - tw * 0.5f,
                                   static_cast<float>(h) - 30.0f, s,
                                   0.4f, 0.4f, 0.4f, w, h);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    app.renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
