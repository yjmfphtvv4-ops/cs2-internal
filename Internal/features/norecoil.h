#pragma once
#include "../globals.h"

// No-recoil is applied inside ThirdPerson's OverrideView hook.
// This namespace exists only for config access and menu consistency.
namespace NoRecoil {
    inline void Run() {
        // Handled in thirdperson.h DetourOverrideView
    }
}
