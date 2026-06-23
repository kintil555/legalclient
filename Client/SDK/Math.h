#pragma once
#include <cmath>

namespace Client::SDK {

struct Vec2 { float x = 0, y = 0; };

struct Vec3 {
    float x = 0, y = 0, z = 0;

    [[nodiscard]] float distanceTo(const Vec3& other) const {
        const float dx = x - other.x;
        const float dy = y - other.y;
        const float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
};

struct AABB {
    Vec3 min;
    Vec3 max;

    [[nodiscard]] bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }
};

} // namespace Client::SDK
