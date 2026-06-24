#include <Windows.h>
#include "Core/Client.h"
#include "Hooks/D3DHook.h"

// ============================================================================
// DllMain.cpp — DLL entry point untuk Bedrock Utility Client
// ============================================================================
// Catatan GDK (v1.21.120+, termasuk 26.31):
//   - Minecraft GDK berjalan sebagai Win32 app biasa (Minecraft.Windows.exe),
//     BUKAN UWP. Window class-nya adalah "Bedrock" (bukan "Windows.UI.Core.CoreWindow"
//     dan bukan "GLFW30"). Referensi: Flarial OSS WindowManager.cpp.
//   - D3D11 device-nya dibuat oleh game setelah rendering thread aktif.
//     Grace period 600ms sering tidak cukup, terutama di PC lambat.
//   - Kita harus nunggu sampai D3D11 benar-benar siap sebelum buat dummy
//     SwapChain, karena D3D11CreateDeviceAndSwapChain butuh HWND yang valid
//     dan sudah dipunya game.

// Cari HWND game yang valid (milik proses ini, visible, punya client area).
static HWND waitForGameWindow(DWORD timeoutMs) {
    const DWORD startTime = GetTickCount();
    while (GetTickCount() - startTime < timeoutMs) {
        // GDK Minecraft (1.21.120+, 26.31): window class = "Bedrock"
        // Sumber: Flarial OSS (WindowManager.cpp) menggunakan FindWindowExW dengan L"Bedrock"
        // Fallback: iterasi semua top-level window milik proses ini.
        HWND hwnd = FindWindowExW(nullptr, nullptr, L"Bedrock", nullptr);
        if (!hwnd) {
            // Fallback: cari window yang owned by proses kita, visible,
            // dan punya area client yang non-zero.
            struct Ctx { DWORD pid; HWND result; };
            Ctx ctx{ GetCurrentProcessId(), nullptr };
            EnumWindows([](HWND h, LPARAM lp) -> BOOL {
                auto* c = reinterpret_cast<Ctx*>(lp);
                DWORD pid = 0;
                GetWindowThreadProcessId(h, &pid);
                if (pid != c->pid || !IsWindowVisible(h)) return TRUE;
                RECT r{}; GetClientRect(h, &r);
                if ((r.right - r.left) > 100 && (r.bottom - r.top) > 100) {
                    c->result = h;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&ctx));
            hwnd = ctx.result;
        }
        if (hwnd) {
            // Pastikan window sudah punya client area (sudah dirender oleh game)
            RECT r{};
            GetClientRect(hwnd, &r);
            if (r.right > 0 && r.bottom > 0) return hwnd;
        }
        Sleep(150);
    }
    return nullptr;
}

// Worker thread: install semua hooks setelah game window & D3D siap.
// Dijalankan di thread terpisah agar tidak deadlock DllMain loader lock.
static DWORD WINAPI clientThread(LPVOID) {
    // Tunggu game window valid (timeout 30 detik untuk PC lambat)
    HWND gameHwnd = waitForGameWindow(30000);
    if (!gameHwnd) return 1; // game tidak ketemu, abort

    // Tambahan delay: tunggu D3D11 device game selesai diinisialisasi.
    // GDK butuh lebih lama dari UWP karena ada proses GDK Xbox auth di background.
    Sleep(1500);

    // Setup modules, EventBus subscribers, InputHook (WndProc)
    Client::Core::initializeClient();

    // Install D3D11 Present hook dengan retry (penting untuk GDK!)
    // D3D11CreateDeviceAndSwapChain bisa gagal kalau D3D device game belum siap.
    bool hookInstalled = false;
    for (int attempt = 0; attempt < 10 && !hookInstalled; ++attempt) {
        hookInstalled = Client::Hooks::D3DHook::get().install();
        if (!hookInstalled) Sleep(500); // tunggu 0.5s sebelum retry
    }

    return hookInstalled ? 0 : 1;
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
