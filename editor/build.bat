@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul
set "EDITOR_ROOT=%CD%"
popd >nul
pushd "%SCRIPT_DIR%.." >nul
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
call "%VCVARS%" x64 >nul 2>&1

set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

set "VCPKG_ROOT=%ROOT%\vendor\vcpkg"
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [Editor] Bootstrapping vcpkg ...
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
    if not "!ERRORLEVEL!"=="0" (
        echo vcpkg bootstrap FAILED with exit code !ERRORLEVEL!
        exit /b 1
    )
)

cd /d "%EDITOR_ROOT%"

if exist "%ROOT%\build-editor\CMakeCache.txt" (
    echo [Editor] Reusing existing configure - ninja re-runs CMake if inputs changed
) else (
    cmake --preset editor
    set "CONFIGURE_RC=!ERRORLEVEL!"
    if not "!CONFIGURE_RC!"=="0" (
        echo CMake configure FAILED with exit code !CONFIGURE_RC!
        exit /b 1
    )
)

cmake --build --preset editor
set "BUILD_RC=%ERRORLEVEL%"
if not "%BUILD_RC%"=="0" (
    echo Editor build FAILED with exit code %BUILD_RC%
    exit /b 1
)

echo.
echo ============================================
echo  Editor build succeeded. Binary at: build-editor\ifs_editor.exe
echo ============================================
