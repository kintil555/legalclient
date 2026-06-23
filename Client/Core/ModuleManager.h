#pragma once
#include <memory>
#include <vector>
#include <algorithm>
#include "../Modules/Module.h"
#include "../Events/EventBus.h"

namespace Client::Core {

// Owns every module instance, wires them into the EventBus, and exposes
// lookup helpers for the UI layer (menu needs to list + toggle modules).
class ModuleManager {
public:
    static ModuleManager& get() {
        static ModuleManager instance;
        return instance;
    }

    template <typename ModuleT, typename... Args>
    ModuleT* registerModule(Args&&... args) {
        auto mod = std::make_unique<ModuleT>(std::forward<Args>(args)...);
        ModuleT* raw = mod.get();
        m_modules.push_back(std::move(mod));
        return raw;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<Modules::Module>>& modules() const {
        return m_modules;
    }

    Modules::Module* findByName(const std::string& name) {
        auto it = std::find_if(m_modules.begin(), m_modules.end(),
            [&](const auto& m) { return m->name() == name; });
        return it != m_modules.end() ? it->get() : nullptr;
    }

    // Called once during client init, after all modules are registered.
    void bindEvents() {
        Events::EventBus::get().tick.subscribe([this](const Events::TickEvent&) {
            for (auto& m : m_modules) if (m->isEnabled()) m->onTick();
        });

        Events::EventBus::get().render.subscribe([this](const Events::RenderEvent&) {
            for (auto& m : m_modules) if (m->isEnabled()) m->onRender();
        });

        Events::EventBus::get().key.subscribe([this](const Events::KeyEvent& evt) {
            for (auto& m : m_modules) m->onKey(evt.virtualKey, evt.isDown);
        });
    }

private:
    std::vector<std::unique_ptr<Modules::Module>> m_modules;
};

} // namespace Client::Core
