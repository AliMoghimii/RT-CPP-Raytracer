# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A real-time Vulkan raytracer written in C++. All rendering is done via a single Vulkan compute shader that writes to an output image, which is then copied to the swapchain. There is no graphics pipeline. The project is currently on the `refactor/radiance-cascades` branch, working toward implementing Sparse Radiance Cascades for global illumination (see `Vulkan-Engine/reference-files/showcase_refactoring_guide.md` for the full plan).

## Build & Shader Compilation

**Build**: Open `Vulkan-Engine/Vulkan-Engine.sln` in Visual Studio 2022, target `x64`. Both Debug and Release configurations exist. The Vulkan SDK must be installed system-wide (`$(VULKAN_SDK)` env var) — the project references it via `$(VULKAN_SDK)\Include` and `$(VULKAN_SDK)\Lib`.

**Shaders**: Run from inside `Vulkan-Engine/`:
```bat
compile_shaders.bat
```
This compiles all `.comp`/`.vert`/`.frag` files found in `shaders/legacy/`, `shaders/visibility/`, `shaders/rc/`, `shaders/shading/`, and `shaders/tonemap/` using `glslc --target-env=vulkan1.3`. Output `.spv` files land alongside their source. The `shaders/common/` folder is include-only and skipped. `glslc` must be on PATH (installed with the Vulkan SDK).

**No tests exist.** Correctness is verified by running the application and checking the rendered output.

## Architecture

### Entry Point & Scene Definition
`source/main.cpp` — defines the scene by constructing CPU-side vectors of `GPU*` structs (materials, spheres, triangles, lights, planes, quads, cubes, BVH nodes), configures global render settings, then calls `VulkanCore::run()`. This is where scene geometry and materials are authored.

### Core Engine
`include/core/VulkanCore.hpp` + `source/core/VulkanCore.cpp` — monolithic Vulkan wrapper (~1190 lines). Handles: instance creation, device selection (scores discrete GPUs higher), swapchain, compute pipeline, descriptor sets, scene buffer uploads, texture loading, sync objects, and the render loop. Camera input (WASD + Q/E + Space/LCtrl) is processed here per-frame. Each frame pushes `CameraPushConstants` and dispatches the compute shader at `1280/16 × 720/16` workgroups, then copies the compute image to the swapchain via `vkCmdCopyImage`.

### GPU Data Structs
`include/scene/GPUData.hpp` — mirrors every GLSL struct in the shader exactly. All vec3 members are followed by an explicit float padding field (`p1`, `p2`, etc.) to satisfy `std430` 16-byte alignment. **If you add fields to a GPU struct, update both the C++ definition and the GLSL struct in the shader — they must match byte-for-byte.**

### Shader
`shaders/legacy/raytracer.comp` — monolithic GLSL compute shader (~970+ lines). Does primary ray casting, BVH traversal, sphere/triangle/plane/quad/cube intersection, Blinn-Phong and PBR shading, reflection, refraction/glass, soft shadows, texture sampling (albedo, normal map, roughness, AO, parallax displacement), procedural noise patterns, fog, and skybox. All scene data arrives via SSBOs; camera/settings arrive via push constants.

**Descriptor set layout (binding indices):**
- 0: `rgba8` storage image (output)
- 1–8: SSBOs for materials, spheres, triangles, lights, planes, quads, cubes, BVH nodes (in that order)
- 9: runtime array of 100 combined image samplers (textures)

### Scene Loaders
- `include/scene/ModelLoader.hpp` + `source/scene/ModelLoader.cpp` — loads `.obj` files into `GPUTriangle` vectors and builds a median-split AABB BVH (`GPUBVHNode`). Call via `ModelLoader::load(path, triangles, bvhNodes, materialIndex, position, rotation, scale)`.
- `include/scene/ImageLoader.hpp` + `source/scene/ImageLoader.cpp` — wraps `stb_image`. `ImageLoader::load()` registers a texture path and returns its index (to assign into material fields). `loadPixels()`/`freePixels()` do the actual pixel decode used by `VulkanCore`.
- `include/math/MathUtils.hpp` — `MathUtils::rotateVec(point, rotInDegrees)` applies ZYX Euler rotation via GLM, used by ModelLoader for OBJ transforms.

### In-Progress Refactor Structure
The `include/` and `source/` subdirectory layout is already in place from Part 0 of the refactor guide:
- `include/{core,scene,math,passes,gi,debug}/`
- `source/{core,scene,passes,gi,debug}/`
- `shaders/{legacy,common,visibility,rc,shading,tonemap}/`

Future passes (visibility, RC probe trace, cascade merge, final gather, tonemap) will live in `source/passes/` and `shaders/<pass-name>/`. Common GLSL headers will live in `shaders/common/`. See `reference-files/showcase_refactoring_guide.md` for the full plan.

## Key Conventions

**GPU struct padding**: Every `vec3` field in a GPU struct must be followed by a `float pN` padding field. The shader and C++ struct must be identical in layout.

**Material fields**: `{ color, ambient, emission, diffuse, color2, specular, reflection, transparency, ior, shadingModel, patternType, roughness, metallic, castShadows, useTexture, albedoIndex, normalMapIndex, roughnessIndex, aoIndex, heightMapIndex, proceduralScale, proceduralWobble, bumpStrength, parallaxScale, p5, p6 }`. Use `-1` for texture indices when a map is absent.

**Texture budget**: The descriptor pool and layout are hardcoded to 100 textures. Adding more requires changing the `descriptorCount = 100` in both `createDescriptorSetLayout()` and `createDescriptorPool()` in `VulkanCore.cpp`.

**Shader path**: `VulkanCore::createComputePipeline()` loads `shaders/legacy/raytracer.comp.spv` relative to the working directory (the output binary directory). The `.spv` must be regenerated via `compile_shaders.bat` after any shader edit.

**Dynamic scene data**: `updateDynamicData()` in `VulkanCore.cpp` re-uploads the sphere buffer each frame (used for the animated eye pupils that track the camera). Any other per-frame CPU→GPU updates need similar map/memcpy/unmap calls.
