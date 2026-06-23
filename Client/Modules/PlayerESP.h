#pragma once
#include "Module.h"
#include "../SDK/EntityList.h"
#include "../SDK/LocalPlayer.h"
#include "../Rendering/WorldToScreen.h"
#include <imgui.h>

namespace Client::Modules {

// Pure visibility ESP: draws an outline box around other players that are
// in view. This module intentionally does NOT compute or display anything
// related to attack range, hit probability, or combat timing - it only
// answers "is this player visible/where is it", which is informational,
// not a competitive advantage. Box color is a single static accent color
// (no "can-hit" state), to avoid any ambiguity with reach/hit indicators.
class PlayerESP : public Module {
public:
    PlayerESP() : Module("Player ESP", Category::Visual, false) {}

    void onConfigSave(nlohmann::json& out) const override {
        Module::onConfigSave(out);
        out["showNames"] = m_showNames;
        out["showDistance"] = m_showDistance;
    }

    void onConfigLoad(const nlohmann::json& in) override {
        Module::onConfigLoad(in);
        if (in.contains("showNames")) m_showNames = in.at("showNames").get<bool>();
        if (in.contains("showDistance")) m_showDistance = in.at("showDistance").get<bool>();
    }

    void onRender() override {
        auto* local = SDK::LocalPlayer::get();
        if (!local) return;

        const auto players = SDK::EntityList::getPlayers();
        const Rendering::ViewMatrix vp = getCurrentViewProjection();

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        const ImU32 boxColor = ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 0.9f));

        for (const auto& entity : players) {
            SDK::Vec2 top, bottom;
            const SDK::Vec3 topWorld = { entity.boundingBox.min.x, entity.boundingBox.max.y, entity.boundingBox.min.z };
            const SDK::Vec3 botWorld = { entity.boundingBox.max.x, entity.boundingBox.min.y, entity.boundingBox.max.z };

            const bool ok1 = Rendering::worldToScreen(topWorld, top, vp, m_screenW, m_screenH);
            const bool ok2 = Rendering::worldToScreen(botWorld, bottom, vp, m_screenW, m_screenH);
            if (!ok1 || !ok2) continue; // off-screen or behind camera

            draw->AddRect(ImVec2(top.x, top.y), ImVec2(bottom.x, bottom.y), boxColor, 2.0f, 0, 1.5f);

            if (m_showDistance) {
                const float dist = local->position().distanceTo(entity.position);
                char label[32];
                snprintf(label, sizeof(label), "%.1fm", dist);
                draw->AddText(ImVec2(top.x, top.y - 14), boxColor, label);
            }
            if (m_showNames && !entity.name.empty()) {
                draw->AddText(ImVec2(top.x, top.y - (m_showDistance ? 28 : 14)), boxColor, entity.name.c_str());
            }
        }
    }

private:
    // Supplied by the rendering hook each frame in a real build (read from
    // the game's camera/view matrix via the D3D11 hook). Stubbed here as
    // an identity-ish matrix so the project links and renders nothing
    // meaningful until that hook is wired up - replace this with the real
    // matrix capture before using the client in an actual game session.
    static Rendering::ViewMatrix getCurrentViewProjection() {
        Rendering::ViewMatrix vp{};
        vp.m[0] = vp.m[5] = vp.m[10] = vp.m[15] = 1.0f; // identity matrix
        return vp;
    }

    bool m_showNames = true;
    bool m_showDistance = true;
    int m_screenW = 1920;
    int m_screenH = 1080;
};

} // namespace Client::Modules
