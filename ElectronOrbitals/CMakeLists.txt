cmake_minimum_required(VERSION 3.20)
project(ElectronOrbitals LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- GLFW (windowing / input) ---
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# --- GLAD (pre-generated OpenGL 4.6 core loader) ---
add_library(glad STATIC glad_gen/src/gl.c)
target_include_directories(glad PUBLIC glad_gen/include)

# --- Executable ---
add_executable(ElectronOrbitals
    src/main.cpp
    src/renderer.cpp
)
target_link_libraries(ElectronOrbitals PRIVATE glfw glad)
