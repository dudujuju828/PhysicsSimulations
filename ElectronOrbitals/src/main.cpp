#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "vec3.h"
#include "mat4.h"
#include "camera.h"
#include "orbital.h"
#include "renderer.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>

#include "stb_easy_font.h"

// --- Constants ---------------------------------------------------------------

constexpr int   kInitialWidth  = 1920;
constexpr int   kInitialHeight = 1080;
constexpr float kMaxFrameDt    = 0.1f;
constexpr float kTextScale     = 2.0f;

// --- Application state -------------------------------------------------------

struct AppState {
    Renderer       renderer;
    OrbitalCatalog catalog;
    Camera         camera;

    int   win_width  = kInitialWidth;
    int   win_height = kInitialHeight;

    int   orbital_index  = 0;
    float density_scale  = 1.0f;
    float bloom_intensity = 0.5f;
    int   max_steps      = 128;
    float anim_speed     = 1.0f;
    float anim_time      = 0.0f;
    bool  paused         = false;

    // Mouse state
    bool   left_dragging  = false;
    bool   right_dragging = false;
    double last_mx = 0, last_my = 0;
};

static void switch_orbital(AppState& app, int new_index) {
    if (new_index < 0) new_index = app.catalog.count - 1;
    if (new_index >= app.catalog.count) new_index = 0;
    app.orbital_index = new_index;

    const auto& orb = app.catalog.orbitals[new_index];
    float default_dist = orb.bounding_radius * 2.5f;
    app.camera.set_distance_target(default_dist, orb.bounding_radius);
}

// --- Cycle step counts -------------------------------------------------------

static int next_step_count(int current) {
    if (current <= 64)  return 128;
    if (current <= 128) return 256;
    return 64;
}

static int prev_step_count(int current) {
    if (current >= 256) return 128;
    if (current >= 128) return 64;
    return 256;
}

// --- GLFW callbacks ----------------------------------------------------------

static void key_callback(GLFWwindow* window, int key, int /*scancode*/,
                          int action, int mods) {
    if (action != GLFW_PRESS) return;
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));

    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    switch (key) {
    case GLFW_KEY_LEFT:
        switch_orbital(*app, app->orbital_index - 1);
        break;
    case GLFW_KEY_RIGHT:
        switch_orbital(*app, app->orbital_index + 1);
        break;
    case GLFW_KEY_UP:
        app->density_scale *= 1.5f;
        break;
    case GLFW_KEY_DOWN:
        app->density_scale /= 1.5f;
        if (app->density_scale < 0.01f) app->density_scale = 0.01f;
        break;
    case GLFW_KEY_B:
        if (shift)
            app->bloom_intensity = std::max(0.0f, app->bloom_intensity - 0.1f);
        else
            app->bloom_intensity = std::min(3.0f, app->bloom_intensity + 0.1f);
        break;
    case GLFW_KEY_S:
        if (shift)
            app->max_steps = prev_step_count(app->max_steps);
        else
            app->max_steps = next_step_count(app->max_steps);
        break;
    case GLFW_KEY_A:
        if (shift)
            app->anim_speed = std::max(0.125f, app->anim_speed * 0.5f);
        else
            app->anim_speed = std::min(8.0f, app->anim_speed * 2.0f);
        break;
    case GLFW_KEY_SPACE:
        app->paused = !app->paused;
        break;
    case GLFW_KEY_R:
        app->density_scale   = 1.0f;
        app->bloom_intensity = 0.5f;
        app->max_steps       = 128;
        app->anim_speed      = 1.0f;
        app->camera.target   = {0, 0, 0};
        switch_orbital(*app, app->orbital_index);
        break;
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        app->left_dragging = (action == GLFW_PRESS);
        if (action == GLFW_PRESS)
            glfwGetCursorPos(window, &app->last_mx, &app->last_my);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        app->right_dragging = (action == GLFW_PRESS);
        if (action == GLFW_PRESS)
            glfwGetCursorPos(window, &app->last_mx, &app->last_my);
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    double dx = xpos - app->last_mx;
    double dy = ypos - app->last_my;
    app->last_mx = xpos;
    app->last_my = ypos;

    if (app->left_dragging)
        app->camera.orbit(static_cast<float>(dx), static_cast<float>(dy));
    if (app->right_dragging)
        app->camera.pan(static_cast<float>(dx), static_cast<float>(dy));
}

static void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    app->camera.zoom(static_cast<float>(yoffset));
}

static void window_size_callback(GLFWwindow* window, int w, int h) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    app->win_width  = w;
    app->win_height = h;
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
                                          "Electron Orbitals", nullptr, nullptr);
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

    AppState app;
    app.catalog.build();
    app.renderer.init();

    // Set up initial orbital
    const auto& first_orb = app.catalog.orbitals[0];
    app.camera.distance    = first_orb.bounding_radius * 2.5f;
    app.camera.distance_to = app.camera.distance;
    app.camera.min_distance = first_orb.bounding_radius * 0.5f;
    app.camera.max_distance = first_orb.bounding_radius * 8.0f;

    glfwSetWindowUserPointer(window, &app);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);

    double prev_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float frame_dt = static_cast<float>(now - prev_time);
        prev_time = now;
        if (frame_dt > kMaxFrameDt) frame_dt = kMaxFrameDt;

        // Get framebuffer size
        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        if (fb_w < 1 || fb_h < 1) {
            glfwPollEvents();
            continue;
        }

        // Update animation
        if (!app.paused)
            app.anim_time += frame_dt * app.anim_speed;

        // Update camera interpolation
        app.camera.update(frame_dt);

        // Ensure FBOs match framebuffer size
        app.renderer.resize_fbos(fb_w, fb_h);

        // Build matrices
        float aspect = static_cast<float>(fb_w) / static_cast<float>(fb_h);
        mat4 view = app.camera.view_matrix();
        mat4 proj = app.camera.projection_matrix(aspect);
        mat4 vp   = proj * view;
        mat4 inv_vp = vp.inverse();

        const auto& orb = app.catalog.orbitals[app.orbital_index];

        // --- Pass 1: Ray march ---
        RaymarchUniforms ru{};
        ru.inv_view_proj  = inv_vp;
        ru.camera_pos     = app.camera.eye_position();
        ru.n              = orb.n;
        ru.l              = orb.l;
        ru.m              = orb.m;
        ru.radial_norm    = orb.radial_norm;
        ru.angular_norm   = orb.angular_norm;
        ru.bounding_radius = orb.bounding_radius;
        ru.density_scale  = app.density_scale;
        ru.max_steps      = app.max_steps;
        ru.time           = app.anim_time;
        ru.anim_speed     = app.anim_speed;

        app.renderer.draw_raymarch(ru);

        // --- Pass 2: Bloom ---
        app.renderer.draw_bloom();

        // --- Pass 3: Composite ---
        app.renderer.draw_composite(app.bloom_intensity);

        // --- Text overlay ---
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        int w = app.win_width;
        int h = app.win_height;
        float s = kTextScale;

        // Orbital label (top-left)
        app.renderer.draw_text(orb.full_label, 15.0f, 12.0f, s,
                               0.9f, 0.9f, 0.9f, w, h);

        // Parameter readout (bottom-left)
        {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "density: %.2f  bloom: %.1f  steps: %d",
                          app.density_scale, app.bloom_intensity, app.max_steps);
            app.renderer.draw_text(buf, 15.0f, static_cast<float>(h) - 55.0f, s,
                                   0.6f, 0.6f, 0.6f, w, h);
        }

        // Controls hint (bottom-center)
        {
            const char* hint = "SPACE: pause  <-/->: orbital  Up/Down: density  B: bloom  S: steps  R: reset";
            float tw = stb_easy_font_width(const_cast<char*>(hint)) * s;
            app.renderer.draw_text(hint, w * 0.5f - tw * 0.5f,
                                   static_cast<float>(h) - 28.0f, s,
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
