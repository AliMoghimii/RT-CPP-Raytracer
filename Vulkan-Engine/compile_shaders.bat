@echo off
setlocal

echo Compiling raytracer.comp...
glslc shaders/raytracer.comp -o shaders/raytracer.comp.spv
if errorlevel 1 (
    echo.
    echo ERROR: Failed to compile shaders/raytracer.comp
    pause
    exit /b 1
)

echo Compiling photonpass.comp...
glslc shaders/photonpass.comp -o shaders/photonpass.comp.spv
if errorlevel 1 (
    echo.
    echo ERROR: Failed to compile shaders/photonpass.comp
    pause
    exit /b 1
)

echo.
echo Shaders compiled successfully.
pause
exit /b 0