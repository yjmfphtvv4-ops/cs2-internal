#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <MinHook.h>
#include <cstdint>

namespace PresentHook {
    bool Initialize();
    void Shutdown();

    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    extern PresentFn oPresent;

    HWND GetProcessWindow();
    HRESULT STDMETHODCALLTYPE Hook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
}
