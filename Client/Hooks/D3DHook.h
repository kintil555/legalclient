#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <MinHook.h>
#include <kiero/kiero.h>
#include "../Core/Client.h"
#include "InputHook.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Client::Hooks {

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

// ============================================================================
// D3DHook — hooks IDXGISwapChain::Present via kiero.
//
// Minecraft Bedrock GDK 26.31 bisa pakai DX11 atau DX12 tergantung hardware.
// Kiero auto-detect, lalu kita bind Present:
//   DX11: vtable index 8   (IDXGISwapChain::Present)
//   DX12: vtable index 140 (IDXGISwapChain::Present juga, offset berbeda karena ID3D12CommandQueue)
//
// Referensi: Flarial OSS SwapchainHook.cpp + Manager.cpp
// ============================================================================
class D3DHook {
public:
    static D3DHook& get() {
        static D3DHook instance;
        return instance;
    }

    bool install() {
        if (m_installed) return true;

        // --- 1. Init kiero — auto-detect DX12, fallback DX11 -----------------
        kiero::Status::Enum s = kiero::init(kiero::RenderType::D3D12);
        if (s != kiero::Status::Success) {
            s = kiero::init(kiero::RenderType::D3D11);
        }
        if (s != kiero::Status::Success) return false;

        m_isDX12 = (kiero::getRenderType() == kiero::RenderType::D3D12);

        // --- 2. Bind Present hook --------------------------------------------
        // DX11: index 8  |  DX12: index 140
        const uint16_t presentIdx = m_isDX12 ? 140 : 8;
        if (kiero::bind(presentIdx,
                        reinterpret_cast<void**>(&m_origPresent),
                        reinterpret_cast<void*>(&D3DHook::hookedPresent))
            != kiero::Status::Success) {
            kiero::shutdown();
            return false;
        }

        m_installed = true;
        return true;
    }

    void uninstall() {
        if (!m_installed) return;

        if (m_imguiReady) {
            if (m_isDX12) {
                ImGui_ImplDX12_Shutdown();
            } else {
                ImGui_ImplDX11_Shutdown();
            }
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            m_imguiReady = false;
        }

        releaseDX11();
        releaseDX12();

        kiero::shutdown();
        m_installed = false;
    }

    [[nodiscard]] bool isInstalled() const { return m_installed; }
    [[nodiscard]] bool isDX12()       const { return m_isDX12; }

private:
    D3DHook() = default;

    // -------------------------------------------------------------------------
    // DX11 init
    // -------------------------------------------------------------------------
    bool initDX11(IDXGISwapChain* pSwapChain) {
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device),
                   reinterpret_cast<void**>(&m_dx11Device)))) return false;
        m_dx11Device->GetImmediateContext(&m_dx11Context);

        ID3D11Texture2D* pBack = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                              reinterpret_cast<void**>(&pBack));
        if (pBack) {
            m_dx11Device->CreateRenderTargetView(pBack, nullptr, &m_dx11RTV);
            pBack->Release();
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        pSwapChain->GetDesc(&desc);
        m_hwnd = desc.OutputWindow;

        // Fallback: cari window via class name jika OutputWindow null/invalid
        if (!m_hwnd || !IsWindow(m_hwnd)) {
            m_hwnd = findGameWindow();
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        applyImGuiStyle();

        ImGui_ImplWin32_Init(m_hwnd);
        ImGui_ImplDX11_Init(m_dx11Device, m_dx11Context);
        return true;
    }

    // -------------------------------------------------------------------------
    // DX12 init
    // -------------------------------------------------------------------------
    bool initDX12(IDXGISwapChain* pSwapChain) {
        auto* pSwapChain3 = static_cast<IDXGISwapChain3*>(pSwapChain);

        // Ambil device DX12
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D12Device),
                   reinterpret_cast<void**>(&m_dx12Device)))) return false;

        DXGI_SWAP_CHAIN_DESC sdesc{};
        pSwapChain->GetDesc(&sdesc);
        m_hwnd        = sdesc.OutputWindow;
        m_bufferCount = sdesc.BufferCount;

        if (!m_hwnd || !IsWindow(m_hwnd)) m_hwnd = findGameWindow();

        // Descriptor heap untuk ImGui (font + tekstur)
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = m_bufferCount + 1;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(m_dx12Device->CreateDescriptorHeap(&heapDesc,
                   IID_PPV_ARGS(&m_dx12SrvHeap)))) return false;

        // RTV heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = m_bufferCount;
        rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(m_dx12Device->CreateDescriptorHeap(&rtvHeapDesc,
                   IID_PPV_ARGS(&m_dx12RtvHeap)))) return false;

        // Per-frame command allocators + RTVs
        m_dx12Frames.resize(m_bufferCount);
        SIZE_T rtvSize = m_dx12Device->GetDescriptorHandleIncrementSize(
                             D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            m_dx12RtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < m_bufferCount; ++i) {
            m_dx12Device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_dx12Frames[i].cmdAlloc));

            ID3D12Resource* pBuf = nullptr;
            pSwapChain3->GetBuffer(i, IID_PPV_ARGS(&pBuf));
            if (pBuf) {
                m_dx12Device->CreateRenderTargetView(pBuf, nullptr, rtvHandle);
                m_dx12Frames[i].rtvHandle = rtvHandle;
                m_dx12Frames[i].resource  = pBuf;
                pBuf->Release();
            }
            rtvHandle.ptr += rtvSize;
        }

        // Command list + queue (queue diambil dari kiero methods table)
        // index 4 di DX12 methods table = ExecuteCommandLists yang mengandung queue ptr
        // Cara Flarial: simpan queue dari CreateCommandQueue hook.
        // Kita pakai cara sederhana: buat command list sendiri.
        m_dx12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_dx12Frames[0].cmdAlloc, nullptr,
            IID_PPV_ARGS(&m_dx12CmdList));
        m_dx12CmdList->Close();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        applyImGuiStyle();

        ImGui_ImplWin32_Init(m_hwnd);
        ImGui_ImplDX12_Init(m_dx12Device, static_cast<int>(m_bufferCount),
            DXGI_FORMAT_R8G8B8A8_UNORM,
            m_dx12SrvHeap,
            m_dx12SrvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_dx12SrvHeap->GetGPUDescriptorHandleForHeapStart());
        return true;
    }

    static void applyImGuiStyle() {
        ImGui::StyleColorsDark();
        ImGuiStyle& st  = ImGui::GetStyle();
        st.WindowRounding = 6.0f;
        st.FrameRounding  = 4.0f;
        st.GrabRounding   = 4.0f;
    }

    // -------------------------------------------------------------------------
    // Cari HWND Minecraft GDK — class "Bedrock" (1.21.120+, 26.31)
    // -------------------------------------------------------------------------
    static HWND findGameWindow() {
        const DWORD myPid = GetCurrentProcessId();

        // Primary: window class "Bedrock" (GDK)
        HWND w = nullptr;
        while ((w = FindWindowExW(nullptr, w, L"Bedrock", nullptr))) {
            DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
            if (pid == myPid) return w;
        }

        // Fallback: enumerate window visible milik proses ini
        struct Ctx { DWORD pid; HWND result; };
        Ctx ctx{ myPid, nullptr };
        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
            DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
            if (pid != c->pid || !IsWindowVisible(h)) return TRUE;
            RECT r{}; GetClientRect(h, &r);
            if (r.right > 200 && r.bottom > 200) { c->result = h; return FALSE; }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
        return ctx.result;
    }

    // -------------------------------------------------------------------------
    // Release helpers
    // -------------------------------------------------------------------------
    void releaseDX11() {
        if (m_dx11RTV)     { m_dx11RTV->Release();     m_dx11RTV     = nullptr; }
        if (m_dx11Context) { m_dx11Context->Release();  m_dx11Context = nullptr; }
        if (m_dx11Device)  { m_dx11Device->Release();   m_dx11Device  = nullptr; }
    }

    void releaseDX12() {
        if (m_dx12CmdList)  { m_dx12CmdList->Release();  m_dx12CmdList  = nullptr; }
        if (m_dx12SrvHeap)  { m_dx12SrvHeap->Release();  m_dx12SrvHeap  = nullptr; }
        if (m_dx12RtvHeap)  { m_dx12RtvHeap->Release();  m_dx12RtvHeap  = nullptr; }
        for (auto& f : m_dx12Frames) {
            if (f.cmdAlloc) { f.cmdAlloc->Release(); f.cmdAlloc = nullptr; }
        }
        m_dx12Frames.clear();
        if (m_dx12Device)  { m_dx12Device->Release();   m_dx12Device   = nullptr; }
    }

    // -------------------------------------------------------------------------
    // Hooked Present — dipanggil setiap frame oleh game
    // -------------------------------------------------------------------------
    static HRESULT WINAPI hookedPresent(IDXGISwapChain* pSwapChain,
                                         UINT SyncInterval, UINT Flags) {
        D3DHook& self = D3DHook::get();

        // Init ImGui saat pertama kali Present dipanggil
        if (!self.m_imguiReady) {
            bool ok = self.m_isDX12
                ? self.initDX12(pSwapChain)
                : self.initDX11(pSwapChain);
            if (ok) {
                self.m_imguiReady = true;
                // Install InputHook sekarang — window sudah pasti valid
                InputHook::get().install();
            }
        }

        if (self.m_imguiReady) {
            // Poll keyboard setiap frame (primary input method)
            InputHook::get().pollKeys();

            if (self.m_isDX12) {
                self.renderDX12(static_cast<IDXGISwapChain3*>(pSwapChain));
            } else {
                self.renderDX11();
            }
        }

        return self.m_origPresent(pSwapChain, SyncInterval, Flags);
    }

    void renderDX11() {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Core::renderClient();

        ImGui::Render();
        m_dx11Context->OMSetRenderTargets(1, &m_dx11RTV, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void renderDX12(IDXGISwapChain3* pSwapChain) {
        if (!m_dx12Queue) return; // queue belum di-set

        UINT frameIdx = pSwapChain->GetCurrentBackBufferIndex();
        if (frameIdx >= m_dx12Frames.size()) return;

        auto& frame = m_dx12Frames[frameIdx];
        frame.cmdAlloc->Reset();
        m_dx12CmdList->Reset(frame.cmdAlloc, nullptr);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource   = frame.resource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_dx12CmdList->ResourceBarrier(1, &barrier);
        m_dx12CmdList->OMSetRenderTargets(1, &frame.rtvHandle, FALSE, nullptr);
        m_dx12CmdList->SetDescriptorHeaps(1, &m_dx12SrvHeap);

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        Core::renderClient();
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_dx12CmdList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        m_dx12CmdList->ResourceBarrier(1, &barrier);
        m_dx12CmdList->Close();

        ID3D12CommandList* lists[] = { m_dx12CmdList };
        m_dx12Queue->ExecuteCommandLists(1, lists);
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------
    bool       m_installed  = false;
    bool       m_imguiReady = false;
    bool       m_isDX12     = false;
    HWND       m_hwnd       = nullptr;
    PresentFn  m_origPresent = nullptr;

    // DX11
    ID3D11Device*           m_dx11Device  = nullptr;
    ID3D11DeviceContext*    m_dx11Context = nullptr;
    ID3D11RenderTargetView* m_dx11RTV     = nullptr;

    // DX12
    struct DX12Frame {
        ID3D12CommandAllocator*      cmdAlloc  = nullptr;
        ID3D12Resource*              resource  = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE  rtvHandle = {};
    };
    ID3D12Device*              m_dx12Device  = nullptr;
    ID3D12DescriptorHeap*      m_dx12SrvHeap = nullptr;
    ID3D12DescriptorHeap*      m_dx12RtvHeap = nullptr;
    ID3D12GraphicsCommandList* m_dx12CmdList = nullptr;
    ID3D12CommandQueue*        m_dx12Queue   = nullptr; // di-set oleh CommandQueueHook
    std::vector<DX12Frame>     m_dx12Frames;
    UINT                       m_bufferCount = 0;

public:
    // DX12 queue harus di-set dari luar saat game create command queue
    void setCommandQueue(ID3D12CommandQueue* q) { m_dx12Queue = q; }
    [[nodiscard]] bool needsCommandQueue() const { return m_isDX12 && !m_dx12Queue; }
};

} // namespace Client::Hooks
