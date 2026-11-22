@echo off
setlocal
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :check_cl_in_path

set "VSINSTALL="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if "%VSINSTALL%"=="" goto :check_cl_in_path
if not exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat" goto :check_cl_in_path

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 1
goto :build_start

:check_cl_in_path
where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: Visual Studio C++ x86 Build Tools not found and cl.exe is not in PATH.
    exit /b 1
)

:build_start
if not "%~1"=="" (
    python gen_def.py "%~1"
    if errorlevel 1 exit /b 1
)

if not exist sdl_proxy.def (
    python gen_def.py
    if errorlevel 1 exit /b 1
)

cl /nologo /c /O2 /MT /W3 /EHsc sdl_proxy.cpp
if errorlevel 1 exit /b 1

lib /nologo /DEF:sdl_proxy.def /MACHINE:X86 /OUT:sdl_proxy.lib
if errorlevel 1 exit /b 1

if not exist ..\mods\display_scaling mkdir ..\mods\display_scaling

link /nologo /DLL /OUT:..\mods\display_scaling\SDL.dll sdl_proxy.obj sdl_proxy.exp user32.lib gdi32.lib opengl32.lib
if errorlevel 1 exit /b 1

del /q sdl_proxy.obj sdl_proxy.lib sdl_proxy.exp 2>nul
echo Build succeeded: ..\mods\display_scaling\SDL.dll
