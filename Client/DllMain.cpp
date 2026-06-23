#include <Windows.h>
#include "Core/Client.h"
#include "Hooks/D3DHook.h"

// ============================================================================
// DllMain.cpp — DLL entry point untuk Bedrock Utility Client
// ============================================================================
// INI ADALAH ROOT CAUSE SEMUANYA TIDAK MUNCUL:
//
//   - Client.cpp punya initializeClient() / renderClient() / shutdownClient()
//   - D3DHook.h punya Present hook untuk ImGui rendering
//   - InputHook.h punya WndProc hook untuk key events
//
//   TAPI tidak ada DllMain yang mengaktifkan semuanya!
//   DLL di-inject, lalu tidak melakukan APA-APA.
//
// Fix:
//   DLL_PROCESS_ATTACH -> spawn thread -> initializeClient() -> D3DHook::install()
//   Every frame        -> D3DHook calls clientRenderFrame() -> renderClient()
//   DLL_PROCESS_DETACH -> D3DHook::uninstall() -> shutdownClient()
// ============================================================================

// Dipanggil oleh D3DHook setiap frame setelah ImGui::NewFrame().
// Fungsi ini mendrive semua rendering: menu, HUD modules, InjectionNotice.
void clientRenderFrame() {
    Client::Core::renderClient();
}

// Worker thread: install semua hooks setelah game window siap.
// Dijalankan di thread terpisah agar tidak deadlock DllMain loader lock.
static DWORD WINAPI clientThread(LPVOID) {
    // Tunggu sampai game window ada
    while (!GetForegroundWindow()) {
        Sleep(100);
    }
    Sleep(600); // grace period: pastikan D3D11 device game sudah ready

    // Setup modules, EventBus subscribers, InputHook (WndProc)
    Client::Core::initializeClient();

    // Install D3D11 Present hook -> akan memanggil ImGui + clientRenderFrame() tiap frame
    Hooks::D3DHook::get().install();

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstance);
            CloseHandle(CreateThread(nullptr, 0, clientThread, nullptr, 0, nullptr));
            break;

        case DLL_PROCESS_DETACH:
            // Urutan shutdown penting: hapus render hook dulu, baru cleanup lainnya
            Hooks::D3DHook::get().uninstall();   // removes Present hook, destroys ImGui
            Client::Core::shutdownClient();       // uninstalls InputHook + saves config
            break;
    }
    return TRUE;
}
