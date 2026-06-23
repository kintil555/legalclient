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
// Usage:
//   D3DHook::get().install();   // called from worker thread after game window ready
//   D3DHook::get().uninstall(); // called from DLL_PROCESS_DETACH

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

class D3DHook {
public:
    static D3DHook& get() {
        static D3DHook instance;
        return instance;
    }

    bool install() {
        if (m_installed) return true;

        // --- 1. Create dummy D3D11 device to get SwapChain vtable ptr --------
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount        = 1;
        sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow       = GetForegroundWindow();
        sd.SampleDesc.Count   = 1;
        sd.Windowed           = TRUE;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device*        dummyDevice  = nullptr;
        ID3D11DeviceContext* dummyContext = nullptr;
        IDXGISwapChain*      dummyChain  = nullptr;
        D3D_FEATURE_LEVEL    level;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &sd, &dummyChain, &dummyDevice, &level, &dummyContext);

        if (FAILED(hr)) return false;

        // vtable[8] = IDXGISwapChain::Present
        void** vt         = *reinterpret_cast<void***>(dummyChain);
        void*  presentAddr = vt[8];

        dummyChain->Release();
        dummyDevice->Release();
        dummyContext->Release();

        // --- 2. Hook Present via MinHook -------------------------------------
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
