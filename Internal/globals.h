#pragma once
#include <Windows.h>
#include <cstdint>

struct Config {
    bool menuOpen = false;

    // ESP
    bool espEnabled = false;
    bool espBox = true;
    bool espHealth = true;
    bool espName = false;
    bool espSkeleton = true;

    // Aimbot
    bool aimbotEnabled = false;
    int aimbotKey = VK_LBUTTON; // hold to aim
    float aimbotFov = 8.0f;
    float aimbotSmooth = 5.0f;
    float aimbotJitter = 0.5f;
    bool aimbotRecoilControl = false;
    int aimbotBone = 6;

    // Visuals
    bool showMenu = false;

    // FOV
    bool fovEnabled = true;
    int fovValue = 90;

    // Combat - Third Person
    bool thirdPersonEnabled = false;
    float thirdPersonDistance = 150.0f;

    // Combat - No Recoil
    bool noRecoilEnabled = false;

    // Combat - Bunny Hop
    bool bunnyHopEnabled = false;

    // Combat - No Flash / No Smoke
    bool noFlash = false;
    bool noSmoke = false;


    // Chams
    bool chamsEnabled = true;
    bool chamsXray = true;
    int chamsMaterial = 2;
    float chamsGlowStrength = 6.0f;
    float chamsVisColor[4] = { 72.0f / 255.0f, 97.0f / 255.0f, 100.0f / 255.0f, 1.0f };
    float chamsHidColor[4] = { 105.0f / 255.0f, 75.0f / 255.0f, 150.0f / 255.0f, 225.0f / 255.0f };
};

inline Config g_Config;
inline WNDPROC g_OriginalWndProc = nullptr;
inline HMODULE g_DllModule = nullptr;
