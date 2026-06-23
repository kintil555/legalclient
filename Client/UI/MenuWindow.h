#pragma once
#include <imgui.h>
#include <Windows.h>
#include "../Core/ModuleManager.h"
#include "../Settings/ConfigManager.h"

namespace Client::UI {

// Main client menu. Visibility is toggled by tapping Right SHIFT - this is
// just a UI show/hide key, it has no gameplay effect on its own.
class MenuWindow {
public:
    static MenuWindow& get() {
        static MenuWindow instance;
        return instance;
    }

    // Call this from the WndProc/raw-input hook for every key event.
    void onKeyEvent(int vKey, bool isDown) {
        if (vKey == VK_RSHIFT && isDown && !m_rshiftWasDown) {
            m_visible = !m_visible;
        }
        m_rshiftWasDown = (vKey == VK_RSHIFT) ? isDown : m_rshiftWasDown;
    }

    void render() {
        if (!m_visible) return;

        ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_FirstUseEver);
        ImGui::Begin("Client Menu", &m_visible);

        ImGui::TextDisabled("Toggle this menu anytime with Right Shift");
        ImGui::Separator();

        if (ImGui::BeginTabBar("##categories")) {
            drawCategoryTab("HUD", Modules::Category::HUD);
            drawCategoryTab("Visual", Modules::Category::Visual);
            drawCategoryTab("Utility", Modules::Category::Utility);
            drawCategoryTab("Performance", Modules::Category::Performance);
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Save Config")) {
            Settings::ConfigManager::get().save("default.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Config")) {
            Settings::ConfigManager::get().load("default.json");
        }

        ImGui::End();
    }

    [[nodiscard]] bool isVisible() const { return m_visible; }

private:
    void drawCategoryTab(const char* label, Modules::Category category) {
        if (!ImGui::BeginTabItem(label)) return;

        for (auto& mod : Core::ModuleManager::get().modules()) {
            if (mod->category() != category) continue;

            bool enabled = mod->isEnabled();
            if (ImGui::Checkbox(mod->name().c_str(), &enabled)) {
                mod->setEnabled(enabled);
            }
        }

        ImGui::EndTabItem();
    }

    bool m_visible = false;
    bool m_rshiftWasDown = false;
};

} // namespace Client::UI
