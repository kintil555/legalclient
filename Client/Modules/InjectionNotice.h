#pragma once
#include "Module.h"
#include <imgui.h>
#include <chrono>

namespace Client::Modules {

// Overlay kecil yang muncul beberapa detik setelah DLL berhasil inject.
// Timer dimulai saat PERTAMA KALI onRender() dipanggil (bukan di constructor)
// supaya tidak habis sebelum D3DHook aktif.
class InjectionNotice : public Module {
public:
    InjectionNotice() : Module("Injection Notice", Category::HUD, true) {}

    // Tidak disimpan ke config - selalu muncul setiap inject baru
    void onConfigSave(nlohmann::json&) const override {}
    void onConfigLoad(const nlohmann::json&) override {}

    void onRender() override {
        // Mulai timer saat render pertama kali dipanggil (D3DHook sudah aktif)
        if (!m_timerStarted) {
            m_shownAt     = std::chrono::steady_clock::now();
            m_timerStarted = true;
        }

        const float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - m_shownAt).count();

        constexpr float kVisible = 6.0f;
        constexpr float kFade    = 1.0f;

        if (elapsed > kVisible + kFade) {
            setEnabled(false);
            return;
        }

        const float alpha = (elapsed <= kVisible)
            ? 1.0f
            : 1.0f - ((elapsed - kVisible) / kFade);

        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f));
        ImGui::SetNextWindowBgAlpha(0.60f);
        ImGui::Begin("##inj_notice", nullptr,
            ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize   |
            ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse    | ImGuiWindowFlags_NoInputs   |
            ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.95f, 0.4f, alpha));
        ImGui::Text("LegalClient injected! (GDK 26.31)");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, alpha * 0.85f));
        ImGui::Text("Press L to open menu");
        ImGui::PopStyleColor();

        ImGui::End();
    }

private:
    bool m_timerStarted = false;
    std::chrono::steady_clock::time_point m_shownAt;
};

} // namespace Client::Modules
