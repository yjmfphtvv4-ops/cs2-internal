#include "present.h"
#include "../globals.h"
#include "../features/esp.h"
#include "../features/aimbot.h"
#include "../features/chams.h"
#include "../features/thirdperson.h"
#include "../features/norecoil.h"
#include "../features/bhop.h"
#include "../features/fov.h"

#include "../sdk/interfaces.h"
#include "../sdk/offsets.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <cstdio>
#include <algorithm>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace PresentHook {
    PresentFn oPresent = nullptr;
    static ID3D11Device* g_Device = nullptr;
    static ID3D11DeviceContext* g_Context = nullptr;
    static HWND g_Hwnd = nullptr;
    static bool g_ImGuiInit = false;
    static ID3D11RenderTargetView* g_BackbufferRTV = nullptr;

    static ESP g_ESP;
    static Aimbot g_Aimbot;
    static Chams g_Chams;

    static WNDPROC oWndProc = nullptr;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam) && g_Config.showMenu)
            return true;
        return CallWindowProc(oWndProc, hwnd, msg, wParam, lParam);
    }

    bool Initialize() {
        HWND gameWindow = GetProcessWindow();
        if (!gameWindow) return false;

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width = 1;
        sd.BufferDesc.Height = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = gameWindow;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swapChain = nullptr;

        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_SINGLETHREADED,
            levels, 1, D3D11_SDK_VERSION, &sd, &swapChain,
            &device, nullptr, &context
        );

        if (FAILED(hr) || !swapChain) return false;

        auto vtable = *reinterpret_cast<uintptr_t**>(swapChain);
        void* presentAddr = reinterpret_cast<void*>(vtable[8]);

        MH_CreateHook(presentAddr, &Hook, reinterpret_cast<void**>(&oPresent));
        MH_EnableHook(presentAddr);

        swapChain->Release();
        device->Release();
        context->Release();
        return true;
    }

    void Shutdown() {
        // Disable hooks first so game doesn't call our code during cleanup
        MH_DisableHook(MH_ALL_HOOKS);
        ThirdPerson::Shutdown();

        if (oWndProc) SetWindowLongPtr(g_Hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        if (g_BackbufferRTV) { g_BackbufferRTV->Release(); g_BackbufferRTV = nullptr; }
        if (g_ImGuiInit) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_ImGuiInit = false;
        }
        // Don't release g_Device/g_Context — they belong to the game
        g_Device = nullptr;
        g_Context = nullptr;
    }

    static void RenderToBackbuffer(IDXGISwapChain* pSwapChain);

    static bool g_Unloaded = false;

    HRESULT STDMETHODCALLTYPE Hook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (g_Unloaded)
            return oPresent(pSwapChain, SyncInterval, Flags);

        if (!g_ImGuiInit) {
            pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_Device));
            g_Device->GetImmediateContext(&g_Context);

            DXGI_SWAP_CHAIN_DESC desc;
            pSwapChain->GetDesc(&desc);
            g_Hwnd = desc.OutputWindow;

            oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_Hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

            ImGui::CreateContext();
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(g_Hwnd);
            ImGui_ImplDX11_Init(g_Device, g_Context);
            g_ImGuiInit = true;
        }

        RenderToBackbuffer(pSwapChain);
        HRESULT result = oPresent(pSwapChain, SyncInterval, Flags);

        static bool unloaded = false;
        static bool unloadRequested = false;
        {
            static bool lastDelete = false;
            bool currentDelete = GetAsyncKeyState(VK_DELETE) & 0x8000;
            if (currentDelete && !lastDelete)
                unloadRequested = true;
            lastDelete = currentDelete;
        }

        if (unloadRequested && !unloaded) {
            unloaded = true;
            g_Unloaded = true;
            HMODULE mod = g_DllModule;
            CreateThread(nullptr, 0, [](LPVOID lp) -> DWORD {
                Sleep(100);
                Shutdown();
                FreeConsole();
                FreeLibraryAndExitThread(static_cast<HMODULE>(lp), 0);
                return 0;
            }, static_cast<LPVOID>(mod), 0, nullptr);
        }

        return result;
    }

    static void CreateBackbufferRTV(IDXGISwapChain* pSwapChain) {
        if (g_BackbufferRTV) { g_BackbufferRTV->Release(); g_BackbufferRTV = nullptr; }
        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)))) {
            g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_BackbufferRTV);
            backBuffer->Release();
        }
    }

    static void RenderToBackbuffer(IDXGISwapChain* pSwapChain) {
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        if (desc.BufferDesc.Width == 0 || desc.BufferDesc.Height == 0)
            return;

        // Recreate RTV if needed (first time or resize)
        if (!g_BackbufferRTV)
            CreateBackbufferRTV(pSwapChain);
        else {
            D3D11_TEXTURE2D_DESC bbDesc;
            ID3D11Texture2D* bb = nullptr;
            g_BackbufferRTV->GetResource(reinterpret_cast<ID3D11Resource**>(&bb));
            if (bb) {
                bb->GetDesc(&bbDesc);
                if (bbDesc.Width != desc.BufferDesc.Width || bbDesc.Height != desc.BufferDesc.Height) {
                    CreateBackbufferRTV(pSwapChain);
                }
                bb->Release();
            }
        }

        if (!g_BackbufferRTV) return;

        // Save game state
        ID3D11RenderTargetView* oldRTVs[8] = {};
        ID3D11DepthStencilView* oldDSV = nullptr;
        UINT oldNumRTVs = 8;
        g_Context->OMGetRenderTargets(8, oldRTVs, &oldDSV);

        // Set our backbuffer RTV
        g_Context->OMSetRenderTargets(1, &g_BackbufferRTV, nullptr);

        // Insert key toggle
        static bool lastInsert = false;
        bool currentInsert = GetAsyncKeyState(VK_INSERT) & 0x8000;
        if (currentInsert && !lastInsert)
            g_Config.showMenu = !g_Config.showMenu;
        lastInsert = currentInsert;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        __try { if (g_Config.aimbotEnabled) g_Aimbot.Run(); } __except (EXCEPTION_EXECUTE_HANDLER) {}

        if (g_Config.showMenu) {
            ImGui::Begin("cs2 internal", &g_Config.showMenu, ImGuiWindowFlags_AlwaysAutoResize);

            if (ImGui::BeginTabBar("##tabs")) {
                if (ImGui::BeginTabItem("Aimbot")) {
                    ImGui::Checkbox("Enabled", &g_Config.aimbotEnabled);
                    const char* keys[] = { "Mouse1", "Mouse2", "Mouse3", "Shift", "Ctrl", "Alt", "X", "C", "V" };
                    int keyVals[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_SHIFT, VK_CONTROL, VK_MENU, 0x58, 0x43, 0x56 };
                    int keyIdx = 0;
                    for (int i = 0; i < IM_ARRAYSIZE(keyVals); i++) {
                        if (g_Config.aimbotKey == keyVals[i]) { keyIdx = i; break; }
                    }
                    if (ImGui::Combo("Key", &keyIdx, keys, IM_ARRAYSIZE(keys)))
                        g_Config.aimbotKey = keyVals[keyIdx];
                    ImGui::SliderFloat("FOV", &g_Config.aimbotFov, 1.f, 180.f, "%.1f");
                    ImGui::SliderFloat("Smooth", &g_Config.aimbotSmooth, 1.f, 20.f, "%.1f");
                    ImGui::Checkbox("Recoil Control", &g_Config.aimbotRecoilControl);
                    const char* bones[] = { "Head", "Neck", "Chest", "Pelvis" };
                    int boneIdx = (g_Config.aimbotBone == 6) ? 0 : (g_Config.aimbotBone == 5) ? 1 : (g_Config.aimbotBone == 4) ? 2 : 3;
                    ImGui::Combo("Bone", &boneIdx, bones, IM_ARRAYSIZE(bones));
                    int bonesMap[] = { 6, 5, 4, 0 };
                    g_Config.aimbotBone = bonesMap[boneIdx];
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Checkbox("Enabled", &g_Config.espEnabled);
                    ImGui::Checkbox("Box", &g_Config.espBox);
                    ImGui::Checkbox("Health", &g_Config.espHealth);
                    ImGui::Checkbox("Name", &g_Config.espName);
                    ImGui::Checkbox("Skeleton", &g_Config.espSkeleton);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("FOV")) {
                    ImGui::Checkbox("Enabled", &g_Config.fovEnabled);
                    ImGui::SliderInt("World FOV", &g_Config.fovValue, 70, 150);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Combat")) {
                    ImGui::Checkbox("Third Person", &g_Config.thirdPersonEnabled);
                    ImGui::SliderFloat("TP Distance", &g_Config.thirdPersonDistance, 50.f, 500.f, "%.0f");
                    ImGui::Separator();
                    ImGui::Checkbox("No Recoil", &g_Config.noRecoilEnabled);
                    ImGui::Separator();
                    ImGui::Checkbox("Bunny Hop", &g_Config.bunnyHopEnabled);
                    ImGui::Separator();
                    ImGui::Checkbox("No Flash", &g_Config.noFlash);
                    ImGui::Checkbox("No Smoke", &g_Config.noSmoke);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Chams")) {
                    ImGui::Checkbox("Enabled", &g_Config.chamsEnabled);
                    int matIdx = std::clamp(g_Config.chamsMaterial, 0, 7);
                    if (ImGui::Combo("Material", &matIdx, Chams::kMaterialNames, Chams::kMaterialCount))
                        g_Config.chamsMaterial = matIdx;
                    ImGui::SliderFloat("Glow Strength", &g_Config.chamsGlowStrength, 1.f, 20.f, "%.1f");
                    ImGui::ColorEdit4("Visible Color", g_Config.chamsVisColor, ImGuiColorEditFlags_NoInputs);
                    ImGui::ColorEdit4("Hidden Color", g_Config.chamsHidColor, ImGuiColorEditFlags_NoInputs);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::End();
        }

        if (g_Config.espEnabled || g_Config.aimbotEnabled) {
            if (g_Config.aimbotEnabled) {
                auto draw = ImGui::GetBackgroundDrawList();
                ImVec2 center(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
                float radius = tanf(g_Config.aimbotFov * 0.5f * 3.14159265f / 180.0f) * center.x;
                draw->AddCircle(center, radius, ImColor(255, 0, 0, 120), 64, 1.5f);
            }
        }

        __try {
            NoRecoil::Run();
            Bhop::Run();
            ThirdPerson::Run();
            FOVChanger::Run();

            // No Flash
            if (g_Config.noFlash) {
                uintptr_t localPawn = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
                if (localPawn) {
                    *(float*)(localPawn + schemas::C_CSPlayerPawnBase::m_flFlashDuration) = 0.f;
                    *(float*)(localPawn + schemas::C_CSPlayerPawnBase::m_flFlashMaxAlpha) = 0.f;
                    *(float*)(localPawn + schemas::C_CSPlayerPawnBase::m_flFlashOverlayAlpha) = 0.f;
                }
            }

            // No Smoke
            if (g_Config.noSmoke) {
                uintptr_t localPawn = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
                if (localPawn) {
                    __try { *(float*)(localPawn + schemas::C_CSPlayerPawnBase::m_flLastSmokeOverlayAlpha) = 0.f; } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
                // Disable smoke volumes via flat identity list
                uintptr_t es = ReadOffset<uintptr_t>(offsets::dwEntityList);
                if (es) {
                    __try {
                        uintptr_t list = *(uintptr_t*)(es + 0x10);
                        if (list) {
                            for (int i = 0; i < 2048; i++) {
                                uintptr_t id = *(uintptr_t*)(list + i * 8);
                                if (!id) continue;
                                uintptr_t ent = *(uintptr_t*)(id + 0);
                                if (!ent) continue;
                                bool tickBegin = *(int*)(ent + schemas::C_SmokeGrenadeProjectile::m_nSmokeEffectTickBegin) > 0;
                                bool didEffect = *(bool*)(ent + schemas::C_SmokeGrenadeProjectile::m_bDidSmokeEffect);
                                if (tickBegin && didEffect)
                                    *(bool*)(ent + schemas::C_SmokeGrenadeProjectile::m_bSmokeVolumeDataReceived) = false;
                            }
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }

            g_Chams.Run();
            if (g_Config.espEnabled) g_ESP.Render();
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Restore game state - release saved refs but keep original RTVs bound
        g_Context->OMSetRenderTargets(8, oldRTVs, oldDSV);
        for (UINT i = 0; i < 8; i++)
            if (oldRTVs[i]) oldRTVs[i]->Release();
        if (oldDSV) oldDSV->Release();
    }

    HWND GetProcessWindow() {
        const char* classes[] = { "Valvee001", "SDL_app", "CEF-OSC-WIDGET", nullptr };
        for (int i = 0; classes[i]; i++) {
            HWND h = FindWindowA(classes[i], nullptr);
            if (h) return h;
        }
        return FindWindowA(nullptr, "Counter-Strike 2");
    }
}
