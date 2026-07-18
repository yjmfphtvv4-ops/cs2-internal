@echo off
setlocal

set "ROOT=%~dp0"
set "TEMP_DIR=%ROOT%_build"
set "OUTPUT_DIR=D:\cs2\build"
set "CONFIG=Release"

echo.
echo === Setting up VS 2022 environment ===
echo.

:: Find Visual Studio 2022
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_DIR=%%i"
)

if not defined VS_DIR (
    echo [!] Visual Studio 2022 not found!
    pause
    exit /b 1
)

call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64

echo.
echo === Building CS2 Internal (x64 %CONFIG%) ===
echo.

if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"
mkdir "%TEMP_DIR%"
cd /d "%TEMP_DIR%"

cmake -G "Visual Studio 17 2022" -A x64 "%ROOT%"
if %ERRORLEVEL% neq 0 (
    echo [!] CMake configuration failed.
    rmdir /s /q "%TEMP_DIR%"
    pause
    exit /b 1
)

cmake --build . --config %CONFIG% -- /v:q /nologo
if %ERRORLEVEL% neq 0 (
    echo [!] Build failed!
    rmdir /s /q "%TEMP_DIR%"
    pause
    exit /b 1
)

echo.
echo === Copying DLL ===
echo.

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
copy /y "%TEMP_DIR%\%CONFIG%\cs2_internal.dll" "%OUTPUT_DIR%\"
copy /y "%TEMP_DIR%\%CONFIG%\cs2_internal.pdb" "%OUTPUT_DIR%\"

rmdir /s /q "%TEMP_DIR%"

echo.
echo ================================
echo  Build complete!
echo  DLL:    %OUTPUT_DIR%\cs2_internal.dll
echo  PDB:    %OUTPUT_DIR%\cs2_internal.pdb
echo ================================
echo.

pause
