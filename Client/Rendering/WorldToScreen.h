#pragma once
#include "../SDK/Math.h"

namespace Client::Rendering {

struct ViewMatrix { float m[16]; };

// Projects a world-space point into 2D screen coordinates using the
// current camera view/projection matrix. Returns false if the point is
// behind the camera (so callers can skip drawing it).
bool worldToScreen(const SDK::Vec3& world, SDK::Vec2& outScreen,
                    const ViewMatrix& viewProj, int screenWidth, int screenHeight);

} // namespace Client::Rendering
