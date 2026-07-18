#include "fov.h"
#include "../globals.h"
#include "../sdk/offsets.h"
#include <cstdio>
#include <algorithm>

namespace FOVChanger {

    void Run() {
        if (!g_Config.fovEnabled) return;

        __try {
            uintptr_t localPawn = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
            if (!localPawn) return;
            // Don't override FOV while scoped (let scope zoom work)
            if (*(bool*)(localPawn + schemas::C_CSPlayerPawn::m_bIsScoped)) return;

            uintptr_t localCtrl = ReadOffset<uintptr_t>(offsets::dwLocalPlayerController);
            if (!localCtrl) return;

            uint32_t desired = static_cast<uint32_t>(std::clamp(g_Config.fovValue, 60, 150));

            // Force controller desired FOV
            *(uint32_t*)(localCtrl + schemas::CBasePlayerController::m_iDesiredFOV) = desired;

            // Force pawn camera services FOV fields
            uintptr_t camSvcs = *(uintptr_t*)(localPawn + schemas::C_BasePlayerPawn::m_pCameraServices);
            if (camSvcs) {
                *(uint32_t*)(camSvcs + schemas::CCSPlayerBase_CameraServices::m_iFOV) = desired;
                *(uint32_t*)(camSvcs + schemas::CCSPlayerBase_CameraServices::m_iFOVStart) = desired;
                *(float*)(camSvcs + schemas::CCSPlayerBase_CameraServices::m_flFOVRate) = 9999.f;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
