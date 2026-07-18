#pragma once

#include <cstdint>
#include <Windows.h>

// CS2-Dumper generated offsets
#include "D:/cs2/output/offsets.hpp"
#include "D:/cs2/output/client_dll.hpp"
#include "D:/cs2/output/buttons.hpp"

// Namespace aliases for convenience
namespace offsets = cs2_dumper::offsets::client_dll;
namespace schemas = cs2_dumper::schemas::client_dll;
namespace buttons = cs2_dumper::buttons;

// Module base helpers — dw* offsets are relative to client.dll
inline uintptr_t GetClientBase() {
    static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    return base;
}

template<typename T>
inline T ReadOffset(ptrdiff_t offset) {
    return *reinterpret_cast<T*>(GetClientBase() + offset);
}
