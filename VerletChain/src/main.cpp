#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "vec2.h"
#include "chain.h"
#include "renderer.h"
#include <cstdlib>
#include <cstdio>

// --- Simulation parameters ---
constexpr int   kNumParticles         = 20;
constexpr float kSegmentLength        = 25.0f;
constexpr Vec2  kGravity              = {0.0f, -980.0f};
constexpr int   kConstraintIterations = 8;
constexpr float kPickRadius           = 25.0f;
constexpr int   kInitialWidth         = 800;
constexpr int   kInitialHeight        = 600;
constexpr float kMaxDt                = 0.033f;

// --- Application state passed to GLFW callbacks ---
struct AppState {
    Chain*      chain         = nullptr;
    bool        dragging      = false;
    std::size_t dragged_index = 0;
    bool        was_pinned    = false;
    float       mouse_x       = 0.0f;
    float       mouse_y       = 0.0f;
    int         win_width     = kInitialWidth;
    int         win_height    = kInitialHeight;
};

// --- GLFW callbacks ---

static void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

static void window_size_callback(GLFWwindow* window, int width, int height) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    app->win_width = width;
    app->win_height = height;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        Vec2 mouse_pos = {app->mouse_x, app->mouse_y};
        std::size_t idx = app->chain->find_nearest(mouse_pos, kPickRadius);
        if (idx != Chain::npos) {
            app->dragging = true;
            app->dragged_index = idx;
            app->was_pinned = app->chain->is_pinned(idx);
            app->chain->set_pinned(idx, true);
            app->chain->set_particle_pos(idx, mouse_pos);
        }
    } else if (action == GLFW_RELEASE) {
        if (app->dragging) {
            if (!app->was_pinned) {
                app->chain->set_pinned(app->dragged_index, false);
            }
            app->dragging = false;
        }
    }
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    app->mouse_x = static_cast<float>(xpos);
    app->mouse_y = static_cast<float>(app->win_height) - static_cast<float>(ypos);

    if (app->dragging) {
        app->chain->set_particle_pos(app->dragged_index,
                                     {app->mouse_x, app->mouse_y});
    }
}

// --- Entry point ---

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(kInitialWidth, kInitialHeight,
                                          "Verlet Chain", nullptr, nullptr);
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
    std::printf("OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

    glfwSwapInterval(1);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Simulation
    Vec2 anchor = {kInitialWidth / 2.0f, kInitialHeight * 0.85f};
    Chain chain(anchor, kNumParticles, kSegmentLength);

    // Renderer
    ChainRenderer renderer;
    renderer.init(kNumParticles);

    // Input
    AppState app_state;
    app_state.chain = &chain;
    glfwSetWindowUserPointer(window, &app_state);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Timing
    double prev_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - prev_time);
        prev_time = now;
        if (dt > kMaxDt) dt = kMaxDt;

        chain.update(dt, kGravity, kConstraintIterations);

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.draw(chain.positions(), app_state.win_width, app_state.win_height);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
