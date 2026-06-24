#pragma once
#include "Module.h"
#include <imgui.h>
#include <chrono>

namespace Client::Modules {

// Pure debug/dev-feedback overlay: shows a small "Client has successfully
// injected!" text in the top-left corner for a few seconds after the DLL
// finishes initializing, then fades out on its own. No gameplay effect -
// this exists purely so you can visually confirm the inject worked.
//
// Catatan GDK: overlay ini sangat penting untuk debug inject karena
// di GDK tidak ada "have fun" message dari injector — kita lihat sendiri
// lewat overlay ini apakah D3DHook berhasil.
class InjectionNotice : public Module {
public:
    InjectionNotice() : Module("Injection Notice", Category::HUD, true) {
        m_shownAt = std::chrono::steady_clock::now();
    }

    // Intentionally not persisted: this should always show on every fresh
    // inject regardless of what an old config file says.
    void onConfigSave(nlohmann::json&) const override {}
    void onConfigLoad(const nlohmann::json&) override {}

    void onRender() override {
        const auto elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - m_shownAt).count();

        constexpr float kVisibleSeconds = 5.0f;  // sedikit lebih lama agar lebih mudah dibaca
        constexpr float kFadeSeconds    = 1.0f;
        if (elapsed > kVisibleSeconds + kFadeSeconds) {
            setEnabled(false);
            return;
        }

        const float alpha = elapsed <= kVisibleSeconds
            ? 1.0f
            : 1.0f - ((elapsed - kVisibleSeconds) / kFadeSeconds);

        // Posisi: kiri atas, sedikit padding
        ImGui::SetNextWindowPos(ImVec2(12, 12));
        ImGui::SetNextWindowBgAlpha(0.55f); // sedikit background agar mudah dibaca di semua map
        ImGui::Begin("##injection_notice", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoInputs   |
            ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.95f, 0.4f, alpha));
        ImGui::Text("LegalClient injected! (GDK)");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, alpha * 0.75f));
        ImGui::Text("Press L to open menu");
        ImGui::PopStyleColor();

        ImGui::End();
    }

private:
    std::chrono::steady_clock::time_point m_shownAt;
};

} // namespace Client::Modules
