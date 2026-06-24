#include <Windows.h>
#include <fstream>
#include <string>
#include <sstream>
#include "Core/Client.h"
#include "Hooks/D3DHook.h"

// ============================================================================
// DllMain.cpp — dengan file logger untuk debug inject
// Log ditulis ke: %APPDATA%\LegalClient\debug.log
// ============================================================================

namespace {

std::string getLogPath() {
    char appdata[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH) == 0)
        return "C:\\LegalClient_debug.log";
    return std::string(appdata) + "\\LegalClient\\debug.log";
}

void ensureDir(const std::string& path) {
    // Extract directory from path
    auto pos = path.rfind('\\');
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
        CreateDirectoryA(dir.c_str(), nullptr);
    }
}

void log(const std::string& msg) {
    static std::string logPath = getLogPath();
    static bool dirReady = false;
    if (!dirReady) { ensureDir(logPath); dirReady = true; }

    std::ofstream f(logPath, std::ios::app);
    if (!f) return;

    // Timestamp sederhana pakai GetTickCount
    std::ostringstream ss;
    ss << "[+" << GetTickCount() << "ms] " << msg << "\n";
    f << ss.str();
    f.flush();
}

bool waitForD3D11(DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        if (GetModuleHandleA("d3d11.dll") != nullptr)
            return true;
        Sleep(100);
    }
    return false;
}

} // namespace

static DWORD WINAPI clientThread(LPVOID) {
    log("=== LegalClient DLL loaded ===");
    log("clientThread started");

    // Cek apakah d3d11.dll sudah ada (langsung, tanpa tunggu)
    bool d3dAlready = GetModuleHandleA("d3d11.dll") != nullptr;
    log(std::string("d3d11.dll saat inject: ") + (d3dAlready ? "SUDAH ada" : "belum ada, tunggu..."));

    if (!waitForD3D11(60000)) {
        log("FATAL: d3d11.dll tidak muncul dalam 60 detik. Abort.");
        return 1;
    }
    log("d3d11.dll confirmed loaded");

    Sleep(800);
    log("Grace period selesai, init client...");

    Client::Core::initializeClient();
    log("initializeClient() done");

    bool ok = false;
    for (int i = 0; i < 20 && !ok; ++i) {
        ok = Client::Hooks::D3DHook::get().install();
        std::ostringstream ss;
        ss << "D3DHook::install() attempt " << (i+1) << ": " << (ok ? "SUCCESS" : "FAILED");
        log(ss.str());
        if (!ok) Sleep(500);
    }

    if (!ok) {
        log("FATAL: D3DHook gagal setelah 20 percobaan.");
    } else {
        log("Client fully initialized! Overlay harusnya muncul.");
    }

    return ok ? 0 : 1;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstance);
            // Tulis log awal SEBELUM spawn thread agar selalu terekam
            {
                // Reset log file di setiap inject baru
                std::string p = getLogPath();
                ensureDir(p);
                std::ofstream f(p, std::ios::trunc);
                f << "=== LegalClient Debug Log ===\n";
                f << "DLL_PROCESS_ATTACH fired\n";
            }
            CloseHandle(CreateThread(nullptr, 0, clientThread, nullptr, 0, nullptr));
            break;

        case DLL_PROCESS_DETACH:
            log("DLL_PROCESS_DETACH — unloading");
            Client::Hooks::D3DHook::get().uninstall();
            Client::Core::shutdownClient();
            break;
    }
    return TRUE;
}
