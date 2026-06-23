#pragma once
#include "Module.h"
#include "../Events/EventBus.h"
#include "../SDK/LocalPlayer.h"

namespace Client::Modules {

// Accessibility module: triggers a single jump input when the local player
// takes damage from an attack. This is the same action a player could do
// manually by reacting to the hit; it does not change jump height, does
// not chain multiple jumps, and is on a cooldown so it can never be used
// to bunny-hop or gain extra mobility. It exists purely to help players
// with slower reaction time avoid follow-up hits/knockback combos.
class AutoJumpOnDamage : public Module {
public:
    AutoJumpOnDamage() : Module("Auto Jump on Damage", Category::Utility, false) {}

    void onEnable() override {
        m_subscription = Events::EventBus::get().damage.subscribe(
            [this](const Events::DamageEvent& evt) { handleDamage(evt); });
    }

    void onTick() override {
        if (m_cooldownTicks > 0) --m_cooldownTicks;
    }

    void onConfigSave(nlohmann::json& out) const override {
        Module::onConfigSave(out);
        out["cooldownTicks"] = m_cooldownCfg;
    }

    void onConfigLoad(const nlohmann::json& in) override {
        Module::onConfigLoad(in);
        if (in.contains("cooldownTicks")) m_cooldownCfg = in.at("cooldownTicks").get<int>();
    }

private:
    void handleDamage(const Events::DamageEvent& evt) {
        // Only react to attacks/projectiles - jumping after fall or fire
        // damage would not help and could look like an exploit attempt.
        const bool relevantCause =
            evt.cause == Events::DamageCause::EntityAttack ||
            evt.cause == Events::DamageCause::Projectile;

        if (!relevantCause || m_cooldownTicks > 0) return;

        auto* player = SDK::LocalPlayer::get();
        if (!player || !player->isOnGround()) return; // can't jump mid-air anyway

        player->triggerJump();           // single vanilla jump input
        m_cooldownTicks = m_cooldownCfg; // prevents jump-spam / bhop abuse
    }

    int m_cooldownTicks = 0;
    int m_cooldownCfg = 10; // ticks (~0.5s at 20 tps)
    Events::EventChannel<Events::DamageEvent>::Handler m_subscription;
};

} // namespace Client::Modules
