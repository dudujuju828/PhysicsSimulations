#pragma once
#include "vec2.h"
#include <algorithm>

struct AABB {
    Vec2 min{};
    Vec2 max{};

    constexpr float width() const { return max.x - min.x; }
    constexpr float height() const { return max.y - min.y; }
    constexpr Vec2 center() const { return (min + max) * 0.5f; }
    constexpr float area() const { return width() * height(); }

    constexpr bool overlaps(const AABB& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y;
    }

    constexpr bool contains(Vec2 p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y;
    }

    constexpr AABB merged(const AABB& o) const {
        return {Vec2::min(min, o.min), Vec2::max(max, o.max)};
    }

    // Expand by a margin on all sides
    constexpr AABB expanded(float margin) const {
        return {{min.x - margin, min.y - margin},
                {max.x + margin, max.y + margin}};
    }
};
