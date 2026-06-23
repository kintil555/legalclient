#pragma once
#include <functional>
#include <vector>
#include <cstdint>

// Lightweight, header-only event bus.
// Modules subscribe to the events they care about; the bus stays decoupled
// from any specific module implementation (observer pattern).

namespace Client::Events {

struct TickEvent {};

struct RenderEvent {
    float deltaTime;
};

struct KeyEvent {
    int virtualKey;
    bool isDown;
};

// Fired by the damage-detection hook whenever the local player takes damage.
// 'cause' helps the AutoJumpOnDamage module decide whether jumping is sensible
// (e.g. skip if cause is fall damage, since jumping won't help there).
enum class DamageCause {
    Unknown,
    EntityAttack,
    Fall,
    Fire,
    Drown,
    Projectile
};

struct DamageEvent {
    float amount;
    DamageCause cause;
};

template <typename EventT>
class EventChannel {
public:
    using Handler = std::function<void(const EventT&)>;

    // Returns the handler itself so callers can keep a reference if they
    // need one (current modules don't need to unsubscribe, but this keeps
    // the API forward-compatible without breaking signatures later).
    Handler subscribe(Handler handler) {
        m_handlers.push_back(handler);
        return handler;
    }

    void dispatch(const EventT& evt) const {
        for (const auto& h : m_handlers) h(evt);
    }

private:
    std::vector<Handler> m_handlers;
};

// Central bus holding one channel per event type used by the client.
class EventBus {
public:
    static EventBus& get() {
        static EventBus instance;
        return instance;
    }

    EventChannel<TickEvent> tick;
    EventChannel<RenderEvent> render;
    EventChannel<KeyEvent> key;
    EventChannel<DamageEvent> damage;
};

} // namespace Client::Events
