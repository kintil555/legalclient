#pragma once
#include "Module.h"
#include "../SDK/LocalPlayer.h"

namespace Client::Modules {

// Simulates holding the "sprint" key whenever the player is moving forward
// and has enough hunger to sprint. This is a pure QoL/accessibility module:
// it does not increase movement speed beyond what vanilla sprint already
// allows, it just removes the need to hold/double-tap the key manually.
class AutoSprint : public Module {
public:
    AutoSprint() : Module("Auto Sprint", Category::Utility, false) {}

    void onTick() override {
        auto* player = SDK::LocalPlayer::get();
        if (!player) return;

        const bool wantsToMove = player->isMovingForward();
        const bool canSprint = player->hungerLevel() > 6 && !player->isSneaking();

        // Setting the input flag is equivalent to the player holding the
        // vanilla sprint key themselves - no speed bonus is added here.
        player->setSprintKeyHeld(wantsToMove && canSprint);
    }

    void onDisable() override {
        if (auto* player = SDK::LocalPlayer::get()) {
            player->setSprintKeyHeld(false);
        }
    }
};

} // namespace Client::Modules
