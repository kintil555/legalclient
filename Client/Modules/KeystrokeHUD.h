#pragma once
#include "Module.h"
#include <array>
#include <imgui.h>
#include <Windows.h> // VK_SPACE, VK_LBUTTON, VK_RBUTTON

namespace Client::Modules {

// Classic "keystrokes" overlay used by streamers: shows which movement
// keys + mouse buttons are currently pressed. Purely cosmetic/HUD, no
// gameplay effect whatsoever.
class KeystrokeHUD : public Module {
public:
    KeystrokeHUD() : Module("Keystrokes", Category::HUD, false) {}

    void onKey(int vKey, bool isDown) override {
        switch (vKey) {
            case 'W': m_w = isDown; break;
            case 'A': m_a = isDown; break;
            case 'S': m_s = isDown; break;
            case 'D': m_d = isDown; break;
            case VK_SPACE: m_space = isDown; break;
            case VK_LBUTTON: m_lmb = isDown; break;
            case VK_RBUTTON: m_rmb = isDown; break;
            default: break;
        }
    }

    void onRender() override {
        ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("##keystrokes", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

        drawKey("W", m_w, 50, 0);
        drawKey("A", m_a, 0, 35);
        drawKey("S", m_s, 50, 35);
        drawKey("D", m_d, 100, 35);
        drawKey("LMB", m_lmb, 0, 80);
        drawKey("RMB", m_rmb, 80, 80);

        ImGui::End();
    }

private:
    void drawKey(const char* label, bool pressed, float xOff, float yOff) {
        ImGui::SetCursorPos(ImVec2(xOff, yOff));
        ImVec4 color = pressed ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f)
                                : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::Button(label, ImVec2(40, 30));
        ImGui::PopStyleColor();
    }

    bool m_w = false, m_a = false, m_s = false, m_d = false;
    bool m_space = false, m_lmb = false, m_rmb = false;
};

} // namespace Client::Modules
