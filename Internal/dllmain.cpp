#include <Windows.h>
#include <thread>
#include <cstdio>
#include <iostream>
#include "hooks/present.h"
#include "features/fov.h"
#include "globals.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

DWORD WINAPI InitThread(LPVOID lpParam) {
    AllocConsole();
    freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
    freopen_s(reinterpret_cast<FILE**>(stdin), "CONIN$", "r", stdin);
    SetConsoleTitleA("cs2 internal debug");

    printf("[+] DLL loaded\n");

    for (int tries = 0; tries < 150; tries++) {
        if (PresentHook::GetProcessWindow()) {
            printf("[+] CS2 window found\n");
            break;
        }
        printf("[-] Waiting for CS2 window (%d/150)...\n", tries + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (!PresentHook::GetProcessWindow()) {
        printf("[-] Timed out waiting for CS2 window\n");
        return 1;
    }

    if (MH_Initialize() != MH_OK) {
        printf("[-] MH_Initialize failed\n");
        return 1;
    }
    printf("[+] MinHook initialized\n");

    if (!PresentHook::Initialize()) {
        printf("[-] PresentHook::Initialize failed\n");
        MH_Uninitialize();
        return 1;
    }
    printf("[+] Hook installed, waiting for Present call...\n");

    while (!GetAsyncKeyState(VK_END)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    printf("[+] END pressed, shutting down\n");
    PresentHook::Shutdown();
    MH_Uninitialize();
    FreeConsole();
    FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReason, LPVOID lpReserved) {
    if (ulReason == DLL_PROCESS_ATTACH) {
        g_DllModule = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, InitThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
