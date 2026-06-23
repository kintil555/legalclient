#include "WorldToScreen.h"

namespace Client::Rendering {

bool worldToScreen(const SDK::Vec3& world, SDK::Vec2& outScreen,
                    const ViewMatrix& vp, int screenWidth, int screenHeight) {
    const float clipX = world.x * vp.m[0] + world.y * vp.m[4] + world.z * vp.m[8]  + vp.m[12];
    const float clipY = world.x * vp.m[1] + world.y * vp.m[5] + world.z * vp.m[9]  + vp.m[13];
    const float clipW = world.x * vp.m[3] + world.y * vp.m[7] + world.z * vp.m[11] + vp.m[15];

    if (clipW < 0.01f) return false; // behind camera

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;

    outScreen.x = (screenWidth / 2.0f) * (1.0f + ndcX);
    outScreen.y = (screenHeight / 2.0f) * (1.0f - ndcY);
    return true;
}

} // namespace Client::Rendering
