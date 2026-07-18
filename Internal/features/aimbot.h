#pragma once
#include "../sdk/structs.h"

class Aimbot {
public:
    void Run();
    Vector3 GetBonePosition(uintptr_t pawn, int bone);
};
