#pragma once
#include "Math.h"

namespace Client::SDK {

// Thin wrapper around the game's internal LocalPlayer structure.
// All offsets are placeholders - real values must be obtained per-version
// via reverse engineering / a maintained offset dump (e.g. through Dumper7
// or a community offset list) and kept in Offsets.h. Hardcoding wrong
// offsets here would crash the client, not "cheat", so accuracy matters
// far more than for cosmetic modules.
class LocalPlayer {
public:
    static LocalPlayer* get(); // resolved via game's player pointer chain

    [[nodiscard]] Vec3 position() const;
    [[nodiscard]] Vec3 viewAngles() const;
    [[nodiscard]] bool isMovingForward() const;
    [[nodiscard]] bool isSneaking() const;
    [[nodiscard]] bool isOnGround() const;
    [[nodiscard]] float hungerLevel() const;

    // Input simulation - these set the same input flags the game reads
    // from keyboard/controller state each tick. They do not bypass any
    // game logic (hunger checks, sneak-cancel, etc. still apply normally).
    void setSprintKeyHeld(bool held);
    void triggerJump();

private:
    void* m_instancePtr = nullptr;
};

} // namespace Client::SDK
