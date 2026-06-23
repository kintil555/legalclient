#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace Client::Modules {

enum class Category {
    HUD,
    Visual,
    Utility,
    Performance
};

// Base class for every feature module in the client.
// Each module is self-contained, config-aware, and event-driven.
class Module {
public:
    Module(std::string name, Category category, bool enabledByDefault = false)
        : m_name(std::move(name)),
          m_category(category),
          m_enabled(enabledByDefault) {}

    virtual ~Module() = default;

    // Lifecycle hooks -------------------------------------------------
    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onTick() {}          // Called once per game tick (~20/s)
    virtual void onRender() {}        // Called once per frame (ImGui overlay)
    virtual void onKey(int vKey, bool isDown) { (void)vKey; (void)isDown; }

    // Config ------------------------------------------------------------
    virtual void onConfigSave(nlohmann::json& out) const { out["enabled"] = m_enabled; }
    virtual void onConfigLoad(const nlohmann::json& in) {
        if (in.contains("enabled")) m_enabled = in.at("enabled").get<bool>();
    }

    // Toggle helpers ------------------------------------------------------
    void setEnabled(bool value) {
        if (value == m_enabled) return;
        m_enabled = value;
        if (m_enabled) onEnable(); else onDisable();
    }
    void toggle() { setEnabled(!m_enabled); }

    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] const std::string& name() const { return m_name; }
    [[nodiscard]] Category category() const { return m_category; }

protected:
    std::string m_name;
    Category m_category;
    bool m_enabled;
};

} // namespace Client::Modules
