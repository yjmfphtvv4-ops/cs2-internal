#include "aimbot.h"
#include "../globals.h"
#include "../sdk/interfaces.h"
#include "../sdk/offsets.h"
#include "../sdk/structs.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>

// Bone array pointer offset within CModelState (0x80 = consistent across CS2 builds)
constexpr ptrdiff_t BONE_ARRAY_OFF = 0x80;

// CS2 bone indices: head=7, neck=6, chest=23, pelvis=1
constexpr int CS2_BONE_HEAD  = 7;
constexpr int CS2_BONE_NECK  = 6;
constexpr int CS2_BONE_CHEST = 23;
constexpr int CS2_BONE_PELVIS = 1;
constexpr int CS2_BONE_BODY  = 1;

struct BoneEntry { float px, py, pz, pad; float qx, qy, qz, qw; };

static Vector3 GetBonePos(uintptr_t pawn, int boneId) {
    uintptr_t node = *(uintptr_t*)(pawn + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!node) return {};
    uintptr_t arr = *(uintptr_t*)(node + schemas::CSkeletonInstance::m_modelState + BONE_ARRAY_OFF);
    if (!arr) return {};
    auto b = *(BoneEntry*)(arr + boneId * sizeof(BoneEntry));
    return { b.px, b.py, b.pz };
}

Vector3 Aimbot::GetBonePosition(uintptr_t pawn, int bone) {
    int csBone = (bone == 6) ? CS2_BONE_HEAD :
                 (bone == 5) ? CS2_BONE_NECK :
                 (bone == 4) ? CS2_BONE_CHEST : CS2_BONE_HEAD;
    __try {
        Vector3 pos = GetBonePos(pawn, csBone);
        if (pos.Length() >= 1.f) return pos;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return reinterpret_cast<C_CSPlayerPawn*>(pawn)->GetOrigin() + Vector3(0, 0, 64.f);
}

void Aimbot::Run() {
    if (!g_Config.aimbotEnabled) return;
    if (g_Config.showMenu) return;
    if (!(GetAsyncKeyState(g_Config.aimbotKey) & 0x8000)) return;

    auto entitySystem = CEntitySystem::GetInstance();
    if (!entitySystem) return;

    auto localPlayerPtr = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
    if (!localPlayerPtr) return;

    auto localPawn = reinterpret_cast<C_CSPlayerPawn*>(localPlayerPtr);
    if (!localPawn->IsAlive()) return;

    Vector3 localOrigin = localPawn->GetOrigin();
    Vector3 localEye = localOrigin + Vector3(0, 0, 64.f);

    int localTeam = localPawn->GetTeamNum();
    uintptr_t localVtable = *(uintptr_t*)localPlayerPtr;

    float bestFov = g_Config.aimbotFov;
    Vector3 bestTargetPos;
    bool foundTarget = false;

    auto localIdentity = *reinterpret_cast<CEntityIdentity**>(localPlayerPtr + 0x10);
    if (!localIdentity) return;

    auto head = localIdentity;
    while (auto prev = head->GetPrev()) head = prev;

    auto engineView = reinterpret_cast<QAngle*>(GetClientBase() + offsets::dwViewAngles);
    QAngle currentView = *engineView;

    float bestFovScore = bestFov;
    auto identity = head;
    while (identity) {
        auto entityPtr = identity->GetEntityPtr();
        if (entityPtr && entityPtr != localPlayerPtr && *(uintptr_t*)entityPtr == localVtable) {
            auto pawn = reinterpret_cast<C_CSPlayerPawn*>(entityPtr);
            if (pawn->IsAlive() && pawn->GetTeamNum() != localTeam) {
                Vector3 bonePos = GetBonePosition(entityPtr, g_Config.aimbotBone);
                if (bonePos.Length() >= 1.f) {
                    QAngle targetAngle = CalcAngle(localEye, bonePos);
                    float dp = targetAngle.pitch - currentView.pitch;
                    float dy = targetAngle.yaw - currentView.yaw;
                    while (dy > 180.f) dy -= 360.f;
                    while (dy < -180.f) dy += 360.f;
                    float fov = std::sqrt(dp * dp + dy * dy);
                    if (fov < bestFov && fov < bestFovScore) {
                        bestFovScore = fov;
                        bestTargetPos = bonePos;
                        foundTarget = true;
                    }
                }
            }
        }
        identity = identity->GetNext();
    }

    if (!foundTarget) return;

    // --- Humanized SendInput-only aimbot ---
    // No direct engineView write. Single movement signal only — SendInput.
    // Dual-write (engineView + SendInput) produces uncorrelated signals that
    // server-side eye velocity analysis and VAC pattern matching both flag.

    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    static std::uniform_real_distribution<float> jitterDist(-1.f, 1.f);
    static std::uniform_real_distribution<float> stepDist(0.82f, 1.18f);

    view_matrix_t vm = *reinterpret_cast<view_matrix_t*>(GetClientBase() + offsets::dwViewMatrix);
    float screenW = ImGui::GetIO().DisplaySize.x;
    float screenH = ImGui::GetIO().DisplaySize.y;
    Vector2 screenPos;
    if (!WorldToScreen(bestTargetPos, screenPos, vm, (int)screenW, (int)screenH)) return;

    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;
    float rawDx = screenPos.x - cx;
    float rawDy = screenPos.y - cy;

    // Recoil compensation applied in screen space
    if (g_Config.aimbotRecoilControl) {
        auto aimPunchSvcs = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(localPawn) + schemas::C_CSPlayerPawn::m_pAimPunchServices);
        if (aimPunchSvcs) {
            auto aimPunch = *reinterpret_cast<Vector3*>(aimPunchSvcs + schemas::CCSPlayer_AimPunchServices::m_predictableBaseAngle);
            rawDx -= aimPunch.y * 2.f;
            rawDy += aimPunch.x * 2.f;
        }
    }

    // Bezier-curved smooth: t driven by smooth factor, randomized step variance
    // t in (0,1] — larger smooth = smaller t per frame = slower approach
    float smooth  = std::clamp(g_Config.aimbotSmooth, 1.f, 25.f);
    float tBase   = 1.f / smooth;
    float tStep   = tBase * stepDist(rng);   // randomized per-frame step ±18%

    // Quadratic Bezier: P(t) = (1-t)^2 * P0 + 2(1-t)t * P1 + t^2 * P2
    // P0 = (0,0), P2 = (rawDx, rawDy), P1 = midpoint pulled toward center
    // Using a single-step approximation: move along the curve at tStep
    float bx = rawDx * (2.f * tStep - tStep * tStep);  // simplified B(t) for P0=0
    float by = rawDy * (2.f * tStep - tStep * tStep);

    // Per-axis jitter — magnitude scales with distance, disappears when close
    float dist    = std::sqrt(rawDx * rawDx + rawDy * rawDy);
    float jScale  = std::clamp(dist * 0.012f, 0.f, 1.4f) * g_Config.aimbotJitter;
    bx += jitterDist(rng) * jScale;
    by += jitterDist(rng) * jScale;

    int moveX = static_cast<int>(std::round(bx));
    int moveY = static_cast<int>(std::round(by));
    if (moveX == 0 && moveY == 0) return;

    INPUT input = {};
    input.type      = INPUT_MOUSE;
    input.mi.dx     = moveX;
    input.mi.dy     = moveY;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}
