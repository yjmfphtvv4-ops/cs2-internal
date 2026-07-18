#pragma once
#include <cstdint>
#include "structs.h"
#include "offsets.h"

class CEntityIdentity {
public:
    [[nodiscard]] uintptr_t GetEntityPtr() const {
        return *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(this) + 0x0);
    }

    [[nodiscard]] CEntityIdentity* GetPrev() const {
        return *reinterpret_cast<CEntityIdentity**>(reinterpret_cast<uintptr_t>(this) + 0x50);
    }

    [[nodiscard]] CEntityIdentity* GetNext() const {
        return *reinterpret_cast<CEntityIdentity**>(reinterpret_cast<uintptr_t>(this) + 0x58);
    }
};

class CEntitySystem {
public:
    [[nodiscard]] CEntityIdentity* GetEntityIdentity(int index) const {
        auto list = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(this) + 0x10);
        if (!list) return nullptr;
        return *reinterpret_cast<CEntityIdentity**>(list + index * 0x8);
    }

    static CEntitySystem* GetInstance() {
        auto addr = ReadOffset<uintptr_t>(offsets::dwEntityList);
        return reinterpret_cast<CEntitySystem*>(addr);
    }
};

class C_BaseEntity {
public:
    [[nodiscard]] int32_t GetHealth() const {
        return *reinterpret_cast<const int32_t*>(reinterpret_cast<uintptr_t>(this) + schemas::C_BaseEntity::m_iHealth);
    }

    [[nodiscard]] int32_t GetTeamNum() const {
        return *reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(this) + schemas::C_BaseEntity::m_iTeamNum);
    }

    [[nodiscard]] uintptr_t GetSceneNode() const {
        return *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(this) + schemas::C_BaseEntity::m_pGameSceneNode);
    }

    [[nodiscard]] uint8_t GetLifeState() const {
        return *reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(this) + schemas::C_BaseEntity::m_lifeState);
    }

    [[nodiscard]] bool IsAlive() const {
        return GetHealth() > 0 && GetLifeState() == 0;
    }
};

class C_BasePlayerPawn : public C_BaseEntity {
public:
    [[nodiscard]] Vector3 GetOrigin() const {
        return *reinterpret_cast<const Vector3*>(reinterpret_cast<uintptr_t>(this) + schemas::C_BasePlayerPawn::m_vOldOrigin);
    }

    [[nodiscard]] uintptr_t GetWeaponServices() const {
        return *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(this) + schemas::C_BasePlayerPawn::m_pWeaponServices);
    }
};

class C_CSPlayerPawn : public C_BasePlayerPawn {
public:
    [[nodiscard]] int32_t GetCrosshairIndex() const {
        return *reinterpret_cast<const int32_t*>(reinterpret_cast<uintptr_t>(this) + schemas::C_CSPlayerPawn::m_iIDEntIndex);
    }
};

class CCSPlayerController {
public:
    [[nodiscard]] uintptr_t GetPawn() const {
        auto pawnHandle = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(this) + schemas::CCSPlayerController::m_hPlayerPawn);
        if ((pawnHandle & 0x7FFF) == 0) return 0;
        auto entitySystem = CEntitySystem::GetInstance();
        if (!entitySystem) return 0;
        auto identity = entitySystem->GetEntityIdentity(pawnHandle & 0x7FFF);
        if (!identity) return 0;
        return identity->GetEntityPtr();
    }
};
