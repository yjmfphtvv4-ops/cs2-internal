#include "chams.h"
#include "../globals.h"
#include "../sdk/interfaces.h"
#include "../sdk/offsets.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <Psapi.h>
#include <MinHook.h>

Chams* Chams::s_instance = nullptr;

Chams::Chams() {
    s_instance = this;
}

Chams::~Chams() {
    Shutdown();
    s_instance = nullptr;
}

uintptr_t Chams::FindPatternRaw(const char* moduleName, const char* pattern) {
    HMODULE hMod = GetModuleHandleA(moduleName);
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
            bytes.push_back(0);
            mask.push_back(false);
            p++;
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
        for (size_t j = 0; j < bytes.size() && found; j++) {
            if (mask[j] && *(uint8_t*)(base + i + j) != bytes[j])
                found = false;
        }
        if (found) return base + i;
    }
    return 0;
}

// Alternate pattern scanning: mask-based (e.g. "48 8B C4 53 57 41 54 48 81 EC D0 00 00 00 49 63 ...")
uintptr_t Chams::FindPatternMsk(const char* moduleName, const char* pattern) {
    return FindPatternRaw(moduleName, pattern);
}

static constexpr const char* kSigDrawObj =
    "48 8B C4 53 57 41 54 48 81 EC D0 00 00 00 49 63";

static constexpr const char* kSigCreateMat =
    "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 8B F2";

static constexpr const char* kSigSetTypeKV3 =
    "40 53 48 83 EC ? 80 FA";

static constexpr const char* kLoadKV3Export =
    "?LoadKV3@@YA_NPEAVKeyValues3@@PEAVCUtlString@@PEBDAEBUKV3ID_t@@2I@Z";

bool Chams::ResolveFunctions() {
    if (m_loadKV3 && m_createMat && m_setTypeKV3 && m_materialSystem) return true;

    HMODULE hTier0 = GetModuleHandleA("tier0.dll");
    HMODULE hMatSys = GetModuleHandleA("materialsystem2.dll");
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hTier0 || !hMatSys) return false;

    m_loadKV3 = reinterpret_cast<LoadKV3Fn>(GetProcAddress(hTier0, kLoadKV3Export));
    if (!m_loadKV3) {
        uintptr_t loadAddr = FindPatternRaw("tier0.dll",
            "48 89 5C 24 08 57 48 83 EC 70 4C 8B D1 48 C7 C0 FF FF FF FF 48 FF C0 41 80 3C 00 00 75 F6");
        if (loadAddr) m_loadKV3 = reinterpret_cast<LoadKV3Fn>(loadAddr);
    }

    uintptr_t createAddr = FindPatternRaw("materialsystem2.dll", kSigCreateMat);
    if (createAddr) m_createMat = reinterpret_cast<CreateMatFn>(createAddr);

    if (hClient) {
        uintptr_t setAddr = FindPatternRaw("client.dll", kSigSetTypeKV3);
        if (setAddr) m_setTypeKV3 = reinterpret_cast<SetTypeKV3Fn>(setAddr);
    }

    using CIFn = void*(__cdecl*)(const char*, int*);
    auto ci = reinterpret_cast<CIFn>(GetProcAddress(hMatSys, "CreateInterface"));
    if (ci) m_materialSystem = ci("VMaterialSystem2_001", nullptr);

    printf("[Chams] Resolve: LoadKV3=%p CreateMat=%p SetTypeKV3=%p MatSys=%p\n",
        m_loadKV3, m_createMat, m_setTypeKV3, m_materialSystem);

    return m_loadKV3 && m_createMat && m_setTypeKV3 && m_materialSystem;
}

std::string Chams::BuildVmat(int matIdx, const float color[4], bool hidden) {
    char body[3800]{};
    float r = std::clamp(color[0], 0.f, 1.f);
    float g = std::clamp(color[1], 0.f, 1.f);
    float b = std::clamp(color[2], 0.f, 1.f);
    float a = std::clamp(color[3], 0.f, 1.f);

    const char* zBuf = hidden
        ? "    F_DISABLE_Z_BUFFERING = 1\n    F_DISABLE_Z_PREPASS = 1\n    F_DISABLE_Z_WRITE = 1\n"
        : "";

    switch (matIdx) {
    case 0:
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
%s    g_vColorTint = [%f, %f, %f, %f]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
})", zBuf, r, g, b, a);
        break;
    case 1: {
        float gstr = std::clamp(m_config.glowStrength, 1.f, 20.f);
        float scale = gstr * 0.75f;
        float bright = gstr;
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_ADDITIVE_BLEND = 1
%s    g_vColorTint = [%f, %f, %f, %f]
    g_vSelfIllumTint = [%f, %f, %f, %f]
    g_flSelfIllumScale = [%.1f, %.1f, %.1f, %.1f]
    g_flSelfIllumBrightness = [%.1f, %.1f, %.1f, %.1f]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})", zBuf, r, g, b, a, r, g, b, a, scale, scale, scale, scale, bright, bright, bright, bright);
        break;
    }
    case 2:
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_ADDITIVE_BLEND = 1
    F_WIREFRAME = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [%f, %f, %f, %f]
    g_vSelfIllumTint = [%f, %f, %f, %f]
    g_flSelfIllumScale = [2.0, 2.0, 2.0, 2.0]
    g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})", r, g, b, a, r, g, b, a);
        break;
    case 3:
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_ADDITIVE_BLEND = 1
%s    g_vColorTint = [%f, %f, %f, %f]
    g_vSelfIllumTint = [%f, %f, %f, %f]
    g_flSelfIllumScale = [0.5, 0.5, 0.5, 0.5]
    g_flSelfIllumBrightness = [2.0, 2.0, 2.0, 2.0]
    g_flMetalness = 1.0
    g_flRoughness = 0.2
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})", zBuf, r, g, b, a, r, g, b, a);
        break;
    case 4:
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_complex.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
%s    g_vColorTint = [%f, %f, %f, %f]
    g_flMetalness = 1.0
    g_flRoughness = 0.18
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
})", zBuf, r, g, b, a);
        break;
    case 5: {
        float gstr = std::clamp(m_config.glowStrength, 1.f, 20.f);
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_ADDITIVE_BLEND = 1
%s    g_vColorTint = [%f, %f, %f, %f]
    g_vSelfIllumTint = [%f, %f, %f, %f]
    g_flSelfIllumScale = [%.1f, %.1f, %.1f, %.1f]
    g_flSelfIllumBrightness = [%.1f, %.1f, %.1f, %.1f]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})", zBuf, r, g, b, a, r * 1.5f, g * 1.5f, b * 1.5f, a,
            gstr * 1.5f, gstr * 1.5f, gstr * 1.5f, gstr * 1.5f,
            gstr * 2.0f, gstr * 2.0f, gstr * 2.0f, gstr * 2.0f);
        break;
    }
    case 6:
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_character.vfx"
    F_BLEND_MODE = 1
%s    g_vColorTint = [%f, %f, %f, %f]
    g_bFogEnabled = 0
    g_flMetalness = 0.000
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})", zBuf, r, g, b, a);
        break;
    case 7:
        std::snprintf(body, sizeof(body), R"({
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = %d
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [%f, %f, %f, %f]
})", hidden ? 1 : 0, r, g, b, a);
        break;
    }

    char buf[4096]{};
    std::snprintf(buf, sizeof(buf),
        "<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n%s",
        body);
    return std::string(buf);
}

Chams::CMaterial2* Chams::CreateMaterial(const char* name, const std::string& vmat) {
    if (!ResolveFunctions()) return nullptr;

    void* raw = std::malloc(0x200);
    if (!raw) return nullptr;
    std::memset(raw, 0, 0x200);

    void* kv3 = m_setTypeKV3(raw, 1U, 6U);
    if (!kv3) { std::free(raw); return nullptr; }

    KV3ID_t id{ "generic", 0x41B818518343427EULL, 0xB5F447C23C0CDF8CULL };
    if (!m_loadKV3(kv3, nullptr, vmat.c_str(), &id, nullptr, 0U)) {
        std::free(raw);
        return nullptr;
    }

    CStrongHandle<CMaterial2> h{};
    m_createMat(m_materialSystem, &h, name, kv3, 0U, 1U);
    std::free(raw);

    return h.binding ? static_cast<CMaterial2*>(h.binding->data) : nullptr;
}

void Chams::EnsureMaterials() {
    int matIdx = std::clamp(m_config.materialIndex, 0, kMaterialCount - 1);
    float gs = m_config.glowStrength;

    if (m_matsReady && m_cachedMatIdx == matIdx) {
        if (memcmp(m_cachedVis, m_config.visibleColor, sizeof(float) * 4) == 0 &&
            memcmp(m_cachedHid, m_config.hiddenColor, sizeof(float) * 4) == 0 &&
            (matIdx != 1 || fabsf(m_cachedGlowStrength - gs) < 0.05f))
            return;
    }

    if (!ResolveFunctions()) return;

    ++m_matGeneration;
    char visName[64], hidName[64];
    std::snprintf(visName, sizeof(visName), "materials/dev/chams_vis_%d.vmat", m_matGeneration);
    std::snprintf(hidName, sizeof(hidName), "materials/dev/chams_hid_%d.vmat", m_matGeneration);

    CMaterial2* newVis = CreateMaterial(visName, BuildVmat(matIdx, m_config.visibleColor, false));
    CMaterial2* newHid = CreateMaterial(hidName, BuildVmat(matIdx, m_config.hiddenColor, true));

    if (!newVis) {
        printf("[Chams] Failed to create visible material\n");
        return;
    }

    m_currentMat.vis = newVis;
    m_currentMat.hid = newHid;
    m_cachedMatIdx = matIdx;
    m_cachedGlowStrength = gs;
    memcpy(m_cachedVis, m_config.visibleColor, sizeof(float) * 4);
    memcpy(m_cachedHid, m_config.hiddenColor, sizeof(float) * 4);
    m_matsReady = true;

        // materials rebuilt
}

void Chams::RefreshPawnCache() {
    m_pawnCount = 0;
    m_localPawn = 0;

    uintptr_t localPawn = ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
    if (!localPawn) return;

    uintptr_t entitySystem = ReadOffset<uintptr_t>(offsets::dwEntityList);
    if (!entitySystem) return;

    m_localPawn = localPawn;
    int localTeam = 0;
    __try {
        localTeam = *(int*)(localPawn + schemas::C_BaseEntity::m_iTeamNum);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    auto addPawn = [&](uintptr_t pawn) {
        if (!pawn || m_pawnCount >= 64) return;
        for (int i = 0; i < m_pawnCount; i++)
            if (m_pawnCache[i].pawn == pawn) return;

        int health = 0;
        uint8_t life = 0;
        uint8_t team = 0;
        __try {
            health = *(int*)(pawn + schemas::C_BaseEntity::m_iHealth);
            life = *(uint8_t*)(pawn + schemas::C_BaseEntity::m_lifeState);
            team = *(uint8_t*)(pawn + schemas::C_BaseEntity::m_iTeamNum);
        } __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        if (team != 2 && team != 3) return;

        PawnEntry& e = m_pawnCache[m_pawnCount++];
        e.pawn = pawn;
        __try {
            e.sceneRoot = *(uintptr_t*)(pawn + schemas::C_BaseEntity::m_pGameSceneNode);
            e.origin = *(Vector3*)(pawn + schemas::C_BasePlayerPawn::m_vOldOrigin);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    };

    for (int i = 1; i <= 64; i++) {
        uint32_t chunkIdx = i >> 9;
        uint32_t entryIdx = i & 0x1FF;
        uintptr_t listEntry = 0;
        __try {
            listEntry = *(uintptr_t*)(entitySystem + 0x8 * chunkIdx + 0x10);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (!listEntry) continue;

        uintptr_t controller = 0;
        __try {
            controller = *(uintptr_t*)(listEntry + 0x70 * entryIdx);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (!controller) continue;

        uint32_t pawnHandle = 0;
        __try {
            pawnHandle = *(uint32_t*)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (!pawnHandle) continue;

        uint32_t idx = pawnHandle & 0x7FFF;
        uint32_t ck = idx >> 9;
        uint32_t ei = idx & 0x1FF;
        uintptr_t pawnLE = 0;
        __try {
            pawnLE = *(uintptr_t*)(entitySystem + 0x8 * ck + 0x10);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (!pawnLE) continue;

        uintptr_t pawn = 0;
        __try {
            pawn = *(uintptr_t*)(pawnLE + 0x70 * ei);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (pawn) addPawn(pawn);
    }
    addPawn(localPawn);
    if (m_pawnCount == 0) return;
}

uintptr_t Chams::MatchPawnBySceneObject(uintptr_t sceneObject) {
    // Method 1: Read entity handles from the scene object
    static constexpr uintptr_t kHandleOffsets[] = {
        0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0
    };

    __try {
        for (uintptr_t off : kHandleOffsets) {
            uint32_t handle = *(uint32_t*)(sceneObject + off);
            if (!handle || handle == 0xFFFFFFFF) continue;

            uint32_t idx = handle & 0x7FFF;
            uint32_t ck = idx >> 9;
            uint32_t ei = idx & 0x1FF;
            uintptr_t entitySystem = ReadOffset<uintptr_t>(offsets::dwEntityList);
            if (!entitySystem) continue;

            uintptr_t le = *(uintptr_t*)(entitySystem + 0x8 * ck + 0x10);
            if (!le) continue;
            uintptr_t ent = *(uintptr_t*)(le + 0x70 * ei);
            if (!ent) continue;

            for (int i = 0; i < m_pawnCount; i++) {
                if (m_pawnCache[i].pawn == ent) return ent;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // Method 2: Direct scene root match (child -> parent walk)
    __try {
        for (int i = 0; i < m_pawnCount; i++) {
            auto& e = m_pawnCache[i];
            if (!e.sceneRoot) continue;
            // Walk the scene node tree from root looking for this sceneObject
            uintptr_t child = e.sceneRoot;
            while (child) {
                if (child == sceneObject) return e.pawn;
                child = *(uintptr_t*)(child + 0x48); // next sibling
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    return 0;
}

uintptr_t Chams::GetPlayerPawn(CMeshData* md) {
    if (!md) return 0;
    uintptr_t so = md->sceneObject;
    if (!so || so < 0x10000) return 0;

    // Method 1: Match by scene node handles
    uintptr_t pawn = MatchPawnBySceneObject(so);

    // Method 2: Fallback by player material + distance
    if (!pawn) {
        const char* matName = nullptr;
        __try {
            auto* vtbl = *(uintptr_t**)md->material;
            if (vtbl) matName = ((const char*(__fastcall*)(void*))vtbl[0])((void*)md->material);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        if (matName &&
            (strstr(matName, "characters/models") ||
             strstr(matName, "models/player") ||
             strstr(matName, "custom_player") ||
             strstr(matName, "/ctm_") ||
             strstr(matName, "/tm_") ||
             strstr(matName, "models/ctm_") ||
             strstr(matName, "models/tm_") ||
             strstr(matName, "player_model") ||
             strstr(matName, "/arms/") ||
             strstr(matName, "/gloves/") ||
             strstr(matName, "/body/") ||
             strstr(matName, "/head") ||
             strstr(matName, "_head_") ||
             strstr(matName, "_body_"))) {
            Vector3 soPos{};
            __try { soPos = *(Vector3*)(so + 0x5C); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (soPos.x != 0 || soPos.y != 0 || soPos.z != 0) {
                uintptr_t best = 0;
                float bestDist = 19200.f * 19200.f;
                for (int i = 0; i < m_pawnCount; i++) {
                    auto& e = m_pawnCache[i];
                    if (!e.pawn) continue;
                    float dx = soPos.x - e.origin.x;
                    float dy = soPos.y - e.origin.y;
                    float dz = soPos.z - e.origin.z;
                    float dist = dx * dx + dy * dy + dz * dz;
                    if (dist < bestDist) { bestDist = dist; best = e.pawn; }
                }
                if (best) pawn = best;
            }
        }
    }

    return pawn;
}

bool __fastcall Chams::DetourDrawObject(void* a, void* b, CMeshData* md, int cnt,
    void* sv, void* sl, void* u1, void* u2)
{
    Chams* self = s_instance;
    if (!self || !self->m_original)
        return false;

    if (!md || cnt <= 0 || cnt > 256)
        return self->m_original(a, b, md, cnt, sv, sl, u1, u2);

    if (!self->m_config.enabled || !self->m_matsReady || !self->m_currentMat.vis)
        return self->m_original(a, b, md, cnt, sv, sl, u1, u2);

    if (!md->sceneObject || md->sceneObject < 0x10000)
        return self->m_original(a, b, md, cnt, sv, sl, u1, u2);

    uintptr_t pawn = 0;
    __try { pawn = self->GetPlayerPawn(md); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!pawn)
        return self->m_original(a, b, md, cnt, sv, sl, u1, u2);

    // Skip teammates
    int pawnTeam = 0, localTeam = 0;
    __try {
        pawnTeam = *(int*)(pawn + schemas::C_BaseEntity::m_iTeamNum);
        if (self->m_localPawn)
            localTeam = *(int*)(self->m_localPawn + schemas::C_BaseEntity::m_iTeamNum);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (pawnTeam != 2 && pawnTeam != 3)
        return self->m_original(a, b, md, cnt, sv, sl, u1, u2);
    if (pawnTeam == localTeam)
        return self->m_original(a, b, md, cnt, sv, sl, u1, u2);

    struct SavedMat { uintptr_t m, m2; };
    SavedMat saved[256];
    for (int i = 0; i < cnt && i < 256; i++) {
        saved[i].m = md[i].material;
        saved[i].m2 = md[i].material2;
    }

    bool restored = false;
    __try {
        // Always apply xray (hidden material) first
        if (self->m_currentMat.hid) {
            for (int i = 0; i < cnt; i++)
                md[i].material = md[i].material2 = reinterpret_cast<uintptr_t>(self->m_currentMat.hid);
            self->m_original(a, b, md, cnt, sv, sl, u1, u2);
        }

        for (int i = 0; i < cnt; i++)
            md[i].material = md[i].material2 = reinterpret_cast<uintptr_t>(self->m_currentMat.vis);

        bool result = self->m_original(a, b, md, cnt, sv, sl, u1, u2);

        for (int i = 0; i < cnt; i++) {
            md[i].material = saved[i].m;
            md[i].material2 = saved[i].m2;
        }
        restored = true;
        return result;

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (!restored)
            for (int i = 0; i < cnt; i++) {
                md[i].material = saved[i].m;
                md[i].material2 = saved[i].m2;
            }
        return self->m_original(a, b, md, cnt, sv, sl, u1, u2);
    }
}

void Chams::Initialize() {
    if (m_hookAddress) return;

    printf("[Chams] Initializing...\n");

    HMODULE hScene = GetModuleHandleA("scenesystem.dll");
    if (!hScene) {
        printf("[Chams] scenesystem.dll not loaded\n");
        return;
    }

    uintptr_t drawObj = FindPatternRaw("scenesystem.dll", kSigDrawObj);
    if (!drawObj) {
        printf("[Chams] DrawObject pattern not found\n");
        return;
    }

    printf("[Chams] DrawObject at 0x%llX\n", drawObj);

    MH_STATUS st = MH_CreateHook(
        reinterpret_cast<void*>(drawObj),
        reinterpret_cast<void*>(&DetourDrawObject),
        reinterpret_cast<void**>(&m_original));
    if (st != MH_OK) {
        printf("[Chams] MH_CreateHook failed: %d\n", st);
        return;
    }

    st = MH_EnableHook(reinterpret_cast<void*>(drawObj));
    if (st != MH_OK) {
        printf("[Chams] MH_EnableHook failed: %d\n", st);
        MH_RemoveHook(reinterpret_cast<void*>(drawObj));
        return;
    }

    m_hookAddress = drawObj;
    m_initialized = true;
    printf("[Chams] Hook installed\n");
}

void Chams::Shutdown() {
    if (m_hookAddress) {
        MH_DisableHook(reinterpret_cast<void*>(m_hookAddress));
        MH_RemoveHook(reinterpret_cast<void*>(m_hookAddress));
        m_hookAddress = 0;
        m_original = nullptr;
    }
    m_currentMat = {};
    m_matsReady = false;
    m_cachedMatIdx = -1;
    m_cachedGlowStrength = -1.f;
    m_pawnCount = 0;
    m_localPawn = 0;
    m_initialized = false;
}

void Chams::Run() {
    // Sync from global config
    m_config.enabled = g_Config.chamsEnabled;
    m_config.xray = g_Config.chamsXray;
    m_config.materialIndex = std::clamp(g_Config.chamsMaterial, 0, kMaterialCount - 1);
    m_config.glowStrength = g_Config.chamsGlowStrength;
    memcpy(m_config.visibleColor, g_Config.chamsVisColor, sizeof(float) * 4);
    memcpy(m_config.hiddenColor, g_Config.chamsHidColor, sizeof(float) * 4);

    if (!m_config.enabled) {
        if (m_hookAddress)
            MH_DisableHook(reinterpret_cast<void*>(m_hookAddress));
        return;
    }

    if (!m_hookAddress)
        Initialize();

    if (!m_hookAddress)
        return;

    MH_EnableHook(reinterpret_cast<void*>(m_hookAddress));
    RefreshPawnCache();
    EnsureMaterials();
}
