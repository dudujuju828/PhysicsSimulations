#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "vec2.h"
#include "spring_euler.h"
#include "spring_verlet.h"
#include "renderer.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

// stb_easy_font_width is defined in the header (included via renderer.cpp),
// but we need it here too for text centering.
#include "stb_easy_font.h"

// --- Simulation parameters ---
constexpr int   kInitialWidth   = 1200;
constexpr int   kInitialHeight  = 675;
constexpr float kMaxFrameDt     = 0.1f;
constexpr float kPresetDuration = 6.0f;
constexpr float kMass           = 1.0f;
constexpr float kTextScale      = 2.0f;

// --- Presets ---
struct Preset {
    const char* name;
    float k;
    Vec2 offset;
    float dt;
    float damping;
};

static constexpr Preset kPresets[] = {
    {"Gentle Spring",     4.0f,   {80.0f,  0.0f}, 1.0f / 60.0f, 0.8f},
    {"Stiff Spring",      50.0f,  {80.0f,  0.0f}, 1.0f / 60.0f, 1.5f},
    {"Large Timestep",    20.0f,  {80.0f,  0.0f}, 1.0f / 20.0f, 1.2f},
    {"Diagonal Launch",   20.0f,  {60.0f, 60.0f}, 1.0f / 60.0f, 1.2f},
};
constexpr int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

// --- Application state ---
struct AppState {
    SpringEuler  euler;           // top-left, no damping
    SpringVerlet verlet;          // top-right, no damping
    SpringEuler  euler_damped;    // bottom-left, damped
    SpringVerlet verlet_damped;   // bottom-right, damped
    Renderer     renderer;

    int   win_width    = kInitialWidth;
    int   win_height   = kInitialHeight;
    int   preset_index = 0;
    float preset_timer = 0.0f;
    float accumulator  = 0.0f;
    bool  paused       = false;
};

// Anchor positions for the four quadrants
static Vec2 anchor_top_left(int w, int h)     { return {w * 0.25f, h * 0.75f}; }
static Vec2 anchor_top_right(int w, int h)    { return {w * 0.75f, h * 0.75f}; }
static Vec2 anchor_bottom_left(int w, int h)  { return {w * 0.25f, h * 0.25f}; }
static Vec2 anchor_bottom_right(int w, int h) { return {w * 0.75f, h * 0.25f}; }

static void reset_preset(AppState& app) {
    const Preset& p = kPresets[app.preset_index];
    int w = app.win_width;
    int h = app.win_height;

    app.euler.reset(anchor_top_left(w, h), p.offset, p.k, kMass, 0.0f);
    app.verlet.reset(anchor_top_right(w, h), p.offset, p.k, kMass, 0.0f);
    app.euler_damped.reset(anchor_bottom_left(w, h), p.offset, p.k, kMass, p.damping);
    app.verlet_damped.reset(anchor_bottom_right(w, h), p.offset, p.k, kMass, p.damping);

    app.accumulator  = 0.0f;
    app.preset_timer = 0.0f;
}

static void next_preset(AppState& app) {
    app.preset_index = (app.preset_index + 1) % kNumPresets;
    reset_preset(app);
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

// --- Drawing -----------------------------------------------------------------

// Draw a single sim quadrant: trail, spring line, anchor, ball, energy text.
// ball_r/g/b and trail_r/g/b set the colors. energy_y is the screen-y for
// the energy readout text.
struct QuadrantInfo {
    Vec2  anchor;
    Vec2  pos;
    const Trail* trail;
    float ball_r, ball_g, ball_b;
    float trail_r, trail_g, trail_b;
    float energy;
    float center_x;   // horizontal center of this quadrant
    float energy_y;    // screen-y for the energy readout
};

static void draw_quadrant(Renderer& r, const QuadrantInfo& q, int w, int h) {
    float s = kTextScale;

    // Trail
    Vec2 trail_buf[Trail::kCapacity];
    std::size_t n = q.trail->extract(trail_buf);
    if (n >= 2)
        r.draw_line_strip({trail_buf, n}, q.trail_r, q.trail_g, q.trail_b, 0.3f, w, h);

    // Spring line
    Vec2 spring[2] = {q.anchor, q.pos};
    r.draw_lines({spring, 2}, 0.5f, 0.5f, 0.5f, 1.0f, w, h);

    // Anchor
    r.draw_points({&q.anchor, 1}, 6.0f, 0.8f, 0.8f, 0.8f, w, h);

    // Ball
    r.draw_points({&q.pos, 1}, 12.0f, q.ball_r, q.ball_g, q.ball_b, w, h);

    // Energy readout
    char buf[64];
    std::snprintf(buf, sizeof(buf), "E = %.1f", q.energy);
    float tw = stb_easy_font_width(buf) * s;
    r.draw_text(buf, q.center_x - tw * 0.5f, q.energy_y, s,
                0.7f, 0.7f, 0.7f, w, h);
}

static void draw_scene(AppState& app) {
    int w = app.win_width;
    int h = app.win_height;
    const Preset& preset = kPresets[app.preset_index];
    Renderer& r = app.renderer;
    float s = kTextScale;

    // --- Dividers ---
    // Vertical divider
    Vec2 vdiv[2] = {{w * 0.5f, 0.0f}, {w * 0.5f, static_cast<float>(h)}};
    r.draw_lines({vdiv, 2}, 0.3f, 0.3f, 0.3f, 1.0f, w, h);

    // Horizontal divider
    Vec2 hdiv[2] = {{0.0f, h * 0.5f}, {static_cast<float>(w), h * 0.5f}};
    r.draw_lines({hdiv, 2}, 0.3f, 0.3f, 0.3f, 1.0f, w, h);

    // --- Draw 4 quadrants ---
    // Euler colors: ball (0.3, 0.6, 1.0), trail (0.3, 0.5, 1.0)
    // Verlet colors: ball (1.0, 0.5, 0.2), trail (1.0, 0.5, 0.3)

    // Top-left: Euler, no damping
    draw_quadrant(r, {
        app.euler.anchor(), app.euler.pos(), &app.euler.trail(),
        0.3f, 0.6f, 1.0f,   0.3f, 0.5f, 1.0f,
        app.euler.energy(),
        w * 0.25f, h * 0.5f - 60.0f
    }, w, h);

    // Top-right: Verlet, no damping
    draw_quadrant(r, {
        app.verlet.anchor(), app.verlet.pos(), &app.verlet.trail(),
        1.0f, 0.5f, 0.2f,   1.0f, 0.5f, 0.3f,
        app.verlet.energy(preset.dt),
        w * 0.75f, h * 0.5f - 60.0f
    }, w, h);

    // Bottom-left: Euler, damped
    draw_quadrant(r, {
        app.euler_damped.anchor(), app.euler_damped.pos(), &app.euler_damped.trail(),
        0.3f, 0.6f, 1.0f,   0.3f, 0.5f, 1.0f,
        app.euler_damped.energy(),
        w * 0.25f, static_cast<float>(h) - 60.0f
    }, w, h);

    // Bottom-right: Verlet, damped
    draw_quadrant(r, {
        app.verlet_damped.anchor(), app.verlet_damped.pos(), &app.verlet_damped.trail(),
        1.0f, 0.5f, 0.2f,   1.0f, 0.5f, 0.3f,
        app.verlet_damped.energy(preset.dt),
        w * 0.75f, static_cast<float>(h) - 60.0f
    }, w, h);

    // --- Text labels ---

    // Column labels at very top
    {
        float tw = stb_easy_font_width(const_cast<char*>("Forward Euler")) * s;
        r.draw_text("Forward Euler", w * 0.25f - tw * 0.5f, 20.0f, s,
                    0.3f, 0.6f, 1.0f, w, h);
    }
    {
        float tw = stb_easy_font_width(const_cast<char*>("Verlet")) * s;
        r.draw_text("Verlet", w * 0.75f - tw * 0.5f, 20.0f, s,
                    1.0f, 0.5f, 0.2f, w, h);
    }

    // Preset name (top center)
    {
        float tw = stb_easy_font_width(const_cast<char*>(preset.name)) * s;
        r.draw_text(preset.name, w * 0.5f - tw * 0.5f, 8.0f, s,
                    0.9f, 0.9f, 0.9f, w, h);
    }

    // Row labels
    {
        const char* label = "No Damping";
        float tw = stb_easy_font_width(const_cast<char*>(label)) * s;
        r.draw_text(label, w * 0.5f - tw * 0.5f, 38.0f, s,
                    0.6f, 0.6f, 0.6f, w, h);
    }
    {
        const char* label = "With Damping";
        float tw = stb_easy_font_width(const_cast<char*>(label)) * s;
        r.draw_text(label, w * 0.5f - tw * 0.5f, h * 0.5f + 8.0f, s,
                    0.6f, 0.6f, 0.6f, w, h);
    }

    // Controls hint at bottom
    {
        const char* hint = "SPACE: pause  N/Right: next  R: reset";
        float tw = stb_easy_font_width(const_cast<char*>(hint)) * s;
        r.draw_text(hint, w * 0.5f - tw * 0.5f, static_cast<float>(h) - 30.0f, s,
                    0.4f, 0.4f, 0.4f, w, h);
    }
}

// --- Entry point -------------------------------------------------------------

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(kInitialWidth, kInitialHeight,
                                          "Euler vs Verlet", nullptr, nullptr);
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
    reset_preset(app);

    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetKeyCallback(window, key_callback);

    double prev_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float frame_dt = static_cast<float>(now - prev_time);
        prev_time = now;
        if (frame_dt > kMaxFrameDt) frame_dt = kMaxFrameDt;

        const Preset& preset = kPresets[app.preset_index];

        if (!app.paused) {
            app.preset_timer += frame_dt;
            if (app.preset_timer >= kPresetDuration) {
                next_preset(app);
            }

            app.accumulator += frame_dt;
            while (app.accumulator >= preset.dt) {
                app.euler.step(preset.dt);
                app.verlet.step(preset.dt);
                app.euler_damped.step(preset.dt);
                app.verlet_damped.step(preset.dt);
                app.accumulator -= preset.dt;
            }
        }

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_scene(app);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    app.renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
