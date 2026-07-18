#include "skin_changer.h"
#include "skin_offsets.h"
#include "../features/game.h"

#include <Windows.h>
#include <unordered_map>
#include <cstring>
#include <cmath>
#include <cstdio>

namespace SkinChanger {

bool thirdPerson = false;

// Game memory allocator functions from tier0.dll
static void* (__fastcall* g_GameAlloc)(size_t) = nullptr;
static void  (__fastcall* g_GameFree)(void*)   = nullptr;

static bool InitGameAllocator() {
    if (g_GameAlloc && g_GameFree)
        return true;

    HMODULE tier0 = GetModuleHandleA("tier0.dll");
    if (!tier0)
        return false;

    g_GameAlloc = reinterpret_cast<decltype(g_GameAlloc)>(
        GetProcAddress(tier0, "MemAlloc_AllocFunc")
    );
    g_GameFree = reinterpret_cast<decltype(g_GameFree)>(
        GetProcAddress(tier0, "MemAlloc_FreeFunc")
    );

    return g_GameAlloc && g_GameFree;
}

// Attribute structures
struct EconItemAttribute {
    char     pad_0000[0x30];
    uint16_t def_index;
    char     pad_0032[2];
    float    value;
    float    init_value;
    int32_t  refundable_currency;
    bool     set_bonus;
    char     pad_0041[7];
};

struct AttributeVector {
    uint64_t  size;
    uintptr_t ptr;
};

enum EconItemAttributeType : uint16_t {
    ATTR_PAINT_KIT = 6,
    ATTR_PAINT_KIT_SEED = 7,
    ATTR_PAINT_KIT_WEAR = 8,
};

// Safe attribute creation using game's allocator
static void CreateAttributes(uintptr_t item, int paintKit, float wear, int seed) {
    if (!item || paintKit <= 0)
        return;

    if (!InitGameAllocator()) {
        OutputDebugStringA("[SkinChanger] Failed to initialize game allocator\n");
        return;
    }

    __try {
        uintptr_t attrList = item + skin_offsets::m_AttributeList;
        
        if (IsBadReadPtr(reinterpret_cast<void*>(attrList), sizeof(AttributeVector))) {
            OutputDebugStringA("[SkinChanger] Invalid attribute list pointer\n");
            return;
        }

        AttributeVector* vec = reinterpret_cast<AttributeVector*>(
            attrList + skin_offsets::m_Attributes
        );

        if (!vec) {
            OutputDebugStringA("[SkinChanger] Invalid attribute vector\n");
            return;
        }

        // CRITICAL: Always remove old attributes first when switching knives/weapons
        if (vec->size != 0 || vec->ptr != 0) {
            void* oldPtr = reinterpret_cast<void*>(vec->ptr);
            vec->size = 0;
            vec->ptr = 0;
            if (oldPtr && g_GameFree) {
                g_GameFree(oldPtr);
            }
        }

        constexpr size_t attrCount = 3;
        EconItemAttribute* attrs = static_cast<EconItemAttribute*>(
            g_GameAlloc(attrCount * sizeof(EconItemAttribute))
        );

        if (!attrs) {
            OutputDebugStringA("[SkinChanger] Failed to allocate attributes\n");
            return;
        }

        memset(attrs, 0, attrCount * sizeof(EconItemAttribute));

        // Paint kit attribute
        attrs[0].def_index = ATTR_PAINT_KIT;
        attrs[0].value = static_cast<float>(paintKit);
        attrs[0].init_value = attrs[0].value;

        // Seed attribute
        attrs[1].def_index = ATTR_PAINT_KIT_SEED;
        attrs[1].value = static_cast<float>(seed >= 0 ? seed : 0);
        attrs[1].init_value = attrs[1].value;

        // Wear attribute
        attrs[2].def_index = ATTR_PAINT_KIT_WEAR;
        attrs[2].value = wear >= 0.0f ? wear : 0.01f;
        attrs[2].init_value = attrs[2].value;

        vec->size = attrCount;
        vec->ptr = reinterpret_cast<uintptr_t>(attrs);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringA("[SkinChanger] Exception in CreateAttributes\n");
    }
}

// Safe attribute removal using game's allocator
static void RemoveAttributes(uintptr_t item) {
    if (!item)
        return;

    if (!InitGameAllocator())
        return;

    __try {
        uintptr_t attrList = item + skin_offsets::m_AttributeList;
        
        if (IsBadReadPtr(reinterpret_cast<void*>(attrList), sizeof(AttributeVector))) {
            return;
        }

        AttributeVector* vec = reinterpret_cast<AttributeVector*>(
            attrList + skin_offsets::m_Attributes
        );

        if (!vec || vec->size == 0)
            return;

        void* ptr = reinterpret_cast<void*>(vec->ptr);
        vec->size = 0;
        vec->ptr = 0;

        if (ptr && g_GameFree) {
            g_GameFree(ptr);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringA("[SkinChanger] Exception in RemoveAttributes\n");
    }
}

struct KnifeDef {
    int id;
    uint32_t subclassHash;
    const char* modelPath;
};

static const KnifeDef kKnives[] = {
    { 500, 3933374535u, "weapons/models/knife/knife_bayonet/weapon_knife_bayonet.vmdl" },
    { 503, 3787235507u, "weapons/models/knife/knife_css/weapon_knife_css.vmdl" },
    { 505, 4046390180u, "weapons/models/knife/knife_flip/weapon_knife_flip.vmdl" },
    { 506, 2047704618u, "weapons/models/knife/knife_gut/weapon_knife_gut.vmdl" },
    { 507, 1731408398u, "weapons/models/knife/knife_karambit/weapon_knife_karambit.vmdl" },
    { 508, 1638561588u, "weapons/models/knife/knife_m9/weapon_knife_m9.vmdl" },
    { 509, 2282479884u, "weapons/models/knife/knife_tactical/weapon_knife_tactical.vmdl" },
    { 512, 3412259219u, "weapons/models/knife/knife_falchion/weapon_knife_falchion.vmdl" },
    { 514, 2511498851u, "weapons/models/knife/knife_bowie/weapon_knife_bowie.vmdl" },
    { 515, 1353709123u, "weapons/models/knife/knife_butterfly/weapon_knife_butterfly.vmdl" },
    { 516, 4269888884u, "weapons/models/knife/knife_push/weapon_knife_push.vmdl" },
    { 517, 1105782941u, "weapons/models/knife/knife_cord/weapon_knife_cord.vmdl" },
    { 518, 275962944u,  "weapons/models/knife/knife_canis/weapon_knife_canis.vmdl" },
    { 519, 1338637359u, "weapons/models/knife/knife_ursus/weapon_knife_ursus.vmdl" },
    { 520, 3230445913u, "weapons/models/knife/knife_navaja/weapon_knife_navaja.vmdl" },
    { 521, 3206681373u, "weapons/models/knife/knife_outdoor/weapon_knife_outdoor.vmdl" },
    { 522, 2595277776u, "weapons/models/knife/knife_stiletto/weapon_knife_stiletto.vmdl" },
    { 523, 4029975521u, "weapons/models/knife/knife_talon/weapon_knife_talon.vmdl" },
    { 525, 365028728u,  "weapons/models/knife/knife_skeleton/weapon_knife_skeleton.vmdl" },
};

static WeaponSlotConfig g_config;
static bool g_initialized = false;

// crash prevention cache
static uintptr_t g_lastRifleWeapon = 0;
static bool g_forceUpdate = true;
static uintptr_t g_lastViewmodelPawn = 0;
static int g_lastViewmodelKnife = -1;
static int g_lastHealth = 100;
static DWORD g_lastRegenerateTime = 0;

using UpdateSubclassFn = void(__fastcall*)(void* entity);
using SetMeshGroupMaskFn = void(__fastcall*)(void* sceneNode, uint64_t mask);
using SetModelFn = void(__fastcall*)(void* entity, const char* model);
using ApplyEconCustomizationFn = void(__fastcall*)(void* weapon, int unk);
using RegenerateWeaponSkinsFn = void(__fastcall*)();

static UpdateSubclassFn g_UpdateSubclass = nullptr;
static SetMeshGroupMaskFn g_SetMeshGroupMask = nullptr;
static SetModelFn g_SetModel = nullptr;
static ApplyEconCustomizationFn g_ApplyEconCustomization = nullptr;
static RegenerateWeaponSkinsFn g_RegenerateWeaponSkins = nullptr;

// Virtual function helpers
template<typename T, typename... Args>
static T CallVirtual(void* thisptr, int index, Args... args) {
    if (!thisptr) return T{};
    void** vtable = *reinterpret_cast<void***>(thisptr);
    if (!vtable) return T{};
    auto fn = reinterpret_cast<T(__fastcall*)(void*, Args...)>(vtable[index]);
    if (!fn) return T{};
    return fn(thisptr, args...);
}

static uint32_t GetKnifeSubclassHash(int knifeId) {
    for (const KnifeDef& k : kKnives) {
        if (k.id == knifeId)
            return k.subclassHash;
    }
    return kKnives[0].subclassHash;
}

static const char* GetKnifeModelPath(int knifeId) {
    for (const KnifeDef& k : kKnives) {
        if (k.id == knifeId)
            return k.modelPath;
    }
    return kKnives[0].modelPath;
}

static uint64_t GetKnifeMeshGroupMask(int knifeId) {
    if (knifeId == 42 || knifeId == 59)
        return 1;
    return 2;
}

static bool IsKnifeDefIndex(uint16_t defIndex) {
    return defIndex == 42 || defIndex == 59 || (defIndex >= 500 && defIndex < 600);
}

static void ApplyMeshAndModel(uintptr_t entity, int knifeId) {
    if (!entity)
        return;

    const uint64_t meshMask = GetKnifeMeshGroupMask(knifeId);
    const char* modelPath = GetKnifeModelPath(knifeId);

    __try {
        uintptr_t renderComponent = Game::Read<uintptr_t>(entity + skin_offsets::m_CRenderComponent);
        if (renderComponent) {
            if (!IsBadWritePtr(reinterpret_cast<void*>(renderComponent + skin_offsets::m_MeshGroupMask), sizeof(uint64_t))) {
                Game::Write<uint64_t>(renderComponent + skin_offsets::m_MeshGroupMask, meshMask);
            }
        }
        if (g_SetModel && modelPath) {
            g_SetModel(reinterpret_cast<void*>(entity), modelPath);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void ApplyViewmodelKnife(uintptr_t pawn, int knifeId) {
    if (!pawn)
        return;

    if (pawn == g_lastViewmodelPawn && knifeId == g_lastViewmodelKnife && !g_forceUpdate)
        return;

    g_lastViewmodelPawn = pawn;
    g_lastViewmodelKnife = knifeId;

    uint32_t armsHandle = Game::Read<uint32_t>(pawn + skin_offsets::m_hHudModelArms);
    uintptr_t arms = Game::GetEntityByHandle(armsHandle);
    if (!arms)
        return;

    uintptr_t armsNode = Game::Read<uintptr_t>(arms + skin_offsets::m_pGameSceneNode);
    if (!armsNode)
        return;

    uintptr_t childNode = Game::Read<uintptr_t>(armsNode + skin_offsets::m_pChild);
    if (!childNode)
        return;

    uintptr_t childOwner = Game::Read<uintptr_t>(childNode + skin_offsets::m_pOwner);
    if (!childOwner)
        return;

    ApplyMeshAndModel(childOwner, knifeId);

    if (g_UpdateSubclass) {
        __try {
            g_UpdateSubclass(reinterpret_cast<void*>(childOwner));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

static void ApplyKnifeSkin(uintptr_t weapon, const SkinConfig& config) {
    if (!weapon || !config.enabled)
        return;

        uintptr_t attrMgr = Game::Read<uintptr_t>(weapon + skin_offsets::m_AttributeManager);
        if (!attrMgr) return;
        uintptr_t item = attrMgr + skin_offsets::m_Item;

    RemoveAttributes(item);

    Game::Write<int32_t>(item + skin_offsets::m_iItemIDHigh, -1);
    Game::Write<uint32_t>(item + skin_offsets::m_iItemIDLow, 0);
    Game::Write<uint32_t>(item + skin_offsets::m_iAccountID, 0);

    Game::Write<uint16_t>(item + skin_offsets::m_iItemDefinitionIndex, 
        static_cast<uint16_t>(config.itemDefinitionIndex));

    Game::Write<int32_t>(item + skin_offsets::m_iEntityQuality, 3);
    Game::Write<uint32_t>(item + skin_offsets::m_iEntityLevel, 1);
    Game::Write<int32_t>(item + skin_offsets::m_iRarityOverride, 7);

    CreateAttributes(item, config.paintKit, config.wear, config.seed);

    Game::Write<int32_t>(weapon + skin_offsets::m_nFallbackPaintKit, config.paintKit);
    Game::Write<int32_t>(weapon + skin_offsets::m_nFallbackSeed, config.seed);
    Game::Write<float>(weapon + skin_offsets::m_flFallbackWear, config.wear);
    Game::Write<int32_t>(weapon + skin_offsets::m_nFallbackStatTrak, config.statTrak);

    if (config.customName[0] != '\0') {
        for (int i = 0; i < 32; ++i) {
            Game::Write<char>(item + skin_offsets::m_szCustomName + i, 0);
        }
        for (int i = 0; i < 31; ++i) {
            Game::Write<char>(item + skin_offsets::m_szCustomName + i, config.customName[i]);
        }
    }

    Game::Write<uint32_t>(weapon + skin_offsets::m_nSubclassID, 
        GetKnifeSubclassHash(config.itemDefinitionIndex));

    ApplyMeshAndModel(weapon, config.itemDefinitionIndex);

    if (g_UpdateSubclass) {
        __try {
            g_UpdateSubclass(reinterpret_cast<void*>(weapon));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    __try {
        CallVirtual<void>(reinterpret_cast<void*>(weapon), 110, true);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        CallVirtual<void*>(reinterpret_cast<void*>(weapon), 195);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    Game::Write<uintptr_t>(item + 0x1D0, 0);

    uintptr_t pawn = Game::GetLocalPlayerPawn();
    ApplyViewmodelKnife(pawn, config.itemDefinitionIndex);
}

static void ApplyWeaponSkin(uintptr_t weapon, const SkinConfig& config) {
    if (!weapon || !config.enabled)
        return;

        uintptr_t attrMgr = Game::Read<uintptr_t>(weapon + skin_offsets::m_AttributeManager);
        if (!attrMgr) return;
        uintptr_t item = attrMgr + skin_offsets::m_Item;

    __try {
        if (IsBadReadPtr(reinterpret_cast<void*>(item), 0x200))
            return;

        RemoveAttributes(item);

        Game::Write<int32_t>(item + skin_offsets::m_iItemIDHigh, -1);
        Game::Write<uint32_t>(item + skin_offsets::m_iItemIDLow, 0);
        Game::Write<uint32_t>(item + skin_offsets::m_iAccountID, 0);

        Game::Write<int32_t>(item + skin_offsets::m_iEntityQuality, 3);
        Game::Write<uint32_t>(item + skin_offsets::m_iEntityLevel, 1);
        Game::Write<int32_t>(item + skin_offsets::m_iRarityOverride, 7);

        CreateAttributes(item, config.paintKit, config.wear, config.seed);

        Game::Write<int32_t>(weapon + skin_offsets::m_nFallbackPaintKit, config.paintKit);
        Game::Write<int32_t>(weapon + skin_offsets::m_nFallbackSeed, config.seed);
        Game::Write<float>(weapon + skin_offsets::m_flFallbackWear, config.wear);
        Game::Write<int32_t>(weapon + skin_offsets::m_nFallbackStatTrak, config.statTrak);

        if (g_UpdateSubclass) {
            __try {
                g_UpdateSubclass(reinterpret_cast<void*>(weapon));
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        __try {
            CallVirtual<void>(reinterpret_cast<void*>(weapon), 110, true);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try {
            CallVirtual<void*>(reinterpret_cast<void*>(weapon), 195);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try {
            if (g_ApplyEconCustomization) {
                g_ApplyEconCustomization(reinterpret_cast<void*>(weapon), 1);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try {
            int32_t currentEventId = Game::Read<int32_t>(weapon + skin_offsets::m_nCustomEconReloadEventId);
            Game::Write<int32_t>(weapon + skin_offsets::m_nCustomEconReloadEventId, currentEventId + 1);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

        __try {
            if (g_RegenerateWeaponSkins) {
                g_RegenerateWeaponSkins();
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void ResolvePatterns() {
    if (!g_SetMeshGroupMask) {
        g_SetMeshGroupMask = reinterpret_cast<SetMeshGroupMaskFn>(
            Game::FindPattern(L"client.dll",
                "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8D 99 ? ? ? ? 48 8B 71")
        );
    }

    if (!g_UpdateSubclass) {
        uintptr_t addr = Game::FindPattern(L"client.dll",
            "40 53 48 83 EC 30 48 8B 41 10 48 8B D9 8B 50 30");

        if (!addr) {
            addr = Game::FindPattern(L"client.dll",
                "4C 8B DC 53 48 81 EC ? ? ? ? 48 8B 41");
        }

        g_UpdateSubclass = reinterpret_cast<UpdateSubclassFn>(addr);
    }

    if (!g_SetModel) {
        g_SetModel = reinterpret_cast<SetModelFn>(
            Game::FindPattern(L"client.dll",
                "40 53 48 83 EC ? 48 8B D9 4C 8B C2 48 8B 0D ? ? ? ? 48 8D 54 24 40")
        );
    }

    if (!g_RegenerateWeaponSkins) {
        g_RegenerateWeaponSkins = reinterpret_cast<RegenerateWeaponSkinsFn>(
            Game::FindPattern(L"client.dll",
                "48 83 EC ? E8 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 48 8B 10")
        );
    }

    if (!g_ApplyEconCustomization) {
        g_ApplyEconCustomization = reinterpret_cast<ApplyEconCustomizationFn>(
            Game::FindPattern(L"client.dll",
                "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 54 41 55 41 56 41 57 48 8B EC")
        );
        
        if (!g_ApplyEconCustomization) {
            g_ApplyEconCustomization = reinterpret_cast<ApplyEconCustomizationFn>(
                Game::FindPattern(L"client.dll",
                    "40 53 48 83 EC ? 48 8B D9 48 8B 0D ? ? ? ? 45 8B C8")
            );
        }
    }
}

static bool ApplyToAllKnives() {
    // Section 1: pawn + weapon services
    __try {
        uintptr_t pawn = Game::GetLocalPlayerPawn();
        if (!pawn) {
            static bool once = false; if (!once) { once = true; printf("[Skin] GetLocalPlayerPawn returned null\n"); }
            return false;
        }

        int health = Game::Read<int>(pawn + 0x34C);
        if (g_lastHealth <= 0 && health > 0) {
            g_forceUpdate = true;
            g_lastViewmodelPawn = 0;
            g_lastViewmodelKnife = -1;
            g_lastRifleWeapon = 0;
        }
        g_lastHealth = health;
        if (health <= 0) {
            g_lastRifleWeapon = 0;
            g_lastViewmodelPawn = 0;
            g_lastViewmodelKnife = -1;
            g_forceUpdate = true;
            return false;
        }

        uint8_t lifeState = Game::Read<uint8_t>(pawn + 0x354);
        if (lifeState != 0) {
            static bool once = false; if (!once) { once = true; printf("[Skin] pawn lifeState != 0 (%d)\n", lifeState); }
            g_lastRifleWeapon = 0; g_lastViewmodelPawn = 0; g_lastViewmodelKnife = -1; g_forceUpdate = true;
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static bool once = false; if (!once) { once = true; printf("[Skin] SEH-1 (pawn)\n"); }
        return false;
    }

    // Section 2: weapon list
    __try {
        uintptr_t weaponServices = Game::Read<uintptr_t>(Game::GetLocalPlayerPawn() + skin_offsets::m_pWeaponServices);
        if (!weaponServices) {
            static bool once = false; if (!once) { once = true; printf("[Skin] weaponServices null\n"); }
            return false;
        }

        // Debug: check active weapon
        { uint32_t activeH = Game::Read<uint32_t>(weaponServices + 0x60);
          if (activeH != 0 && activeH != 0xFFFFFFFF) { static uint32_t lastAH = 0; if (activeH != lastAH) { lastAH = activeH;
              uintptr_t activeW = Game::GetEntityByHandle(activeH);
              if (activeW) {
                   uintptr_t attrMgr = Game::Read<uintptr_t>(activeW + skin_offsets::m_AttributeManager);
                   uintptr_t item = attrMgr ? attrMgr + skin_offsets::m_Item : 0;
                  uint16_t activeDef = Game::Read<uint16_t>(item + skin_offsets::m_iItemDefinitionIndex);
                  printf("[Skin] activeWeapon handle=0x%X defIndex=%u\n", activeH, activeDef);
              }
          }}
        }

        uintptr_t myWeaponsVector = weaponServices + 0x48;
        uint32_t weaponCount = Game::Read<uint32_t>(myWeaponsVector);
        uintptr_t weaponArray = Game::Read<uintptr_t>(myWeaponsVector + 0x8);

        if (weaponCount > 64 || weaponCount == 0) {
            static bool once = false; if (!once) { once = true; printf("[Skin] weaponCount=%u (bad)\n", weaponCount); }
            return false;
        }
        if (!weaponArray) {
            static bool once = false; if (!once) { once = true; printf("[Skin] weaponArray null\n"); }
            return false;
        }

        bool appliedAny = false;
        static int last_weapon_crash = -1;
        static int last_weapon_log[8] = {-1,-1,-1,-1,-1,-1,-1,-1};

        // Section 3: per-weapon loop
        for (uint32_t i = 0; i < weaponCount; i++) {
            __try {
                uint32_t weaponHandle = Game::Read<uint32_t>(weaponArray + (i * 4));
                if (weaponHandle == 0 || weaponHandle == 0xFFFFFFFF)
                    continue;

                { static bool once = false; if (!once) { once = true;
                    auto* es = CEntitySystem::GetInstance();
                    printf("[Skin] w[%u] handle=0x%X entSys=0x%llX\n", i, weaponHandle, (uintptr_t)es);
                }}
                uintptr_t weapon = Game::GetEntityByHandle(weaponHandle);
                if (!weapon)
                    continue;

                uintptr_t attrMgr = Game::Read<uintptr_t>(weapon + skin_offsets::m_AttributeManager);
                if (!attrMgr) continue;
                uintptr_t item = attrMgr + skin_offsets::m_Item;
                uint16_t defIndex = Game::Read<uint16_t>(item + skin_offsets::m_iItemDefinitionIndex);
                if (i < 8 && defIndex != last_weapon_log[i]) { last_weapon_log[i] = defIndex; printf("[Skin] weapon[%u] defIndex=%u\n", i, defIndex); }

                if (IsKnifeDefIndex(defIndex)) {
                    if (!g_config.knife.enabled) continue;

                    int currentPaintKit = Game::Read<int>(weapon + skin_offsets::m_nFallbackPaintKit);
                    bool needsUpdate = (
                        defIndex != static_cast<uint16_t>(g_config.knife.itemDefinitionIndex) ||
                        currentPaintKit != g_config.knife.paintKit ||
                        g_forceUpdate
                    );
                    if (needsUpdate) {
                        ApplyKnifeSkin(weapon, g_config.knife);
                        appliedAny = true;
                    }
                } else {
                    if (!g_config.rifle.enabled) continue;
                    if (defIndex != static_cast<uint16_t>(g_config.rifle.itemDefinitionIndex)) continue;

                    int currentPaintKit = Game::Read<int>(weapon + skin_offsets::m_nFallbackPaintKit);
                    bool needsUpdate = (
                        currentPaintKit != g_config.rifle.paintKit ||
                        weapon != g_lastRifleWeapon ||
                        g_forceUpdate
                    );
                    if (needsUpdate) {
                        ApplyWeaponSkin(weapon, g_config.rifle);
                        g_lastRifleWeapon = weapon;
                        appliedAny = true;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                if ((int)i != last_weapon_crash) { last_weapon_crash = (int)i; printf("[Skin] SEH-LOOP at weapon[%u]\n", i); }
            }
        }

        if (appliedAny && g_RegenerateWeaponSkins) {
            DWORD currentTime = GetTickCount();
            if (currentTime - g_lastRegenerateTime > 500) {
                __try { g_RegenerateWeaponSkins(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                g_lastRegenerateTime = currentTime;
            }
        }

        g_forceUpdate = false;
        { static int last_applied = -1; if ((int)appliedAny != last_applied) { last_applied = appliedAny; printf("[Skin] %s\n", appliedAny ? "APPLIED" : "no weapons matched"); } }
        return appliedAny;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static bool once = false; if (!once) { once = true; printf("[Skin] SEH-2 (weapon list)\n"); }
        return false;
    }
}

void Initialize() {
    if (g_initialized)
        return;

    __try {
        ResolvePatterns();
        static bool pdbg = false; if (!pdbg) { pdbg = true;
            printf("[Skin] Patterns: SetMG=%llX UpdateSub=%llX SetModel=%llX RegSkin=%llX ApplyEcon=%llX\n",
                (uintptr_t)g_SetMeshGroupMask, (uintptr_t)g_UpdateSubclass,
                (uintptr_t)g_SetModel, (uintptr_t)g_RegenerateWeaponSkins,
                (uintptr_t)g_ApplyEconCustomization);
            printf("[Skin] Allocator: %s\n", InitGameAllocator() ? "OK" : "FAILED");
        }
        InitGameAllocator();
        g_initialized = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        g_initialized = false;
    }
}

void ApplySkins() {
    g_forceUpdate = true; // force the update on next loop run

    if (!g_initialized) {
        __try {
            Initialize();
            if (!g_initialized) {
                return;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
    }

    __try {
        uintptr_t pawn = Game::GetLocalPlayerPawn();
        if (!pawn) {
            return;
        }
        { static bool once = false; if (!once) { once = true; printf("[Skin] ApplySkins running, pawn=0x%llX\n", pawn); } }
        { static int lastK = -1, lastR = -1, lastKI = -1, lastRI = -1;
            int nk = g_config.knife.enabled ? g_config.knife.itemDefinitionIndex : 0;
            int nr = g_config.rifle.enabled ? g_config.rifle.itemDefinitionIndex : 0;
            int ek = g_config.knife.enabled;
            int er = g_config.rifle.enabled;
            if (ek != lastK || er != lastR || nk != lastKI || nr != lastRI) {
                lastK = ek; lastR = er; lastKI = nk; lastRI = nr;
                printf("[Skin] state: knife.en=%d idx=%d | rifle.en=%d idx=%d\n", ek, nk, er, nr);
            }
        }
        ApplyToAllKnives();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        static bool once = false; if (!once) { once = true; printf("[Skin] SEH exception in ApplySkins\n"); }
    }
}

void ForceWeaponUpdate(uintptr_t weapon) {
    if (!weapon)
        return;

    __try {
        CallVirtual<void>(reinterpret_cast<void*>(weapon), 110, true);
        CallVirtual<void*>(reinterpret_cast<void*>(weapon), 195);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

WeaponSlotConfig& GetConfig() {
    return g_config;
}

const char* GetWeaponName(int defIndex) {
    switch (defIndex) {
        case 1: return "Desert Eagle";
        case 2: return "Dual Berettas";
        case 3: return "Five-SeveN";
        case 4: return "Glock-18";
        case 7: return "AK-47";
        case 8: return "AUG";
        case 9: return "AWP";
        case 10: return "FAMAS";
        case 11: return "G3SG1";
        case 13: return "Galil AR";
        case 14: return "M249";
        case 16: return "M4A4";
        case 17: return "MAC-10";
        case 19: return "P90";
        case 23: return "MP5-SD";
        case 24: return "UMP-45";
        case 25: return "XM1014";
        case 26: return "PP-Bizon";
        case 27: return "MAG-7";
        case 28: return "Negev";
        case 29: return "Sawed-Off";
        case 30: return "Tec-9";
        case 31: return "Zeus x27";
        case 32: return "P2000";
        case 33: return "MP7";
        case 34: return "MP9";
        case 35: return "Nova";
        case 36: return "P250";
        case 38: return "SCAR-20";
        case 39: return "SG 553";
        case 40: return "SSG 08";
        case 60: return "M4A1-S";
        case 61: return "USP-S";
        case 63: return "CZ75-Auto";
        case 64: return "R8 Revolver";
        default: return "Unknown";
    }
}

const char* GetKnifeName(KnifeType knife) {
    switch (knife) {
        case KnifeType::BAYONET: return "Bayonet";
        case KnifeType::FLIP: return "Flip Knife";
        case KnifeType::GUT: return "Gut Knife";
        case KnifeType::KARAMBIT: return "Karambit";
        case KnifeType::M9_BAYONET: return "M9 Bayonet";
        case KnifeType::HUNTSMAN: return "Huntsman Knife";
        case KnifeType::FALCHION: return "Falchion Knife";
        case KnifeType::BOWIE: return "Bowie Knife";
        case KnifeType::BUTTERFLY: return "Butterfly Knife";
        case KnifeType::SHADOW_DAGGERS: return "Shadow Daggers";
        case KnifeType::PARACORD: return "Paracord Knife";
        case KnifeType::SURVIVAL: return "Survival Knife";
        case KnifeType::URSUS: return "Ursus Knife";
        case KnifeType::NAVAJA: return "Navaja Knife";
        case KnifeType::NOMAD: return "Nomad Knife";
        case KnifeType::STILETTO: return "Stiletto Knife";
        case KnifeType::TALON: return "Talon Knife";
        case KnifeType::CLASSIC: return "Classic Knife";
        case KnifeType::SKELETON: return "Skeleton Knife";
        default: return "Unknown Knife";
    }
}

const char* GetPaintKitName(PaintKit kit) {
    switch (kit) {
        case PaintKit::DOPPLER_RUBY: return "Doppler Ruby";
        case PaintKit::DOPPLER_SAPPHIRE: return "Doppler Sapphire";
        case PaintKit::DOPPLER_BLACK_PEARL: return "Doppler Black Pearl";
        case PaintKit::DOPPLER_PHASE1: return "Doppler Phase 1";
        case PaintKit::DOPPLER_PHASE2: return "Doppler Phase 2";
        case PaintKit::DOPPLER_PHASE3: return "Doppler Phase 3";
        case PaintKit::DOPPLER_PHASE4: return "Doppler Phase 4";
        case PaintKit::GAMMA_DOPPLER_EMERALD: return "Gamma Doppler Emerald";
        case PaintKit::GAMMA_DOPPLER_PHASE1: return "Gamma Doppler Phase 1";
        case PaintKit::GAMMA_DOPPLER_PHASE2: return "Gamma Doppler Phase 2";
        case PaintKit::GAMMA_DOPPLER_PHASE3: return "Gamma Doppler Phase 3";
        case PaintKit::GAMMA_DOPPLER_PHASE4: return "Gamma Doppler Phase 4";
        case PaintKit::FADE: return "Fade";
        case PaintKit::MARBLE_FADE: return "Marble Fade";
        case PaintKit::TIGER_TOOTH: return "Tiger Tooth";
        case PaintKit::CRIMSON_WEB: return "Crimson Web";
        case PaintKit::SLAUGHTER: return "Slaughter";
        case PaintKit::CASE_HARDENED: return "Case Hardened";
        case PaintKit::AUTOTRONIC: return "Autotronic";
        default: return "Unknown Paint Kit";
    }
}

} // namespace SkinChanger

