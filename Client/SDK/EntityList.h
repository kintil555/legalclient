#pragma once
#include "Math.h"
#include <vector>
#include <string>

namespace Client::SDK {

struct Entity {
    void* instancePtr = nullptr;
    std::string name;
    Vec3 position;
    AABB boundingBox;
    bool isPlayer = false;
};

class EntityList {
public:
    static std::vector<Entity> getPlayers(); // excludes local player
};

} // namespace Client::SDK
