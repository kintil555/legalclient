#pragma once
#include <Windows.h>
#include "../Events/EventBus.h"

// InputHook - subclasses the game window's WndProc so every WM_KEYDOWN,
// WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_LBUTTONDOWN, WM_LBUTTONUP,
// WM_RBUTTONDOWN, and WM_RBUTTONUP message is forwarded into the EventBus
// key channel.  That is the missing piece that makes the menu toggle and
// KeystrokeHUD work: the EventBus had subscribers but nothing was ever
// calling dispatch() with real input.
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

    // Finds the Minecraft Bedrock window, saves the original WndProc, and
    // replaces it with our hooked procedure.  Safe to call more than once
    // (subsequent calls are no-ops if already installed).
    bool install() {
        if (m_installed) return true;

        // Minecraft Bedrock's main window class name (Windows 10/11 store build
        // and "preview" APK-on-Windows builds both use this class).
        HWND hwnd = FindWindowA("Windows.UI.Core.CoreWindow", nullptr);
        if (!hwnd) {
            // Fallback: iterate all top-level windows owned by this process.
            hwnd = findGameWindow();
        }
        if (!hwnd) return false;

        m_hwnd        = hwnd;
        m_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(&InputHook::hookedWndProc)));

        m_installed = (m_origWndProc != nullptr);
        return m_installed;
    }

    // Restores the original WndProc.  Must be called before the DLL unloads.
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

    // Walk top-level windows to find the one belonging to our process.
    static HWND findGameWindow() {
        struct Context { DWORD pid; HWND result; };
        Context ctx{ GetCurrentProcessId(), nullptr };

        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
            auto* ctx = reinterpret_cast<Context*>(lp);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == ctx->pid && IsWindowVisible(hwnd)) {
                ctx->result = hwnd;
                return FALSE; // stop enumeration
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

        return ctx.result;
    }

    // Dispatch a virtual-key event into the EventBus then call the original
    // WndProc so the game still receives its own input normally.
    static void dispatchKey(int vKey, bool isDown) {
        Events::EventBus::get().key.dispatch({ vKey, isDown });
    }

    static LRESULT CALLBACK hookedWndProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam) {
        InputHook& self = InputHook::get();

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

        // Always forward to the original WndProc so the game works normally.
        return CallWindowProcA(self.m_origWndProc, hwnd, msg, wParam, lParam);
    }

    bool     m_installed   = false;
    HWND     m_hwnd        = nullptr;
    WNDPROC  m_origWndProc = nullptr;
};

} // namespace Client::Hooks
