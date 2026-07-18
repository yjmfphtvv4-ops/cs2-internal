#pragma once
#include "../sdk/structs.h"
#include "imgui.h"

class ESP {
public:
    void Render();
    void Box(const Vector2& top, const Vector2& bot, ImColor color);
    void HealthBar(const Vector2& top, const Vector2& bot, int health);
    void Name(const Vector2& top, const char* name, ImColor color);
    void Skeleton(uintptr_t pawnPtr, const view_matrix_t& viewMatrix, int sw, int sh);
};
