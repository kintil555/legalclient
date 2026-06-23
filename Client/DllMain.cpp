#include <Windows.h>
#include "Core/Client.h"
#include "Hooks/D3DHook.h"

// ============================================================================
// DllMain.cpp — DLL entry point untuk Bedrock Utility Client
// ============================================================================

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

    // Install D3D11 Present hook -> ImGui + renderClient() tiap frame
    Client::Hooks::D3DHook::get().install();

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstance);
            CloseHandle(CreateThread(nullptr, 0, clientThread, nullptr, 0, nullptr));
            break;

        case DLL_PROCESS_DETACH:
            Client::Hooks::D3DHook::get().uninstall();  // removes Present hook, destroys ImGui
            Client::Core::shutdownClient();              // uninstalls InputHook + saves config
            break;
    }
    return TRUE;
}
