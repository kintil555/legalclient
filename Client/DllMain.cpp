#include <Windows.h>
#include <fstream>
#include <string>
#include <sstream>
#include "Core/Client.h"
#include "Hooks/D3DHook.h"
#include "Hooks/CommandQueueHook.h"

// ============================================================================
// DllMain.cpp
// Log: %APPDATA%\LegalClient\debug.log
// ============================================================================

namespace {

std::string getLogPath() {
    char appdata[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH) == 0)
        return "C:\\LegalClient_debug.log";
    return std::string(appdata) + "\\LegalClient\\debug.log";
}

void ensureDir(const std::string& path) {
    auto pos = path.rfind('\\');
    if (pos != std::string::npos)
        CreateDirectoryA(path.substr(0, pos).c_str(), nullptr);
}

void log(const std::string& msg) {
    static std::string logPath = getLogPath();
    static bool dirReady = false;
    if (!dirReady) { ensureDir(logPath); dirReady = true; }
    std::ofstream f(logPath, std::ios::app);
    if (!f) return;
    std::ostringstream ss;
    ss << "[+" << GetTickCount64() << "ms] " << msg << "\n";
    f << ss.str();
    f.flush();
}

bool waitForModule(const char* name, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        if (GetModuleHandleA(name)) return true;
        Sleep(100);
    }
    return false;
}

} // namespace

static DWORD WINAPI clientThread(LPVOID) {
    log("=== LegalClient DLL loaded ===");

    // Tunggu d3d11 atau d3d12 muncul (salah satu pasti ada)
    log("Menunggu DirectX module...");
    bool hasDX = false;
    for (int i = 0; i < 600 && !hasDX; ++i) { // max 60 detik
        hasDX = GetModuleHandleA("d3d11.dll") || GetModuleHandleA("d3d12.dll");
        if (!hasDX) Sleep(100);
    }
    if (!hasDX) { log("FATAL: tidak ada DX11/DX12 dalam 60 detik."); return 1; }

    bool hasDX12 = GetModuleHandleA("d3d12.dll") != nullptr;
    bool hasDX11 = GetModuleHandleA("d3d11.dll") != nullptr;
    log(std::string("DX12: ") + (hasDX12 ? "ya" : "tidak") +
        " | DX11: " + (hasDX11 ? "ya" : "tidak"));

    // Grace period agar swapchain game terbentuk
    Sleep(1500);
    log("Grace period selesai, init client...");

    // Init modules
    Client::Core::initializeClient();
    log("initializeClient() done");

    // Install MinHook dulu (dibutuhkan D3DHook + CommandQueueHook)
    MH_Initialize();
    log("MH_Initialize done");

    // Kalau DX12 — install CommandQueueHook lebih dulu
    // supaya saat D3DHook Present pertama dipanggil, queue sudah siap
    if (hasDX12) {
        bool cqOk = Client::Hooks::CommandQueueHook::get().install();
        log(std::string("CommandQueueHook: ") + (cqOk ? "OK" : "FAILED (mungkin tidak perlu)"));
    }

    // Install D3DHook (Present hook via kiero)
    bool hookOk = false;
    for (int i = 0; i < 30 && !hookOk; ++i) {
        hookOk = Client::Hooks::D3DHook::get().install();
        std::ostringstream ss;
        ss << "D3DHook attempt " << (i+1) << ": " << (hookOk ? "OK" : "fail");
        log(ss.str());
        if (!hookOk) Sleep(300);
    }

    if (!hookOk) {
        log("FATAL: D3DHook gagal setelah 30 percobaan.");
        return 1;
    }

    log("D3DHook installed! Overlay harusnya muncul saat frame pertama.");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstance);
            {
                std::string p = getLogPath();
                ensureDir(p);
                std::ofstream f(p, std::ios::trunc);
                f << "=== LegalClient Debug Log ===\n";
                f << "DLL_PROCESS_ATTACH fired\n";
                f.flush();
            }
            CloseHandle(CreateThread(nullptr, 0, clientThread, nullptr, 0, nullptr));
            break;

        case DLL_PROCESS_DETACH:
            log("DLL_PROCESS_DETACH");
            Client::Hooks::CommandQueueHook::get().uninstall();
            Client::Hooks::D3DHook::get().uninstall();
            MH_Uninitialize();
            Client::Core::shutdownClient();
            break;
    }
    return TRUE;
}
