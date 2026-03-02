#pragma once

struct UIState {
    // Layer toggles (keys 1-5)
    bool show_aabb_overlay    = false;  // 1: AABB boxes around shapes
    bool show_bvh_tree        = false;  // 2: BVH depth-colored boxes + tree diagram
    bool show_query_vis       = false;  // 3: Traversal visualization on click
    bool show_brute_compare   = false;  // 4: Brute force vs BVH pair lines
    bool show_narrow_phase    = false;  // 5: Collision results coloring

    // Mode
    bool use_bvh              = true;
    bool paused               = false;
    bool step_mode            = false;  // When paused with selection, step through BVH

    // Sliders
    int   target_count        = 30;     // Object count (5-200)
    float speed_mult          = 1.0f;   // Speed multiplier (0-3)

    // Selection
    int   selected_shape      = -1;
    int   hovered_shape       = -1;
    int   dragged_shape       = -1;
    Vec2  drag_offset{};

    // Step-through state
    int   step_index          = -1;
    bool  build_anim_active   = false;
    int   build_anim_step     = 0;

    // Slider interaction
    int   active_slider       = -1;     // -1 = none, 0 = count, 1 = speed

    // Stats
    int   broad_phase_pairs   = 0;
    int   brute_force_pairs   = 0;
    int   narrow_phase_tests  = 0;
    int   actual_collisions   = 0;
    int   false_positives     = 0;
    int   bvh_node_count      = 0;
    float fps                 = 0.0f;
    bool  bvh_mismatch        = false;
};

struct Slider {
    float x, y, w, h;          // Position and size in pixels
    float min_val, max_val;
    float* value;
    const char* label;

    float handle_x() const {
        float t = (*value - min_val) / (max_val - min_val);
        return x + t * w;
    }

    bool contains(float px, float py) const {
        return px >= x - 5 && px <= x + w + 5 &&
               py >= y - 5 && py <= y + h + 5;
    }

    void drag_to(float px) {
        float t = (px - x) / w;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        *value = min_val + t * (max_val - min_val);
    }
};
