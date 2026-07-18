#pragma once
#include <cstdint>

// Skin changer specific offsets for CS2
namespace skin_offsets {

    // C_AttributeManager offsets (from C_EconEntity)
    constexpr std::uintptr_t m_AttributeManager = 0x11A8;  // C_AttributeContainer
    constexpr std::uintptr_t m_Item = 0x50;                // C_EconItemView (inside C_AttributeContainer)

    // C_EconItemView offsets
    constexpr std::uintptr_t m_iItemDefinitionIndex = 0x1BA;  // uint16
    constexpr std::uintptr_t m_iEntityQuality = 0x1BC;        // int32
    constexpr std::uintptr_t m_iEntityLevel = 0x1C0;          // uint32
    constexpr std::uintptr_t m_iItemIDHigh = 0x1D0;           // uint32
    constexpr std::uintptr_t m_iItemIDLow = 0x1D4;            // uint32
    constexpr std::uintptr_t m_iAccountID = 0x1D8;            // uint32
    constexpr std::uintptr_t m_iRarityOverride = 0x1F0;       // int32
    constexpr std::uintptr_t m_AttributeList = 0x208;         // CAttributeList
    constexpr std::uintptr_t m_szCustomName = 0x2F8;          // char[161]

    // CAttributeList offsets
    constexpr std::uintptr_t m_Attributes = 0x8;              // C_UtlVectorEmbeddedNetworkVar<CEconItemAttribute>

    // C_BasePlayerWeapon / C_EconEntity offsets
    constexpr std::uintptr_t m_nSubclassID = 0x380;           // CUtlStringToken (uint32)
    constexpr std::uintptr_t m_nFallbackPaintKit = 0x1680;    // int32
    constexpr std::uintptr_t m_nFallbackSeed = 0x1684;        // int32
    constexpr std::uintptr_t m_flFallbackWear = 0x1688;       // float
    constexpr std::uintptr_t m_nFallbackStatTrak = 0x168C;    // int32
    constexpr std::uintptr_t m_nCustomEconReloadEventId = 0x18E4;  // int32 - CRITICAL: increment to trigger composite material rebuild

    // CRenderComponent offsets
    constexpr std::uintptr_t m_CRenderComponent = 0xAF0;      // CRenderComponent* on C_BaseModelEntity

    // CModelState offsets
    constexpr std::uintptr_t m_MeshGroupMask = 0x1C8;         // uint64 (within CModelState)

    // C_BaseEntity offsets
    constexpr std::uintptr_t m_pGameSceneNode = 0x330;        // CGameSceneNode*
    constexpr std::uintptr_t m_CBodyComponent = 0x30;         // CBodyComponent*

    // CGameSceneNode offsets
    constexpr std::uintptr_t m_pChild = 0x40;                 // CGameSceneNode*
    constexpr std::uintptr_t m_pOwner = 0x30;                 // C_BaseEntity*

    // CBodyComponent offsets
    constexpr std::uintptr_t m_animationController = 0x30;    // CAnimationController offset

    // CAnimationController offsets
    constexpr std::uintptr_t m_pGraphInstanceAG2 = 0x18;      // CNmGraphInstance*

    // C_BasePlayerPawn offsets
    constexpr std::uintptr_t m_pWeaponServices = 0x1208;      // CPlayer_WeaponServices*
    constexpr std::uintptr_t m_hHudModelArms = 0x1B7C;        // CHandle<C_BaseEntity> (Matches Offsets::m_hHudModelArms in offsets.h)
    constexpr std::uintptr_t m_agentItem = 0x620;             // C_EconItemView (agent cosmetic)

    // CPlayer_WeaponServices offsets
    constexpr std::uintptr_t m_hActiveWeapon = 0x60;          // CHandle<C_BasePlayerWeapon>
}
