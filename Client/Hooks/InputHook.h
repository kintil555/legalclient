#pragma once
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include "../Events/EventBus.h"

// InputHook - menangkap key input game via dua metode:
//
// METODE 1 (primary): GetAsyncKeyState polling setiap frame dari render hook.
//   Ini JAUH lebih reliable di GDK karena tidak butuh SetWindowLongPtrW.
//   SetWindowLongPtrW gagal di GDK karena Windows tidak mengizinkan subclassing
//   window dari thread/DLL yang bukan pemilik window tersebut.
//
// METODE 2 (secondary): WndProc subclass, dicoba tetap tapi sebagai fallback.
//   Kalau berhasil, key event dari WndProc juga masuk EventBus.
//   Tidak masalah kalau gagal karena polling sudah cukup.
//
// Catatan GDK: window class = L"Bedrock" (v1.21.120+, termasuk 26.31).
// Referensi: Flarial OSS WindowManager.cpp

namespace Client::Hooks {

// Daftar virtual key yang dipantau oleh polling
static constexpr int kTrackedKeys[] = {
    'L',         // menu toggle
    'W', 'A', 'S', 'D',    // WASD untuk KeystrokeHUD
    VK_SPACE, VK_SHIFT, VK_CONTROL,
    VK_LBUTTON, VK_RBUTTON,
    VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
};

class InputHook {
public:
    static InputHook& get() {
        static InputHook instance;
        return instance;
    }

    bool install() {
        if (m_installed) return true;

        // Coba WndProc subclass (opsional, boleh gagal)
        HWND hwnd = findGameWindow();
        if (hwnd) {
            m_hwnd = hwnd;
            // Pakai SetWindowLongPtrW
            m_origWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(&InputHook::hookedWndProc)));
            if (!m_origWndProc) {
                // Gagal - tidak apa-apa, polling akan handle key input
                m_hwnd = nullptr;
            }
        }

        // Init state polling semua tracked key ke "tidak ditekan"
        for (int k : kTrackedKeys) m_prevKeyState[k] = false;

        m_installed = true;
        return true;
    }

    void uninstall() {
        if (!m_installed) return;
        if (m_hwnd && m_origWndProc) {
            SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(m_origWndProc));
            m_origWndProc = nullptr;
            m_hwnd = nullptr;
        }
        m_installed = false;
    }

    // Dipanggil setiap frame dari D3DHook (sebelum ImGui render).
    // Poll GetAsyncKeyState untuk semua tracked key dan dispatch event
    // kalau ada perubahan state (edge detection).
    void pollKeys() {
        for (int vk : kTrackedKeys) {
            const bool pressed = (GetAsyncKeyState(vk) & 0x8000) != 0;
            const bool prev    = m_prevKeyState[vk];

            if (pressed != prev) {
                Events::EventBus::get().key.dispatch({ vk, pressed });
                m_prevKeyState[vk] = pressed;
            }
        }
    }

    [[nodiscard]] bool isInstalled() const { return m_installed; }
    [[nodiscard]] bool hasWndProcHook() const { return m_hwnd != nullptr; }

private:
    InputHook() = default;

    static HWND findGameWindow() {
        const DWORD myPid = GetCurrentProcessId();

        // Primary: class "Bedrock" (GDK Minecraft 1.21.120+, 26.31)
        HWND wnd = nullptr;
        while ((wnd = FindWindowExW(nullptr, wnd, L"Bedrock", nullptr))) {
            DWORD pid = 0;
            GetWindowThreadProcessId(wnd, &pid);
            if (pid == myPid) return wnd;
        }

        // Fallback: enumerate visible windows milik proses kita
        struct Ctx { DWORD pid; HWND result; };
        Ctx ctx{ myPid, nullptr };
        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
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

    static LRESULT CALLBACK hookedWndProc(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam) {
        InputHook& self = InputHook::get();

        extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return true;

        // WndProc-based key dispatch (redundan dengan polling, tidak masalah)
        switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                Events::EventBus::get().key.dispatch({
                    static_cast<int>(wParam), true });
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                Events::EventBus::get().key.dispatch({
                    static_cast<int>(wParam), false });
                break;
            case WM_LBUTTONDOWN: Events::EventBus::get().key.dispatch({ VK_LBUTTON, true });  break;
            case WM_LBUTTONUP:   Events::EventBus::get().key.dispatch({ VK_LBUTTON, false }); break;
            case WM_RBUTTONDOWN: Events::EventBus::get().key.dispatch({ VK_RBUTTON, true });  break;
            case WM_RBUTTONUP:   Events::EventBus::get().key.dispatch({ VK_RBUTTON, false }); break;
            default: break;
        }

        return CallWindowProcW(self.m_origWndProc, hwnd, msg, wParam, lParam);
    }

    bool    m_installed   = false;
    HWND    m_hwnd        = nullptr;
    WNDPROC m_origWndProc = nullptr;
    bool    m_prevKeyState[256] = {};  // state per key untuk edge detection polling
};

} // namespace Client::Hooks
