#pragma once
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include "../Events/EventBus.h"

// InputHook - subclasses the game window's WndProc so every WM_KEYDOWN,
// WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_LBUTTONDOWN, WM_LBUTTONUP,
// WM_RBUTTONDOWN, dan WM_RBUTTONUP message diforward ke EventBus.
//
// Catatan GDK vs UWP:
//   - UWP (sebelum v1.21.120): window class = "Windows.UI.Core.CoreWindow"
//   - GDK (v1.21.120+, termasuk 26.31): window class = "Bedrock" (W).
//     Referensi: Flarial OSS src/Client/Managers/WindowManager.cpp
//     menggunakan FindWindowExW(nullptr, nullptr, L"Bedrock", nullptr).
//
// Installation:  call InputHook::install() once from initializeClient().
// Removal:       call InputHook::uninstall() from shutdownClient().

namespace Client::Hooks {

class InputHook {
public:
    static InputHook& get() {
        static InputHook instance;
        return instance;
    }

    bool install() {
        if (m_installed) return true;

        HWND hwnd = findGameWindow();
        if (!hwnd) return false;

        m_hwnd = hwnd;
        // Pakai SetWindowLongPtrW karena GDK window adalah Unicode window
        m_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(&InputHook::hookedWndProc)));

        m_installed = (m_origWndProc != nullptr);
        return m_installed;
    }

    void uninstall() {
        if (!m_installed || !m_hwnd) return;
        SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(m_origWndProc));
        m_installed   = false;
        m_hwnd        = nullptr;
        m_origWndProc = nullptr;
    }

    [[nodiscard]] bool isInstalled() const { return m_installed; }

private:
    InputHook() = default;

    // Cari HWND game GDK Minecraft (1.21.120+, 26.31).
    // Primary: FindWindowExW dengan class L"Bedrock" — persis seperti Flarial OSS.
    // Fallback: enumerate top-level windows milik proses ini.
    static HWND findGameWindow() {
        const DWORD myPid = GetCurrentProcessId();

        // Primary: class "Bedrock" (GDK Minecraft 1.21.120+)
        HWND wnd = nullptr;
        while ((wnd = FindWindowExW(nullptr, wnd, L"Bedrock", nullptr))) {
            DWORD pid = 0;
            GetWindowThreadProcessId(wnd, &pid);
            if (pid == myPid) return wnd;
        }

        // Fallback: enumerate semua top-level windows milik proses kita.
        struct Context { DWORD pid; HWND result; };
        Context ctx{ myPid, nullptr };
        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Context*>(lp);
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid != c->pid || !IsWindowVisible(h)) return TRUE;
            RECT r{}; GetClientRect(h, &r);
            if ((r.right - r.left) > 200 && (r.bottom - r.top) > 200) {
                c->result = h;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

        return ctx.result;
    }

    static void dispatchKey(int vKey, bool isDown) {
        Events::EventBus::get().key.dispatch({ vKey, isDown });
    }

    static LRESULT CALLBACK hookedWndProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam) {
        InputHook& self = InputHook::get();

        // Forward ke ImGui agar input di menu bekerja.
        // Forward declare manual karena fungsi ini ada di .cpp, bukan di header.
        extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return true;

        switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                dispatchKey(static_cast<int>(wParam), true);
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                dispatchKey(static_cast<int>(wParam), false);
                break;
            case WM_LBUTTONDOWN: dispatchKey(VK_LBUTTON, true);  break;
            case WM_LBUTTONUP:   dispatchKey(VK_LBUTTON, false); break;
            case WM_RBUTTONDOWN: dispatchKey(VK_RBUTTON, true);  break;
            case WM_RBUTTONUP:   dispatchKey(VK_RBUTTON, false); break;
            default: break;
        }

        // Selalu forward ke original WndProc (pakai W karena GDK adalah Unicode window)
        return CallWindowProcW(self.m_origWndProc, hwnd, msg, wParam, lParam);
    }

    bool     m_installed   = false;
    HWND     m_hwnd        = nullptr;
    WNDPROC  m_origWndProc = nullptr;
};

} // namespace Client::Hooks
