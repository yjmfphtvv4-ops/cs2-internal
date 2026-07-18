#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include "../sdk/structs.h"

class Chams {
public:
    static constexpr int kMaterialCount = 8;
    static constexpr const char* kMaterialNames[kMaterialCount] = {
        "Flat", "Glow", "Outline", "Glass", "Metallic", "GlowBurn", "Latex", "GlassFX"
    };

    struct Config {
        bool enabled = true;
        bool xray = true;
        int materialIndex = 2;
        float glowStrength = 6.0f;
        float visibleColor[4] = { 72.0f / 255.0f, 97.0f / 255.0f, 100.0f / 255.0f, 1.0f };
        float hiddenColor[4] = { 105.0f / 255.0f, 75.0f / 255.0f, 150.0f / 255.0f, 225.0f / 255.0f };
    };

    Chams();
    ~Chams();

    void Initialize();
    void Shutdown();
    void Run();

    Config& GetConfig() { return m_config; }

private:
    Config m_config;
    bool m_initialized = false;

    struct CMeshData {
        uint8_t pad0[0x18];
        uintptr_t sceneObject;
        uintptr_t material;
        uintptr_t material2;
        uint8_t pad1[0x38];
    };

    struct KV3ID_t { const char* name; uint64_t u0; uint64_t u1; };
    struct ResBind_t { void* data; };
    template<typename T> struct CStrongHandle { ResBind_t* binding = nullptr; };
    struct CMaterial2 {};

    using DrawObjectFn = bool(__fastcall*)(void*, void*, CMeshData*, int, void*, void*, void*, void*);
    using LoadKV3Fn = bool(__fastcall*)(void*, void*, const char*, const KV3ID_t*, const KV3ID_t*, unsigned int);
    using CreateMatFn = void(__fastcall*)(void*, CStrongHandle<CMaterial2>*, const char*, void*, unsigned int, unsigned int);
    using SetTypeKV3Fn = void*(__fastcall*)(void*, unsigned int, unsigned int);

    DrawObjectFn m_original = nullptr;
    uintptr_t m_hookAddress = 0;

    LoadKV3Fn m_loadKV3 = nullptr;
    CreateMatFn m_createMat = nullptr;
    SetTypeKV3Fn m_setTypeKV3 = nullptr;
    void* m_materialSystem = nullptr;

    struct MatPair { CMaterial2* vis = nullptr; CMaterial2* hid = nullptr; };
    MatPair m_currentMat{};
    bool m_matsReady = false;
    int m_cachedMatIdx = -1;
    float m_cachedVis[4]{};
    float m_cachedHid[4]{};
    float m_cachedGlowStrength = -1.0f;
    int m_matGeneration = 0;

    struct PawnEntry {
        uintptr_t pawn = 0;
        uintptr_t sceneRoot = 0;
        Vector3 origin{};
    };
    PawnEntry m_pawnCache[64]{};
    int m_pawnCount = 0;
    uintptr_t m_localPawn = 0;

    static Chams* s_instance;

    static bool __fastcall DetourDrawObject(void* a, void* b, CMeshData* md, int cnt, void* sv, void* sl, void* u1, void* u2);
    uintptr_t GetPlayerPawn(CMeshData* md);

    void RefreshPawnCache();
    uintptr_t MatchPawnBySceneObject(uintptr_t sceneObject);
    std::string BuildVmat(int matIdx, const float color[4], bool hidden);

    bool ResolveFunctions();
    CMaterial2* CreateMaterial(const char* name, const std::string& vmat);
    void EnsureMaterials();
    uintptr_t FindPatternRaw(const char* moduleName, const char* pattern);
    uintptr_t FindPatternMsk(const char* moduleName, const char* pattern);
};
