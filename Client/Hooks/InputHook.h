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
//   - GDK (v1.21.120+, termasuk 26.31): game pakai GLFW sebagai windowing
//     backend. Window class = "GLFW30". Ini BERBEDA dari UWP, dan
//     FindWindowA("Windows.UI.Core.CoreWindow") akan SELALU GAGAL di GDK.
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

    // Finds the Minecraft Bedrock (GDK) window and replaces its WndProc
    // with our hooked procedure. Safe to call more than once.
    bool install() {
        if (m_installed) return true;

        HWND hwnd = findGameWindow();
        if (!hwnd) return false;

        m_hwnd        = hwnd;
        m_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(&InputHook::hookedWndProc)));

        m_installed = (m_origWndProc != nullptr);
        return m_installed;
    }

    // Restores the original WndProc. Must be called before the DLL unloads.
    void uninstall() {
        if (!m_installed || !m_hwnd) return;
        SetWindowLongPtrA(m_hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(m_origWndProc));
        m_installed   = false;
        m_hwnd        = nullptr;
        m_origWndProc = nullptr;
    }

    [[nodiscard]] bool isInstalled() const { return m_installed; }

private:
    InputHook() = default;

    // Cari HWND game yang tepat untuk GDK Minecraft (26.31).
    // Urutan pencarian:
    //   1. FindWindowA("GLFW30") — GDK Minecraft 1.21.120+ pakai GLFW
    //   2. Enumerate top-level windows milik proses ini (fallback universal)
    // NOTE: "Windows.UI.Core.CoreWindow" adalah class UWP LAMA, JANGAN PAKAI
    //       untuk GDK karena tidak akan ketemu.
    static HWND findGameWindow() {
        const DWORD myPid = GetCurrentProcessId();

        // Coba GLFW30 dulu (primary untuk GDK Minecraft)
        HWND hwnd = FindWindowA("GLFW30", nullptr);
        if (hwnd) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == myPid) return hwnd;
            // GLFW30 ada tapi bukan punya proses kita, lanjut ke fallback
        }

        // Fallback: enumerate semua top-level windows milik proses kita.
        // Cari yang visible dan punya client area > 200px (bukan tooltip, dll).
        struct Context { DWORD pid; HWND result; };
        Context ctx{ myPid, nullptr };

        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Context*>(lp);
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid != c->pid || !IsWindowVisible(h)) return TRUE;

            RECT r{};
            GetClientRect(h, &r);
            if ((r.right - r.left) > 200 && (r.bottom - r.top) > 200) {
                c->result = h;
                return FALSE; // stop enumeration, ketemu
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

        // Forward ke ImGui dulu agar ImGui bisa handle input di menu
        // (GDK Minecraft bisa pakai WndProc normal, tidak perlu treatment khusus)
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return true;

        switch (msg) {
            // ---- Keyboard ----
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                dispatchKey(static_cast<int>(wParam), true);
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                dispatchKey(static_cast<int>(wParam), false);
                break;

            // ---- Mouse buttons (KeystrokeHUD needs these) ----
            case WM_LBUTTONDOWN: dispatchKey(VK_LBUTTON, true);  break;
            case WM_LBUTTONUP:   dispatchKey(VK_LBUTTON, false); break;
            case WM_RBUTTONDOWN: dispatchKey(VK_RBUTTON, true);  break;
            case WM_RBUTTONUP:   dispatchKey(VK_RBUTTON, false); break;

            default: break;
        }

        // Selalu forward ke original WndProc agar game tetap menerima input.
        return CallWindowProcA(self.m_origWndProc, hwnd, msg, wParam, lParam);
    }

    bool     m_installed   = false;
    HWND     m_hwnd        = nullptr;
    WNDPROC  m_origWndProc = nullptr;
};

} // namespace Client::Hooks
