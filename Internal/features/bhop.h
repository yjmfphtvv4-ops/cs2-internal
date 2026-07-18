#pragma once
#include <cstdint>
#include "../globals.h"
#include "../sdk/offsets.h"

namespace Bhop {
    inline void Run() {
        if (!g_Config.bunnyHopEnabled) return;
        if (!(GetAsyncKeyState(VK_SPACE) & 0x8000)) return;

        __try {
            uintptr_t localPawn = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
            if (!localPawn) return;
            int health = *(int*)(localPawn + schemas::C_BaseEntity::m_iHealth);
            if (health <= 0) return;

            uint32_t flags = *(uint32_t*)(localPawn + schemas::C_BaseEntity::m_fFlags);
            uintptr_t jumpAddr = GetClientBase() + buttons::jump;

            if (flags & 1) { // on ground
                *(int*)jumpAddr = 65537; // press jump
            } else {
                *(int*)jumpAddr = 256; // release jump
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
