# cs2-internal

Internal CS2 cheat DLL using ImGui and MinHook.

## Features

- Aimbot
- ESP
- Chams
- FOV changer
- Skin changer
- BHop
- No recoil
- Thirdperson

## Build

Requires CMake and Visual Studio 2022.

```powershell
cd Internal
cmake -B build
cmake --build build --config Release
```

The compiled DLL will be in `Internal/build/Release/cs2_internal.dll`.

## Credits

- [Dear ImGui](https://github.com/ocornut/imgui)
- [MinHook](https://github.com/TsudaKageyu/minhook)
