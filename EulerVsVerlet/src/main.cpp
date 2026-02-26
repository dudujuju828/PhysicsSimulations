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
constexpr int   kInitialHeight  = 700;
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
};

static constexpr Preset kPresets[] = {
    {"Gentle Spring",     4.0f,   {80.0f,  0.0f}, 1.0f / 60.0f},
    {"Stiff Spring",      50.0f,  {80.0f,  0.0f}, 1.0f / 60.0f},
    {"Very Stiff Spring", 200.0f, {80.0f,  0.0f}, 1.0f / 60.0f},
    {"Large Timestep",    20.0f,  {80.0f,  0.0f}, 1.0f / 20.0f},
    {"Diagonal Launch",   20.0f,  {60.0f, 60.0f}, 1.0f / 60.0f},
};
constexpr int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

// --- Application state ---
struct AppState {
    SpringEuler  euler;
    SpringVerlet verlet;
    Renderer     renderer;

    int   win_width    = kInitialWidth;
    int   win_height   = kInitialHeight;
    int   preset_index = 0;
    float preset_timer = 0.0f;
    float accumulator  = 0.0f;
    bool  paused       = false;
};

static Vec2 euler_anchor(int w, int h) {
    return {w * 0.25f, h * 0.5f};
}

static Vec2 verlet_anchor(int w, int h) {
    return {w * 0.75f, h * 0.5f};
}

static void reset_preset(AppState& app) {
    const Preset& p = kPresets[app.preset_index];
    app.euler.reset(euler_anchor(app.win_width, app.win_height),
                    p.offset, p.k, kMass);
    app.verlet.reset(verlet_anchor(app.win_width, app.win_height),
                     p.offset, p.k, kMass);
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

static void draw_scene(AppState& app) {
    int w = app.win_width;
    int h = app.win_height;
    const Preset& preset = kPresets[app.preset_index];
    Renderer& r = app.renderer;

    // Vertical divider
    Vec2 divider[2] = {{w * 0.5f, 0.0f}, {w * 0.5f, static_cast<float>(h)}};
    r.draw_lines({divider, 2}, 0.3f, 0.3f, 0.3f, 1.0f, w, h);

    // --- Euler side (left) ---
    {
        Vec2 anchor = app.euler.anchor();
        Vec2 pos    = app.euler.pos();

        // Trail
        Vec2 trail_buf[Trail::kCapacity];
        std::size_t n = app.euler.trail().extract(trail_buf);
        if (n >= 2)
            r.draw_line_strip({trail_buf, n}, 0.3f, 0.5f, 1.0f, 0.3f, w, h);

        // Spring line
        Vec2 spring[2] = {anchor, pos};
        r.draw_lines({spring, 2}, 0.5f, 0.5f, 0.5f, 1.0f, w, h);

        // Anchor
        r.draw_points({&anchor, 1}, 6.0f, 0.8f, 0.8f, 0.8f, w, h);

        // Ball
        r.draw_points({&pos, 1}, 12.0f, 0.3f, 0.6f, 1.0f, w, h);
    }

    // --- Verlet side (right) ---
    {
        Vec2 anchor = app.verlet.anchor();
        Vec2 pos    = app.verlet.pos();

        Vec2 trail_buf[Trail::kCapacity];
        std::size_t n = app.verlet.trail().extract(trail_buf);
        if (n >= 2)
            r.draw_line_strip({trail_buf, n}, 1.0f, 0.5f, 0.3f, 0.3f, w, h);

        Vec2 spring[2] = {anchor, pos};
        r.draw_lines({spring, 2}, 0.5f, 0.5f, 0.5f, 1.0f, w, h);

        r.draw_points({&anchor, 1}, 6.0f, 0.8f, 0.8f, 0.8f, w, h);
        r.draw_points({&pos, 1}, 12.0f, 1.0f, 0.5f, 0.2f, w, h);
    }

    // --- Text ---
    float s = kTextScale;

    // "Euler" label (top of left half)
    {
        float tw = stb_easy_font_width(const_cast<char*>("Euler")) * s;
        r.draw_text("Euler", w * 0.25f - tw * 0.5f, 20.0f, s,
                    0.3f, 0.6f, 1.0f, w, h);
    }

    // "Verlet" label (top of right half)
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

    // Energy readouts
    char buf[64];

    std::snprintf(buf, sizeof(buf), "E = %.1f", app.euler.energy());
    {
        float tw = stb_easy_font_width(buf) * s;
        r.draw_text(buf, w * 0.25f - tw * 0.5f, h * 0.5f + 100.0f, s,
                    0.7f, 0.7f, 0.7f, w, h);
    }

    std::snprintf(buf, sizeof(buf), "E = %.1f", app.verlet.energy(preset.dt));
    {
        float tw = stb_easy_font_width(buf) * s;
        r.draw_text(buf, w * 0.75f - tw * 0.5f, h * 0.5f + 100.0f, s,
                    0.7f, 0.7f, 0.7f, w, h);
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
