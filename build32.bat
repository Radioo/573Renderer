@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul
set "ROOT=%CD%"
popd >nul

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo ERROR: Could not find Visual Studio installation
    exit /b 1
)
set "VCVARS=%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    echo ERROR: vcvarsall.bat not found at %VCVARS%
    exit /b 1
)
call "%VCVARS%" x86 >nul 2>&1

set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

echo [Build32] Using VS from: %VSDIR%
echo [Build32] CMake: & where cmake

set "VCPKG_ROOT=%ROOT%\vendor\vcpkg"
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo ERROR: vcpkg submodule missing. Run from repo root:
    echo    git submodule update --init --recursive
    exit /b 1
)
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [Build32] Bootstrapping vcpkg ...
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
    if not "!ERRORLEVEL!"=="0" (
        echo vcpkg bootstrap FAILED with exit code !ERRORLEVEL!
        exit /b 1
    )
)

cd /d "%ROOT%"

if exist "build32\CMakeCache.txt" (
    echo [Build32] Reusing existing configure - ninja re-runs CMake if inputs changed
) else (
    cmake --preset dev32
    set "CONFIGURE_RC=!ERRORLEVEL!"
    if not "!CONFIGURE_RC!"=="0" (
        echo CMake configure FAILED with exit code !CONFIGURE_RC!
        exit /b 1
    )
)

cmake --build --preset dev32
set "BUILD_RC=%ERRORLEVEL%"
if not "%BUILD_RC%"=="0" (
    echo Build FAILED with exit code %BUILD_RC%
    exit /b 1
)

echo.
echo ============================================
echo  Build successful! Binary at: bin\573Renderer32.exe
echo ============================================
echo.
echo The 32-bit build exists to host 32-bit game DLLs (IIDX 24 and older,
echo which ship libavs-win32 / libafp-win32). A 64-bit process cannot load
echo them. Point it at a 32-bit Konami game directory.
