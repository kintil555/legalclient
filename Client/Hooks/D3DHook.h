#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <MinHook.h>
#include "../Core/Client.h"   // for Client::Core::renderClient()

// Forward-declare ImGui Win32 WndProc handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Client::Hooks {

// D3DHook - hooks IDXGISwapChain::Present (vtable index 8) so we can:
//   1. Initialize ImGui once on the first frame.
//   2. Call ImGui::NewFrame() -> renderClient() -> ImGui::Render() every frame.
//   3. Clean up ImGui on uninstall.
//
// Catatan GDK (v1.21.120+, 26.31):
//   - Game jalan sebagai Win32 EXE biasa, bukan UWP container.
//   - Untuk buat dummy SwapChain, kita butuh HWND yang valid dari game.
//     GetForegroundWindow() tidak reliable di GDK — game sering belum jadi
//     foreground window saat DLL diinject. Kita cari HWND game secara aktif.
//   - SwapChain dummy HARUS pakai HWND game yang actual, bukan sembarang HWND,
//     karena D3D11 akan validasi window ownership pada beberapa driver.

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

class D3DHook {
public:
    static D3DHook& get() {
        static D3DHook instance;
        return instance;
    }

    bool install() {
        if (m_installed) return true;

        // --- 1. Cari HWND game yang valid ----------------------------------------
        HWND gameWindow = findGameWindow();
        if (!gameWindow) {
            // Fallback ke GetForegroundWindow jika enumerasi gagal
            gameWindow = GetForegroundWindow();
        }
        if (!gameWindow) return false;

        // --- 2. Create dummy D3D11 device untuk ambil SwapChain vtable ----------
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount        = 1;
        sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width   = 8;   // minimal size, tidak perlu match resolusi game
        sd.BufferDesc.Height  = 8;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow       = gameWindow; // HARUS pakai HWND game yang valid!
        sd.SampleDesc.Count   = 1;
        sd.Windowed           = TRUE;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device*        dummyDevice  = nullptr;
        ID3D11DeviceContext* dummyContext = nullptr;
        IDXGISwapChain*      dummyChain  = nullptr;
        D3D_FEATURE_LEVEL    level;

        // Coba beberapa feature level agar kompatibel dengan berbagai GPU/driver
        constexpr D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &sd, &dummyChain, &dummyDevice, &level, &dummyContext);

        if (FAILED(hr)) {
            // Hardware gagal (misal: GPU belum ready), coba WARP software renderer
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                nullptr, 0, D3D11_SDK_VERSION,
                &sd, &dummyChain, &dummyDevice, &level, &dummyContext);
        }

        if (FAILED(hr)) return false;

        // vtable[8] = IDXGISwapChain::Present
        void** vt         = *reinterpret_cast<void***>(dummyChain);
        void*  presentAddr = vt[8];

        dummyChain->Release();
        dummyDevice->Release();
        dummyContext->Release();

        // --- 3. Hook Present via MinHook -----------------------------------------
        if (MH_Initialize() != MH_OK) return false;

        if (MH_CreateHook(presentAddr,
                          reinterpret_cast<void*>(&D3DHook::hookedPresent),
                          reinterpret_cast<void**>(&m_origPresent)) != MH_OK)
            return false;

        if (MH_EnableHook(presentAddr) != MH_OK) return false;

        m_presentTarget = presentAddr;
        m_installed     = true;
        return true;
    }

    void uninstall() {
        if (!m_installed) return;

        MH_DisableHook(m_presentTarget);
        MH_RemoveHook(m_presentTarget);
        MH_Uninitialize();

        if (m_imguiReady) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            m_imguiReady = false;
        }

        if (m_renderTargetView) { m_renderTargetView->Release(); m_renderTargetView = nullptr; }
        if (m_deviceContext)    { m_deviceContext->Release();    m_deviceContext    = nullptr; }
        if (m_device)           { m_device->Release();           m_device           = nullptr; }

        m_installed = false;
    }

    [[nodiscard]] bool isInstalled() const { return m_installed; }

private:
    D3DHook() = default;

    // Cari HWND game (Minecraft.Windows.exe GDK) yang benar-benar punya
    // D3D-renderable surface. GDK Minecraft pakai GLFW sebagai windowing
    // backend, sehingga window class-nya "GLFW30".
    static HWND findGameWindow() {
        // Primary: cari class GLFW30 milik proses ini
        struct Ctx { DWORD pid; HWND result; };
        Ctx ctx{ GetCurrentProcessId(), nullptr };

        // Pertama coba FindWindowA dengan class GLFW30
        HWND hwnd = FindWindowA("GLFW30", nullptr);
        if (hwnd) {
            // Verify milik proses kita
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == ctx.pid) return hwnd;
        }

        // Fallback: enumerate semua top-level windows
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

    void initImGui(IDXGISwapChain* pSwapChain) {
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device),
                   reinterpret_cast<void**>(&m_device))))
            return;

        m_device->GetImmediateContext(&m_deviceContext);

        ID3D11Texture2D* pBack = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                              reinterpret_cast<void**>(&pBack));
        if (pBack) {
            m_device->CreateRenderTargetView(pBack, nullptr, &m_renderTargetView);
            pBack->Release();
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        pSwapChain->GetDesc(&desc);
        m_hwnd = desc.OutputWindow;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Dark theme with rounded corners
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 6.0f;
        style.FrameRounding  = 4.0f;
        style.GrabRounding   = 4.0f;

        ImGui_ImplWin32_Init(m_hwnd);
        ImGui_ImplDX11_Init(m_device, m_deviceContext);

        m_imguiReady = true;
    }

    static HRESULT WINAPI hookedPresent(IDXGISwapChain* pSwapChain,
                                         UINT SyncInterval, UINT Flags) {
        D3DHook& self = D3DHook::get();

        if (!self.m_imguiReady)
            self.initImGui(pSwapChain);

        if (self.m_imguiReady && self.m_renderTargetView) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Drive all client rendering (menu + HUD modules)
            Client::Core::renderClient();

            ImGui::Render();
            self.m_deviceContext->OMSetRenderTargets(
                1, &self.m_renderTargetView, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        return self.m_origPresent(pSwapChain, SyncInterval, Flags);
    }

    bool                    m_installed        = false;
    bool                    m_imguiReady       = false;
    void*                   m_presentTarget    = nullptr;
    PresentFn               m_origPresent      = nullptr;

    ID3D11Device*           m_device           = nullptr;
    ID3D11DeviceContext*    m_deviceContext    = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;
    HWND                    m_hwnd             = nullptr;
};

} // namespace Client::Hooks
