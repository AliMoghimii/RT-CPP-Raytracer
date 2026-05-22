@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM  Vulkan Engine — Shader Compilation
REM  Compiles all .comp / .vert / .frag files in every shader
REM  subfolder, with shaders/common as the include path.
REM ============================================================

REM --- Configuration ---
set GLSLC=glslc
set SHADER_ROOT=shaders
set INCLUDE_DIR=%SHADER_ROOT%\common
set TARGET=--target-env=vulkan1.3
set ERROR_COUNT=0

REM --- Folders that contain compilable shaders (NOT 'common', which is headers only) ---
set FOLDERS=legacy visibility rc shading tonemap

REM --- Sanity check: is glslc on PATH? ---
where %GLSLC% >nul 2>&1
if errorlevel 1 (
    echo ERROR: glslc not found in PATH.
    echo Install the Vulkan SDK from https://vulkan.lunarg.com/ and re-open your terminal.
    exit /b 1
)

echo Compiling shaders for Vulkan 1.3...
echo.

REM --- Compile each folder ---
for %%F in (%FOLDERS%) do (
    if exist "%SHADER_ROOT%\%%F" (
        echo [%%F]
        for %%E in (comp vert frag) do (
            for %%S in ("%SHADER_ROOT%\%%F\*.%%E") do (
                if exist "%%S" (
                    echo   %%~nxS
                    %GLSLC% %TARGET% -I "%INCLUDE_DIR%" "%%S" -o "%%S.spv"
                    if errorlevel 1 (
                        echo     ^^^>^^^> COMPILATION FAILED
                        set /a ERROR_COUNT+=1
                    )
                )
            )
        )
    )
)

echo.
if %ERROR_COUNT%==0 (
    echo Success - all shaders compiled.
    exit /b 0
) else (
    echo FAILED with %ERROR_COUNT% error^(s^).
    exit /b 1
)