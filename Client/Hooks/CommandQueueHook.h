#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <MinHook.h>
#include "D3DHook.h"

// CommandQueueHook - intercept ID3D12Device::CreateCommandQueue
// untuk mendapat pointer ke command queue yang dipakai game.
// Wajib untuk DX12 render: tanpa queue, ExecuteCommandLists tidak bisa dipanggil.
//
// Referensi: Flarial OSS & standard DX12 overlay technique.

namespace Client::Hooks {

using CreateCommandQueueFn = HRESULT(WINAPI*)(
    ID3D12Device*, const D3D12_COMMAND_QUEUE_DESC*, REFIID, void**);

class CommandQueueHook {
public:
    static CommandQueueHook& get() {
        static CommandQueueHook instance;
        return instance;
    }

    bool install() {
        if (m_installed) return true;

        HMODULE hD3D12 = GetModuleHandleA("d3d12.dll");
        if (!hD3D12) return false;

        // Buat dummy DX12 device untuk ambil vtable CreateCommandQueue
        ID3D12Device* dummy = nullptr;
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                   IID_PPV_ARGS(&dummy)))) return false;

        // vtable index 8 = CreateCommandQueue pada ID3D12Device
        void** vt  = *reinterpret_cast<void***>(dummy);
        void*  addr = vt[8];
        dummy->Release();

        if (MH_CreateHook(addr,
                reinterpret_cast<void*>(&CommandQueueHook::hooked),
                reinterpret_cast<void**>(&m_orig)) != MH_OK) return false;
        if (MH_EnableHook(addr) != MH_OK) return false;

        m_hookAddr  = addr;
        m_installed = true;
        return true;
    }

    void uninstall() {
        if (!m_installed) return;
        MH_DisableHook(m_hookAddr);
        MH_RemoveHook(m_hookAddr);
        m_installed = false;
    }

private:
    CommandQueueHook() = default;

    static HRESULT WINAPI hooked(ID3D12Device* pDevice,
                                  const D3D12_COMMAND_QUEUE_DESC* pDesc,
                                  REFIID riid, void** ppCommandQueue) {
        HRESULT hr = get().m_orig(pDevice, pDesc, riid, ppCommandQueue);
        if (SUCCEEDED(hr) && ppCommandQueue &&
            pDesc->Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            // Ini queue yang dipakai untuk render — kasih ke D3DHook
            auto* q = static_cast<ID3D12CommandQueue*>(*ppCommandQueue);
            if (D3DHook::get().needsCommandQueue()) {
                D3DHook::get().setCommandQueue(q);
            }
        }
        return hr;
    }

    bool                   m_installed = false;
    void*                  m_hookAddr  = nullptr;
    CreateCommandQueueFn   m_orig      = nullptr;
};

} // namespace Client::Hooks
