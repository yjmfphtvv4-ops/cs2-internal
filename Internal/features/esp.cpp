#include "esp.h"
#include "../globals.h"
#include "../sdk/interfaces.h"
#include "../sdk/offsets.h"
#include <algorithm>
#include <cstdio>

void ESP::Box(const Vector2& top, const Vector2& bot, ImColor color) {
    float h = bot.y - top.y;
    float w = h * 0.5f;
    float x = top.x - w / 2.f;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddRect(ImVec2(x, top.y), ImVec2(x + w, bot.y), color);
    draw->AddRect(ImVec2(x - 1, top.y - 1), ImVec2(x + w + 1, bot.y + 1), ImColor(0, 0, 0, 180));
}

void ESP::HealthBar(const Vector2& top, const Vector2& bot, int health) {
    float h = bot.y - top.y;
    float w = h * 0.5f;
    float x = top.x - w / 2.f;
    float barWidth = 4.f;
    float barX = x - barWidth - 2.f;
    float fillH = (h - 4.f) * (std::clamp(health, 0, 100) / 100.f);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddRectFilled(ImVec2(barX, top.y), ImVec2(barX + barWidth, bot.y), ImColor(0, 0, 0, 200));
    draw->AddRectFilled(ImVec2(barX + 1, bot.y - 1 - fillH), ImVec2(barX + barWidth - 1, bot.y - 1),
        ImColor(std::min(255, (100 - health) * 5), std::min(255, health * 5), 0));
}

static const char* GetPlayerName(uintptr_t pawnPtr, CEntityIdentity* listHead, uintptr_t localVtable) {
    uintptr_t pawnIdentity = *(uintptr_t*)(pawnPtr + 0x10);
    auto list = *(uintptr_t*)(*(uintptr_t*)(CEntitySystem::GetInstance()) + 0x10);
    int pawnIndex = -1;
    for (int i = 0; i < 64; i++) {
        if (*(uintptr_t*)(list + i * 8) == pawnIdentity) { pawnIndex = i; break; }
    }
    if (pawnIndex < 0) return nullptr;

    auto identity = listHead;
    while (identity) {
        auto ep = identity->GetEntityPtr();
        if (ep && *(uintptr_t*)ep != localVtable) {
            auto handle = *reinterpret_cast<uint32_t*>(ep + 0x914);
            if ((handle & 0x7FFF) == pawnIndex) {
                const char* name = *(const char**)(ep + 0x868);
                if (name && name[0]) return name;
            }
        }
        identity = identity->GetNext();
    }
    return nullptr;
}

void ESP::Name(const Vector2& top, const char* name, ImColor color) {
    const char* text = name ? name : "Unknown";
    auto draw = ImGui::GetBackgroundDrawList();
    auto textSize = ImGui::CalcTextSize(text);
    draw->AddText(ImVec2(top.x - textSize.x / 2.f, top.y - textSize.y - 2.f), color, text);
}

#include <excpt.h>

// CS2 bone indices
enum BoneIndex : int {
    BONE_PELVIS        = 2,
    BONE_STOMACH       = 3,
    BONE_SPINE         = 4,
    BONE_NECK          = 5,
    BONE_HEAD          = 6,
    BONE_LEFT_SHOULDER = 8,
    BONE_LEFT_ELBOW    = 9,
    BONE_LEFT_HAND     = 10,
    BONE_RIGHT_SHOULDER= 13,
    BONE_RIGHT_ELBOW   = 14,
    BONE_RIGHT_HAND    = 15,
    BONE_LEFT_HIP      = 17,
    BONE_LEFT_KNEE     = 18,
    BONE_LEFT_FOOT     = 19,
    BONE_RIGHT_HIP     = 20,
    BONE_RIGHT_KNEE    = 21,
    BONE_RIGHT_FOOT    = 22,
    BONE_HEAD_TOP      = 7,
};

static const int BONE_PAIRS[][2] = {
    {BONE_HEAD_TOP, BONE_HEAD},
    {BONE_HEAD, BONE_NECK},
    {BONE_NECK, BONE_SPINE},
    {BONE_SPINE, BONE_STOMACH},
    {BONE_STOMACH, BONE_PELVIS},
    {BONE_SPINE, BONE_LEFT_SHOULDER},
    {BONE_LEFT_SHOULDER, BONE_LEFT_ELBOW},
    {BONE_LEFT_ELBOW, BONE_LEFT_HAND},
    {BONE_SPINE, BONE_RIGHT_SHOULDER},
    {BONE_RIGHT_SHOULDER, BONE_RIGHT_ELBOW},
    {BONE_RIGHT_ELBOW, BONE_RIGHT_HAND},
    {BONE_PELVIS, BONE_LEFT_HIP},
    {BONE_LEFT_HIP, BONE_LEFT_KNEE},
    {BONE_LEFT_KNEE, BONE_LEFT_FOOT},
    {BONE_PELVIS, BONE_RIGHT_HIP},
    {BONE_RIGHT_HIP, BONE_RIGHT_KNEE},
    {BONE_RIGHT_KNEE, BONE_RIGHT_FOOT},
};

// Bone array: read from (sceneNode + m_modelState) + 0x80
// Each bone entry is 32 bytes; position is Vector3 at offset 0
static constexpr size_t BONE_ARRAY_OFFSET = 0x80;
static constexpr size_t BONE_STRIDE = 32;

static Vector3 ReadBonePos(uintptr_t boneArray, int boneIndex) {
    Vector3 pos = {0, 0, 0};
    __try {
        pos = *reinterpret_cast<Vector3*>(boneArray + boneIndex * BONE_STRIDE);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return pos;
}

void ESP::Skeleton(uintptr_t pawnPtr, const view_matrix_t& viewMatrix, int sw, int sh) {
    uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(pawnPtr + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!sceneNode) return;

    uintptr_t modelState = sceneNode + schemas::CSkeletonInstance::m_modelState;
    uintptr_t boneArray = *reinterpret_cast<uintptr_t*>(modelState + BONE_ARRAY_OFFSET);
    if (!boneArray || boneArray < 0x100000) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImColor color(255, 255, 255, 200);

    Vector2 p1, p2;
    for (auto& pair : BONE_PAIRS) {
        Vector3 from = ReadBonePos(boneArray, pair[0]);
        Vector3 to   = ReadBonePos(boneArray, pair[1]);
        if (from.x == 0 && from.y == 0 && from.z == 0) continue;
        if (to.x == 0 && to.y == 0 && to.z == 0) continue;
        if (!WorldToScreen(from, p1, viewMatrix, sw, sh)) continue;
        if (!WorldToScreen(to,   p2, viewMatrix, sw, sh)) continue;
        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 1.5f);
    }
}

void ESP::Render() {
    if (!g_Config.espEnabled) return;

    auto entitySystem = CEntitySystem::GetInstance();
    if (!entitySystem) return;

    auto localPlayerPtr = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
    if (!localPlayerPtr) return;

    int localTeam = reinterpret_cast<C_CSPlayerPawn*>(localPlayerPtr)->GetTeamNum();
    uintptr_t localVtable = *(uintptr_t*)localPlayerPtr;

    auto viewMatrix = ReadOffset<view_matrix_t>(offsets::dwViewMatrix);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    auto localIdentity = *reinterpret_cast<CEntityIdentity**>(localPlayerPtr + 0x10);
    if (!localIdentity) return;

    auto head = localIdentity;
    while (auto prev = head->GetPrev()) head = prev;

    auto identity = head;
    while (identity) {
        auto entityPtr = identity->GetEntityPtr();
        if (entityPtr && entityPtr != localPlayerPtr && *(uintptr_t*)entityPtr == localVtable) {
            auto pawn = reinterpret_cast<C_CSPlayerPawn*>(entityPtr);
            if (pawn->IsAlive() && pawn->GetTeamNum() != localTeam) {
                Vector3 origin = pawn->GetOrigin();
                Vector3 headPos = origin + Vector3(0, 0, 72.f);
                Vector2 screenTop, screenBot;

                if (WorldToScreen(headPos, screenTop, viewMatrix, sw, sh) &&
                    WorldToScreen(origin, screenBot, viewMatrix, sw, sh)) {
                    if (g_Config.espSkeleton) Skeleton(entityPtr, viewMatrix, sw, sh);
                    if (g_Config.espBox) Box(screenTop, screenBot, ImColor(255, 0, 0, 255));
                    if (g_Config.espHealth) HealthBar(screenTop, screenBot, pawn->GetHealth());
                    if (g_Config.espName) {
                        const char* name = GetPlayerName(entityPtr, head, localVtable);
                        Name(screenTop, name ? name : "Enemy", ImColor(255, 0, 0, 255));
                    }
                }
            }
        }
        identity = identity->GetNext();
    }
}
