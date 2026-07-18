#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <Psapi.h>
#include <cstdio>
#include <MinHook.h>
#include "../globals.h"
#include "../sdk/offsets.h"

struct CViewSetup {
    char pad_0x000[0x450];
    float flOrthoLeft, flOrthoTop, flOrthoRight, flOrthoBottom;
    char pad_0x460[0x38];
    float flFov, flFovViewmodel;
    float vecOrigin[3];
    char pad_0x4AC[0xC];
    float angView[3];
    char pad_0x4C4[0x14];
    float flAspectRatio, flFarZ, flNearZ;
    char pad_0x4E4[0x61];
    unsigned char nViewFlags;
};

namespace ThirdPerson {
    inline uintptr_t g_hookAddr = 0;
    using OverrideViewFn = void(__fastcall*)(__int64, CViewSetup*);
    inline OverrideViewFn g_original = nullptr;

    static void ZeroAimPunch() {
        __try {
            uintptr_t localPawn = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
            if (!localPawn) return;
            int health = *(int*)(localPawn + schemas::C_BaseEntity::m_iHealth);
            if (health <= 0) return;

            uintptr_t punchSvcs = *(uintptr_t*)(localPawn + schemas::C_CSPlayerPawn::m_pAimPunchServices);
            if (!punchSvcs) return;

            float* punch = (float*)(punchSvcs + schemas::CCSPlayer_AimPunchServices::m_predictableBaseAngle);
            punch[0] = 0.f; punch[1] = 0.f; punch[2] = 0.f;

            float* punchVel = (float*)(punchSvcs + schemas::CCSPlayer_AimPunchServices::m_predictableBaseAngleVel);
            punchVel[0] = 0.f; punchVel[1] = 0.f; punchVel[2] = 0.f;

            // Also zero camera-services view punch so OverrideView sees clean angles
            uintptr_t camSvcs = *(uintptr_t*)(localPawn + schemas::C_BasePlayerPawn::m_pCameraServices);
            if (camSvcs) {
                float* camPunch = (float*)(camSvcs + schemas::CPlayer_CameraServices::m_vecCsViewPunchAngle);
                camPunch[0] = 0.f; camPunch[1] = 0.f; camPunch[2] = 0.f;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void __fastcall DetourOverrideView(__int64 a1, CViewSetup* pSetup) {
        if (g_Config.noRecoilEnabled)
            ZeroAimPunch();

        if (g_original)
            g_original(a1, pSetup);

        if (!g_Config.thirdPersonEnabled || !pSetup) return;

        __try {
            float rxp = pSetup->angView[0] * 0.01745329251f;
            float ryw = pSetup->angView[1] * 0.01745329251f;
            float fx = cosf(rxp) * cosf(ryw);
            float fy = cosf(rxp) * sinf(ryw);
            float fz = -sinf(rxp);
            float d = g_Config.thirdPersonDistance;
            pSetup->vecOrigin[0] -= fx * d;
            pSetup->vecOrigin[1] -= fy * d;
            pSetup->vecOrigin[2] -= fz * d;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline uintptr_t FindPattern(const char* pattern) {
        HMODULE hMod = GetModuleHandleA("client.dll");
        if (!hMod) return 0;
        MODULEINFO mi;
        GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi));
        uintptr_t base = reinterpret_cast<uintptr_t>(hMod);
        uintptr_t size = mi.SizeOfImage;
        std::vector<int> bytes;
        std::vector<bool> mask;
        const char* p = pattern;
        while (*p) {
            if (*p == ' ') { p++; continue; }
            if (*p == '?') {
                bytes.push_back(0); mask.push_back(false); p++;
                if (*p == '?') p++;
            } else {
                char buf[3] = { p[0], p[1], 0 };
                bytes.push_back(strtol(buf, nullptr, 16));
                mask.push_back(true);
                p += 2;
            }
        }
        for (uintptr_t i = 0; i < size - bytes.size(); i++) {
            bool found = true;
            for (size_t j = 0; j < bytes.size() && found; j++)
                if (mask[j] && *(uint8_t*)(base + i + j) != bytes[j])
                    found = false;
            if (found) return base + i;
        }
        return 0;
    }

    inline void Install() {
        if (g_hookAddr) return;
        uintptr_t addr = FindPattern("40 57 48 83 EC ? 48 8B FA E8 ? ? ? ? BA");
        if (!addr) {
            printf("[TP] Pattern not found\n");
            return;
        }
        if (MH_CreateHook(reinterpret_cast<void*>(addr), &DetourOverrideView,
            reinterpret_cast<void**>(&g_original)) != MH_OK) return;
        if (MH_EnableHook(reinterpret_cast<void*>(addr)) != MH_OK) {
            MH_RemoveHook(reinterpret_cast<void*>(addr));
            return;
        }
        g_hookAddr = addr;
        printf("[TP] Hook installed at 0x%llX\n", addr);
    }

    inline void WriteFlag(bool enabled) {
        __try {
            uintptr_t input = ReadOffset<uintptr_t>(offsets::dwCSGOInput);
            if (input)
                *(bool*)(input + 0xA51) = enabled;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void Shutdown() {
        if (g_hookAddr) {
            MH_DisableHook(reinterpret_cast<void*>(g_hookAddr));
            MH_RemoveHook(reinterpret_cast<void*>(g_hookAddr));
            g_hookAddr = 0;
            g_original = nullptr;
        }
    }

    inline void Run() {
        bool needHook = g_Config.thirdPersonEnabled || g_Config.noRecoilEnabled;
        if (needHook) {
            if (!g_hookAddr) Install();
            WriteFlag(g_Config.thirdPersonEnabled);
        } else if (g_hookAddr) {
            WriteFlag(false);
            Shutdown();
        }
    }
}
