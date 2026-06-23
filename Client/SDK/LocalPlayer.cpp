#include "LocalPlayer.h"

namespace Client::SDK {

// ============================================================================
// STUB IMPLEMENTATION
// ----------------------------------------------------------------------------
// These bodies exist only so the project links into a runnable DLL during
// development/CI. They do NOT talk to the real game in any way - get()
// always returns a dummy instance, and every accessor returns a safe
// default. Before using this client in an actual game session, every
// function below must be replaced with real memory reads / hooked calls
// resolved from offsets specific to the Bedrock build you're targeting.
// ============================================================================

namespace {
    LocalPlayer g_dummyInstance;
}

LocalPlayer* LocalPlayer::get() {
    // TODO: resolve the real LocalPlayer pointer via the game's pointer
    // chain (e.g. ClientInstance -> LocalPlayer) once offsets are known.
    return &g_dummyInstance;
}

Vec3 LocalPlayer::position() const {
    return Vec3{};
}

Vec3 LocalPlayer::viewAngles() const {
    return Vec3{};
}

bool LocalPlayer::isMovingForward() const {
    return false;
}

bool LocalPlayer::isSneaking() const {
    return false;
}

bool LocalPlayer::isOnGround() const {
    return true;
}

float LocalPlayer::hungerLevel() const {
    return 20.0f; // vanilla max, so AutoSprint's hunger check never blocks in stub mode
}

void LocalPlayer::setSprintKeyHeld(bool held) {
    (void)held; // TODO: forward to the real input-state structure
}

void LocalPlayer::triggerJump() {
    // TODO: set the real jump input flag for one tick
}

} // namespace Client::SDK
