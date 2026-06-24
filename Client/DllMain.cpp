#include <Windows.h>
#include "Core/Client.h"
#include "Hooks/D3DHook.h"

// ============================================================================
// DllMain.cpp — DLL entry point untuk Bedrock Utility Client (GDK 26.31)
// ============================================================================
//
// Strategy:
//   1. Tunggu d3d11.dll ter-load ke game (poll GetModuleHandleA).
//      Ini tanda bahwa D3D sudah diinisialisasi game, aman untuk hook.
//   2. Tidak perlu HWND game — D3DHook buat dummy window sendiri (kiero-style).
//   3. Retry loop 20x jika D3DHook::install() gagal (jaga-jaga race condition).

static bool waitForD3D11(DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        if (GetModuleHandleA("d3d11.dll") != nullptr)
            return true;
        Sleep(100);
    }
    return false;
}

static DWORD WINAPI clientThread(LPVOID) {
    // Tunggu d3d11.dll muncul di proses — artinya game sudah init renderer.
    // Timeout 60 detik untuk PC paling lambat sekalipun.
    if (!waitForD3D11(60000))
        return 1;

    // Sedikit grace period setelah D3D11 load agar render thread game siap.
    Sleep(800);

    // Setup modules + InputHook
    Client::Core::initializeClient();

    // Install Present hook. D3DHook buat dummy window sendiri (tidak butuh
    // HWND game), sehingga tidak ada dependency pada Bedrock window class.
    // Retry 20x dengan interval 500ms jika masih gagal.
    bool ok = false;
    for (int i = 0; i < 20 && !ok; ++i) {
        ok = Client::Hooks::D3DHook::get().install();
        if (!ok) Sleep(500);
    }

    return ok ? 0 : 1;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstance);
            CloseHandle(CreateThread(nullptr, 0, clientThread, nullptr, 0, nullptr));
            break;

        case DLL_PROCESS_DETACH:
            Client::Hooks::D3DHook::get().uninstall();
            Client::Core::shutdownClient();
            break;
    }
    return TRUE;
}
