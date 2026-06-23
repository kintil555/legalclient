#pragma once
#include <imgui.h>
#include <Windows.h>
#include "../Core/ModuleManager.h"
#include "../Settings/ConfigManager.h"

namespace Client::UI {

// Main client menu.  Visibility is toggled by tapping the L key.
// This is a UI-only toggle with zero gameplay effect.
//
// Why L?  Right Shift was the original design but the key event pipeline
// was never wired up (InputHook was missing), so the menu was permanently
// hidden.  L is a comfortable, single-handed key that does not conflict
// with any vanilla Bedrock keybind.  Change kMenuToggleKey below to
// remap it to any other virtual-key code.
class MenuWindow {
public:
    static MenuWindow& get() {
        static MenuWindow instance;
        return instance;
    }

    // Virtual-key code used to show/hide the menu.
    //   'L'        = 0x4C  (current default)
    //   VK_RSHIFT  = 0xA1  (original broken binding)
    //   VK_INSERT  = 0x2D
    //   VK_HOME    = 0x24
    static constexpr int kMenuToggleKey = 'L';

    // Called for every key event forwarded by InputHook.
    // Edge-triggered: fires only on the first key-down transition so
    // holding L does not rapidly flicker the menu open/closed.
    void onKeyEvent(int vKey, bool isDown) {
        const bool isToggle = (vKey == kMenuToggleKey);

        // Toggle on the falling edge of key-down (fresh press only).
        if (isToggle && isDown && !m_toggleKeyWasDown) {
            m_visible = !m_visible;
        }

        if (isToggle) m_toggleKeyWasDown = isDown;
    }

    void render() {
        if (!m_visible) return;

        ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_FirstUseEver);
        ImGui::Begin("Client Menu", &m_visible);

        ImGui::TextDisabled("Press L to toggle this menu");
        ImGui::Separator();

        if (ImGui::BeginTabBar("##categories")) {
            drawCategoryTab("HUD",         Modules::Category::HUD);
            drawCategoryTab("Visual",      Modules::Category::Visual);
            drawCategoryTab("Utility",     Modules::Category::Utility);
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

    bool m_visible          = false;
    bool m_toggleKeyWasDown = false;  // tracks held state for edge detection
};

} // namespace Client::UI
