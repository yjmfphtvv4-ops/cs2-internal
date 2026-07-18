#pragma once
#include <cstdint>
#include <windows.h>
#include <vector>
#include <Psapi.h>
#include "../sdk/offsets.h"
#include "../sdk/interfaces.h"

namespace Game {
    template<typename T>
    inline T Read(uintptr_t addr) {
        return *(T*)addr;
    }

    template<typename T>
    inline void Write(uintptr_t addr, T val) {
        *(T*)addr = val;
    }

    inline uintptr_t FindPattern(LPCWSTR moduleW, const char* pattern) {
        char modName[256];
        WideCharToMultiByte(CP_ACP, 0, moduleW, -1, modName, 256, nullptr, nullptr);
        HMODULE hMod = GetModuleHandleA(modName);
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
                bytes.push_back(0); mask.push_back(false); p++;
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
            for (size_t j = 0; j < bytes.size() && found; j++)
                if (mask[j] && *(uint8_t*)(base + i + j) != bytes[j])
                    found = false;
            if (found) return base + i;
        }
        return 0;
    }

    inline uintptr_t GetLocalPlayerPawn() {
        return ReadOffset<uintptr_t>(offsets::dwLocalPlayerPawn);
    }

    inline uintptr_t GetEntityByHandle(uint32_t handle) {
        uint16_t index = handle & 0x7FFF;
        if (index == 0) return 0;
        __try {
            uintptr_t list = *reinterpret_cast<uintptr_t*>(GetClientBase() + offsets::dwEntityList);
            if (!list || IsBadReadPtr((void*)(list + index * 8), 8)) return 0;
            uintptr_t identity = *reinterpret_cast<uintptr_t*>(list + index * 8);
            if (!identity || IsBadReadPtr((void*)identity, 8)) return 0;
            return *reinterpret_cast<uintptr_t*>(identity);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return 0;
    }
}
