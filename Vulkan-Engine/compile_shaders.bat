@echo off
glslc shaders/raytracer.comp -o shaders/raytracer.comp.spv
echo Shaders compiled.
exit /b 0