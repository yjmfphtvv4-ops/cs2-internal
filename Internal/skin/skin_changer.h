#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>

namespace SkinChanger {

    extern bool thirdPerson;

    // Agent definitions
    enum class AgentType : int {
        DEFAULT_CT = 14,           // Default CT agent
        DEFAULT_T = 15,            // Default T agent
    };

    // Agent skin definitions (paint kits for specific skins)
    enum class AgentSkin : int {
        SIR_BLOODY_SKULLHEAD = 3,  // Sir Bloody Skullhead Darryl paint kit ID
    };

    // Knife definitions
    enum class KnifeType : int {
        BAYONET = 500,
        FLIP = 505,
        GUT = 506,
        KARAMBIT = 507,
        M9_BAYONET = 508,
        HUNTSMAN = 509,
        FALCHION = 512,
        BOWIE = 514,
        BUTTERFLY = 515,
        SHADOW_DAGGERS = 516,
        PARACORD = 517,
        SURVIVAL = 518,
        URSUS = 519,
        NAVAJA = 520,
        NOMAD = 521,
        STILETTO = 522,
        TALON = 523,
        CLASSIC = 503,
        SKELETON = 525
    };

    // Paint kit IDs for popular skins
    enum class PaintKit : int {
        // Doppler Phases
        DOPPLER_RUBY = 415,
        DOPPLER_SAPPHIRE = 416,
        DOPPLER_BLACK_PEARL = 417,
        DOPPLER_PHASE1 = 418,
        DOPPLER_PHASE2 = 419,
        DOPPLER_PHASE3 = 420,
        DOPPLER_PHASE4 = 421,
        
        // Popular skins
        GAMMA_DOPPLER_EMERALD = 568,
        GAMMA_DOPPLER_PHASE1 = 569,
        GAMMA_DOPPLER_PHASE2 = 570,
        GAMMA_DOPPLER_PHASE3 = 571,
        GAMMA_DOPPLER_PHASE4 = 572,
        
        FADE = 38,
        MARBLE_FADE = 413,
        TIGER_TOOTH = 411,
        CRIMSON_WEB = 12,
        SLAUGHTER = 59,
        CASE_HARDENED = 44,
        AUTOTRONIC = 619,
        LORE = 568,
        GAMMA_DOPPLER = 568
    };

    struct SkinConfig {
        int itemDefinitionIndex = -1;  // Weapon/knife ID
        int paintKit = 0;              // Skin paint kit
        int seed = 0;                  // Pattern seed
        float wear = 0.0001f;          // Wear value (0.0 = FN, 1.0 = BS)
        int statTrak = -1;             // StatTrak counter (-1 = disabled)
        char customName[32] = "";      // Custom name tag
        bool enabled = false;
    };

    // Agent cosmetic configuration
    struct AgentConfig {
        int agent = static_cast<int>(AgentType::DEFAULT_CT);  // Agent definition index
        int paintKit = 0;                                      // Agent skin paint kit
        int seed = 0;                                          // Pattern seed
        float wear = 0.0001f;                                  // Wear value
        bool enabled = false;
    };

    // Weapon slot configuration
    struct WeaponSlotConfig {
        AgentConfig agent;
        SkinConfig knife;
        SkinConfig rifle;
    };

    // Initialize skin changer
    void Initialize();

    // Apply skins to local player weapons
    void ApplySkins();

    // Force update weapon model and animations
    void ForceWeaponUpdate(uintptr_t weapon);

    // Get current configuration
    WeaponSlotConfig& GetConfig();

    // Utility: Get weapon name from definition index
    const char* GetWeaponName(int defIndex);

    // Utility: Get knife name
    const char* GetKnifeName(KnifeType knife);

    // Utility: Get paint kit name
    const char* GetPaintKitName(PaintKit kit);
}
