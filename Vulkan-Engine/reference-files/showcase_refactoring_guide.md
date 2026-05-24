# Sparse Radiance Cascades — Project Refactoring Guide
## GameRepublic Student Showcase Build — v2

> **Compiled for**: Jose Javier Serrano Solis & teammate  
> **Target**: GameRepublic Student Showcase 2026  
> **Existing project**: Vulkan C++ compute-shader ray tracer with software BVH  
> **Goal**: Add Sparse Radiance Cascades (showcase-grade, pragmatic scope)  
> **Companion document**: `sparse_rc_dissertation_guide.md` (the deep theory reference)  
> **Document version**: v2 (adds Part 0 — Project Setup Stage covering Vulkan SDK migration, folder restructure, .vcxproj fixes, new shader compilation script, and legacy shader relocation)

---

## How to Use This Document

This is a **practical refactoring guide**. It assumes you've already read (or have available) the dissertation study guide. This document focuses on:

1. **What to change** in your teammate's existing project, file by file
2. **In what order** to make the changes (so the project doesn't break for long stretches)
3. **What to skip for the showcase** vs what to do later for the dissertation
4. **Concrete code patches** ready to apply

When in doubt, this guide is opinionated. The dissertation guide explains *why* each decision is made; this guide is *what to type*.

**Important**: Run this guide alongside Claude Code (or another AI assistant) inside your repo. The AI can read this document plus your actual source files and help with the mechanical parts.

---

## Table of Contents

### Part 0 — Project Setup Stage (DO FIRST)
0.1 Why This Stage Exists  
0.2 Update to Vulkan SDK (from bundled `lib/vulkan` to system SDK)  
0.3 Folder Restructure (`include/` + `source/` split)  
0.4 Visual Studio Project File Repair (.vcxproj path fixes)  
0.5 Update Additional Include Directories  
0.6 Move Legacy Shader to `shaders/legacy/`  
0.7 New Shader Compilation Script (`compile_shaders.bat` v2)  
0.8 Build Verification Before Part A

### Part A — Setup
1. The Branching Strategy with Your Teammate
2. Pre-Refactor Cleanup (Bugs to Fix First)
3. Build Configuration Updates
4. Folder Restructure
5. Asset Preparation (Tessellation, Test Scenes)

### Part B — Architectural Refactor
6. Phase 1: Extract the Mega-Shader into Modules
7. Phase 2: Introduce the G-Buffer
8. Phase 3: Decouple Vulkan Initialization
9. Phase 4: Add a Render Graph

### Part C — Radiance Cascades Implementation (Showcase Scope)
10. Showcase-Scope Decision: Dense Grid First
11. Phase 5: Cascade Data Structures
12. Phase 6: Cascade 0 Ray Tracing
13. Phase 7: Multi-Cascade Hierarchy
14. Phase 8: Cascade Merging
15. Phase 9: Final Gather
16. Phase 10: Tonemap & Composite

### Part D — Polish for Showcase
17. ImGui Debug Interface (Showcase Edition)
18. Demo Scene Authoring
19. Performance Profiling
20. Visual Sanity Checks

### Part E — Migration Path to Dissertation Build
21. From Dense Grid to Sparse Hash Map
22. Adding DDGI and SSGI Comparison Baselines
23. Reference Path Tracer for RMSE
24. Benchmark Infrastructure

---

# Part 0 — Project Setup Stage

> **Read this first. Do not skip.**  
> Every later phase assumes the foundation built in this phase. If you go straight to Part A on top of a misconfigured project, you will spend more time debugging build errors than implementing cascades. Budget 1–2 days for this phase. It is unglamorous but unavoidable.

## 0.1 — Why This Stage Exists

The existing project has four foundational issues that block the refactor:

1. **Vulkan headers and libs are bundled inside the repo** at `Vulkan-Engine/lib/vulkan/` rather than coming from a system-wide Vulkan SDK install. This locks the project to whatever version was committed, prevents using validation layers, and won't reliably support the Vulkan 1.3 features we need (synchronization2, dynamic rendering, descriptor indexing).
2. **All source and header files sit flat at the project root** (or inside `Vulkan-Engine/`) with no separation. This prevents the modular architecture the refactor requires.
3. **The Visual Studio project file (.vcxproj) references files by their old flat paths.** Once we move files into subfolders it will break with red-X icons in Solution Explorer.
4. **The shader compilation script hardcodes one file** (`raytracer.comp`). As soon as we add a second shader for the refactor, the script silently leaves it uncompiled.

Part 0 fixes all four before any bug fixes or feature work. Crucially, **the project must still build and run identically to before** at the end of Part 0. We are reshaping the foundation, not changing behaviour yet.

### Verification at the end of Part 0

Before moving to Part A, you should be able to:

- Run `compile_shaders.bat` and see `shaders/legacy/raytracer.comp` get compiled to `raytracer.comp.spv` in the same folder
- Build the project in Visual Studio (both Debug and Release configurations, x64)
- Run the executable and see the same image the project produced *before* the restructure — pixel-identical
- See no red-X icons in Solution Explorer

If any of those fail, fix it before progressing.

## 0.2 — Update to Vulkan SDK

### Current state

```
Vulkan-Engine/
└── lib/
    └── vulkan/
        ├── Include/    ← bundled, outdated copy
        └── Lib/
```

The Visual Studio project's include and library paths currently point at this bundled copy, e.g. `C:\Users\javie\GitHub\RT-CPP-Raytracer\Vulkan-Engine\lib\vulkan`.

### Target state

The project should use the LunarG Vulkan SDK installed system-wide at `C:\VulkanSDK\1.4.321.1\` (or whichever version is current). The installer sets the `VULKAN_SDK` environment variable so we can reference it portably as `$(VULKAN_SDK)` in MSBuild paths.

### Step-by-step

#### 1. Install the Vulkan SDK

1. Download the latest Windows installer from https://vulkan.lunarg.com/sdk/home#windows
2. Run the installer with default options. It installs to `C:\VulkanSDK\<version>\` and sets up `VULKAN_SDK` and adds the SDK's `Bin` folder to `PATH`
3. Reboot if prompted (or at least open a *new* terminal — PATH changes don't apply to existing shells)

#### 2. Verify installation

Open a **new** terminal (not an existing one — PATH won't be updated):

```bat
echo %VULKAN_SDK%
where vulkaninfo
where glslc
vulkaninfoSDK --summary
```

Expected output: a path like `C:\VulkanSDK\1.4.321.1`, paths to the SDK's binaries, and a summary listing your GPU. If any command fails or shows the old bundled path, the SDK isn't on PATH; reopen the terminal or reboot.

#### 3. Update Visual Studio project paths

1. Right-click the project in Solution Explorer → **Properties**
2. Set **Configuration: All Configurations** and **Platform: All Platforms** at the top so changes apply across Debug/Release and x64/x86
3. Navigate to **VC++ Directories**

**Include Directories**:
- Find any path containing `lib\vulkan\Include` or referencing `$(ProjectDir)lib\vulkan` — remove it
- Add: `$(VULKAN_SDK)\Include`

**Library Directories**:
- Find any path containing `lib\vulkan\Lib` — remove it
- Add: `$(VULKAN_SDK)\Lib`

**Linker → Input → Additional Dependencies**:
- Confirm `vulkan-1.lib` is in the list (it should already be there)

Click **Apply** then **OK**.

#### 4. Remove the bundled Vulkan copy

Once you've confirmed the project still builds against the system SDK:

```bat
cd Vulkan-Engine
rmdir /s /q lib\vulkan
```

If `lib/` becomes empty, remove it too:

```bat
rmdir lib
```

(If `lib/` contains other third-party headers like GLFW or stb_image, leave it — only delete the `vulkan/` subfolder.)

#### 5. Test the build

Build the project in Visual Studio. It should compile cleanly against the system SDK and produce the same output as before. If you get "cannot find vulkan/vulkan.h" errors, the include path didn't update — recheck VC++ Directories.

#### 6. Commit

```bat
git add -A
git commit -m "Migrate from bundled Vulkan headers to system VULKAN_SDK"
```

## 0.3 — Folder Restructure

### Target layout

```
Vulkan-Engine/
├── include/
│   ├── core/                  (empty for now; populated in Phase 3)
│   ├── resources/             (empty for now; populated in Phase 3)
│   ├── scene/
│   │   ├── GPUData.hpp
│   │   ├── ImageLoader.hpp
│   │   └── ModelLoader.hpp
│   ├── math/
│   │   └── MathUtils.hpp
│   ├── passes/                (empty for now; populated in Phase 1)
│   ├── gi/                    (empty for now; populated in Phase 5)
│   └── debug/                 (empty for now; populated in Phase 7)
├── source/
│   ├── core/
│   ├── resources/
│   ├── scene/
│   │   ├── ImageLoader.cpp
│   │   └── ModelLoader.cpp
│   ├── passes/
│   ├── gi/
│   ├── debug/
│   ├── VulkanCore.cpp         (stays for now; split in Phase 3)
│   └── main.cpp
├── shaders/
│   ├── legacy/                ← frozen original; dispatched by legacyPass (do not modify)
│   │   ├── raytracer.comp
│   │   └── raytracer.comp.spv
│   ├── monolith/              ← actively refactored monolithic shader (Phase 1 working copy)
│   │   ├── raytracer.comp
│   │   └── raytracer.comp.spv
│   ├── common/                (empty for now)
│   ├── visibility/            (empty for now)
│   ├── rc/                    (empty for now)
│   ├── shading/               (empty for now)
│   └── tonemap/               (empty for now)
├── assets/
│   ├── models/
│   └── textures/
├── compile_shaders.bat
├── Vulkan-Engine.vcxproj
└── Vulkan-Engine.vcxproj.filters
```

Note: `VulkanCore.hpp` and `VulkanCore.cpp` stay at the project root for now. They will be split into focused classes (`VulkanContext`, `Swapchain`, `CommandManager`, etc.) during Part B Phase 3. Premature splitting will create more breakage than benefit.

### File placement rules (cheat sheet)

| File | Goes to |
|---|---|
| `GPUData.hpp` | `include/scene/` |
| `ImageLoader.hpp` | `include/scene/` |
| `ImageLoader.cpp` | `source/scene/` |
| `ModelLoader.hpp` | `include/scene/` |
| `ModelLoader.cpp` | `source/scene/` |
| `MathUtils.hpp` | `include/math/` |
| `main.cpp` | `source/` |
| `VulkanCore.hpp` | stay at project root (transient) |
| `VulkanCore.cpp` | stay at project root (transient) |
| `raytracer.comp` (frozen original) | `shaders/legacy/` — do not modify; dispatched by `legacyPass` |
| `raytracer.comp` (refactored copy) | `shaders/monolith/` — the Phase 1 working copy; receives `#include` directives |
| `raytracer.comp.spv` | regenerated in both folders by `compile_shaders.bat` |

### Step-by-step

#### 1. Close Visual Studio entirely

The IDE locks files. Close it before moving anything.

#### 2. Create the folder structure

From a terminal in `Vulkan-Engine/`:

```bat
mkdir include\core include\resources include\scene include\math
mkdir include\passes include\gi include\debug
mkdir source\core source\resources source\scene
mkdir source\passes source\gi source\debug
mkdir shaders\legacy shaders\common shaders\visibility
mkdir shaders\rc shaders\shading shaders\tonemap
```

#### 3. Move files

Use file explorer or:

```bat
move GPUData.hpp include\scene\
move ImageLoader.hpp include\scene\
move ImageLoader.cpp source\scene\
move ModelLoader.hpp include\scene\
move ModelLoader.cpp source\scene\
move MathUtils.hpp include\math\
move main.cpp source\
move shaders\raytracer.comp shaders\legacy\
move shaders\raytracer.comp.spv shaders\legacy\
```

#### 4. Update `#include` directives in source files

Every `#include` in your `.cpp` and `.hpp` files that references one of the moved headers needs to be updated. The new convention uses the subfolder as a namespace-like prefix:

| Old | New |
|---|---|
| `#include "GPUData.hpp"` | `#include "scene/GPUData.hpp"` |
| `#include "ImageLoader.hpp"` | `#include "scene/ImageLoader.hpp"` |
| `#include "ModelLoader.hpp"` | `#include "scene/ModelLoader.hpp"` |
| `#include "MathUtils.hpp"` | `#include "math/MathUtils.hpp"` |

Files to check:

- `VulkanCore.hpp` — includes `GPUData.hpp`, `ModelLoader.hpp`, `ImageLoader.hpp`
- `VulkanCore.cpp` — likely the same
- `main.cpp` — likely includes most of the above
- `ModelLoader.cpp` — includes `MathUtils.hpp`
- `ModelLoader.hpp` — includes `GPUData.hpp`
- `ImageLoader.cpp` — includes `ImageLoader.hpp`
- `ImageLoader.hpp` — no headers from this project
- `MathUtils.hpp` — no headers from this project

Use your IDE's find-in-files (Ctrl+Shift+F in VS) to find every match across the codebase. Don't rely on memory.

The reason for the `scene/`-prefixed include style is that we set the include search path to `$(ProjectDir)include` in the next step. With that single path, the compiler resolves `scene/GPUData.hpp` to `include/scene/GPUData.hpp` automatically. This scales: when you write `passes/PrimaryVisibilityPass.hpp` in Phase 1, no further configuration is needed.

## 0.4 — Visual Studio Project File Repair

After moving files, Visual Studio's project file still references their old locations. Opening the solution shows red-X icons on every moved file and the error "The document cannot be opened. It has been renamed, deleted or moved."

### Approach: edit .vcxproj XML directly (fastest)

#### 1. Back up first

Copy these files somewhere safe in case the edit goes wrong:

```bat
copy Vulkan-Engine.vcxproj Vulkan-Engine.vcxproj.bak
copy Vulkan-Engine.vcxproj.filters Vulkan-Engine.vcxproj.filters.bak
copy Vulkan-Engine.vcxproj.user Vulkan-Engine.vcxproj.user.bak
```

#### 2. Open `Vulkan-Engine.vcxproj` in a text editor (not Visual Studio)

VS Code, Notepad++, or any plain text editor.

#### 3. Find and update file references

Look for `<ClInclude Include="..." />` (header files) and `<ClCompile Include="..." />` (source files). Update each path:

Before:
```xml
<ItemGroup>
  <ClInclude Include="GPUData.hpp" />
  <ClInclude Include="ImageLoader.hpp" />
  <ClInclude Include="MathUtils.hpp" />
  <ClInclude Include="ModelLoader.hpp" />
  <ClInclude Include="VulkanCore.hpp" />
</ItemGroup>
<ItemGroup>
  <ClCompile Include="ImageLoader.cpp" />
  <ClCompile Include="main.cpp" />
  <ClCompile Include="ModelLoader.cpp" />
  <ClCompile Include="VulkanCore.cpp" />
</ItemGroup>
```

After:
```xml
<ItemGroup>
  <ClInclude Include="include\scene\GPUData.hpp" />
  <ClInclude Include="include\scene\ImageLoader.hpp" />
  <ClInclude Include="include\math\MathUtils.hpp" />
  <ClInclude Include="include\scene\ModelLoader.hpp" />
  <ClInclude Include="VulkanCore.hpp" />
</ItemGroup>
<ItemGroup>
  <ClCompile Include="source\scene\ImageLoader.cpp" />
  <ClCompile Include="source\main.cpp" />
  <ClCompile Include="source\scene\ModelLoader.cpp" />
  <ClCompile Include="VulkanCore.cpp" />
</ItemGroup>
```

Use Windows-style backslashes (`\`), not forward slashes. MSBuild is inconsistent about forward slashes.

#### 4. Repeat for `.vcxproj.filters`

The `.filters` file controls the Solution Explorer's virtual folder organization. To make Solution Explorer mirror the new disk layout, add filter entries and assign each file to its filter:

```xml
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <Filter Include="include">
      <UniqueIdentifier>{generate-a-guid}</UniqueIdentifier>
    </Filter>
    <Filter Include="include\scene">
      <UniqueIdentifier>{generate-a-guid}</UniqueIdentifier>
    </Filter>
    <Filter Include="include\math">
      <UniqueIdentifier>{generate-a-guid}</UniqueIdentifier>
    </Filter>
    <Filter Include="source">
      <UniqueIdentifier>{generate-a-guid}</UniqueIdentifier>
    </Filter>
    <Filter Include="source\scene">
      <UniqueIdentifier>{generate-a-guid}</UniqueIdentifier>
    </Filter>
    <Filter Include="shaders">
      <UniqueIdentifier>{generate-a-guid}</UniqueIdentifier>
    </Filter>
    <Filter Include="shaders\legacy">
      <UniqueIdentifier>{generate-a-guid}</UniqueIdentifier>
    </Filter>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="include\scene\GPUData.hpp">
      <Filter>include\scene</Filter>
    </ClInclude>
    <ClInclude Include="include\scene\ImageLoader.hpp">
      <Filter>include\scene</Filter>
    </ClInclude>
    <ClInclude Include="include\math\MathUtils.hpp">
      <Filter>include\math</Filter>
    </ClInclude>
    <!-- and so on -->
  </ItemGroup>
</Project>
```

For GUIDs, any GUID generator works. In Visual Studio you can also use **Tools → Create GUID** (pick the "Registry Format" option, e.g. `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`). Generate one fresh GUID per filter — don't reuse them.

Setting up filters is optional for the build to work but highly recommended for sanity. Without filters, Solution Explorer shows everything flat which gets unmanageable as the project grows.

#### 5. Reopen Visual Studio

Open the solution. The red-X icons should be gone. Solution Explorer should show files organized into the filters you defined.

### Alternative: fix in the IDE (slower, no XML editing)

If editing XML feels risky:

1. Open the solution in Visual Studio
2. For each red-X file in Solution Explorer:
   - Right-click → **Remove** (NOT Delete — Remove keeps the file on disk, just removes the broken project reference)
3. Right-click the project → **Add → Existing Item**, navigate to each subfolder (`include/scene/`, `source/scene/`, etc.) and re-add the moved files
4. Drag files into the appropriate Filter folders in Solution Explorer if you've created filters

This works but is tedious with many files. XML editing is faster for ≥5 files.

## 0.5 — Update Additional Include Directories

This step is what makes `#include "scene/GPUData.hpp"`-style includes resolve correctly. Without it the compiler can't find headers in `include/scene/` even though they're in the project.

1. Right-click the project → **Properties**
2. **Configuration: All Configurations**, **Platform: All Platforms**
3. Navigate to **C/C++ → General → Additional Include Directories**
4. Add this entry: `$(ProjectDir)include`

That single entry is sufficient. Don't add each subfolder individually — adding just `$(ProjectDir)include` lets you write paths relative to it:

```cpp
#include "scene/GPUData.hpp"   // resolves to $(ProjectDir)include/scene/GPUData.hpp
#include "math/MathUtils.hpp"  // resolves to $(ProjectDir)include/math/MathUtils.hpp
```

The subfolder name becomes a namespace-like prefix in includes, which scales cleanly as you add new subfolders during Phase 1+ (you write `#include "passes/PrimaryVisibilityPass.hpp"` and no further configuration is needed).

5. Confirm `$(VULKAN_SDK)\Include` is also still in this list (from step 0.2)
6. Click **OK**

Build the project. It should compile cleanly. If you get "cannot open include file" errors, the include path is wrong — recheck the spelling and that you used `$(ProjectDir)include` (not `$(ProjectDir)\include` with a leading backslash, which can cause issues in some MSBuild configurations).

## 0.6 — Move Legacy Shader to `shaders/legacy/`

You should have already moved the files in step 0.3. This section just confirms why this matters and what to verify.

### Why two shader folders

The renderer uses a dual-pipeline approach during the refactor, controlled by a feature flag in C++:

```cpp
// In VulkanCore / Renderer::render()
if (useLegacyRenderer) {
    legacyPass.execute(cmd, scene, cam, swapchainImage);
} else {
    // New pipeline: primary → probeAlloc → trace → merge → gather → tonemap
    primary.execute(...);
    // ...
}
```

**`shaders/legacy/`** — the frozen original monolithic shader. This is the known-good reference. `legacyPass` always dispatches this. **Do not modify it.** It lets you toggle back to a working renderer at any point during the refactor, and serves as the ground-truth comparison when validating new pipeline output.

**`shaders/monolith/`** — a copy of the original that is actively developed during Phase 1. This is where `#include` directives replace the inline definitions, and where the common headers are validated. The `else` branch of the toggle dispatches this during Phase 1 (it is still monolithic, but uses the new headers). From Phase 2 onward, the `else` branch is replaced by actual pass shaders, and `monolith/` gets deleted.

When the refactor completes — primary visibility, probe trace, cascade merge, final gather, and tonemap are all working — you delete both `legacy/` and `monolith/` in one commit titled something like "Remove legacy and monolith shaders; new pipeline canonical." Clean ending to the refactor saga.

### Verify

```bat
dir shaders\legacy
dir shaders\monolith
```

Both should show `raytracer.comp` and (after running `compile_shaders.bat` in the next step) `raytracer.comp.spv`. If the `.spv` files are missing, that's expected — the next step regenerates them.

### Update the shader path in C++ code

The C++ side loads the compiled SPIR-V via a path. With the dual-pipeline approach, `VulkanCore.cpp` needs to load both shaders and expose a toggle. In `createComputePipeline` or equivalent:

```cpp
// Legacy pipeline — frozen reference
auto legacyShaderCode  = readFile("shaders/legacy/raytracer.comp.spv");

// Monolith pipeline — Phase 1 refactored version (replaced by pass shaders in Phase 2+)
auto monolithShaderCode = readFile("shaders/monolith/raytracer.comp.spv");
```

And in the render loop:

```cpp
if (useLegacyRenderer) {
    legacyPass.execute(cmd, scene, cam, swapchainImage);
} else {
    monolithPass.execute(cmd, scene, cam, swapchainImage); // Phase 1
    // Phase 2+: replace with primary → probeAlloc → trace → merge → gather → tonemap
}
```

`useLegacyRenderer` can be a `bool` member of `VulkanCore` toggled at startup or via a keyboard shortcut.

## 0.7 — New Shader Compilation Script

Replace the contents of `compile_shaders.bat` (located at `Vulkan-Engine/compile_shaders.bat`) with this:

```bat
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
set FOLDERS=legacy monolith visibility rc shading tonemap

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
```

### What changed vs. v1

The v1 script (`glslc shaders/raytracer.comp -o shaders/raytracer.comp.spv`) compiles exactly one shader. The new version:

- **Walks every shader subfolder** (`legacy`, `monolith`, `visibility`, `rc`, `shading`, `tonemap`) and compiles every `.comp`/`.vert`/`.frag` it finds. As you add new pass shaders during Phase 1+ they're picked up automatically — no script edits needed.
- **Adds the `-I "shaders/common"` flag** so shaders can `#include` shared headers like `ray.glsl` and `octahedral.glsl` (which we'll create in Phase 1). Without this flag the moment you add a header, every shader that includes it will fail to compile.
- **Targets Vulkan 1.3** (`--target-env=vulkan1.3`) since we bumped the API version (done explicitly later in Part A Bug 4 — but the shader compiler needs to know now).
- **Skips `shaders/common/`** intentionally — files in there are headers (`.glsl` extension), not standalone shaders. They get included by other files, not compiled directly.
- **Reports errors per-shader** instead of stopping at the first failure. Lets you see all problems in one pass.
- **Verifies `glslc` is on PATH** with a friendly error message if the Vulkan SDK isn't installed correctly.

### Test the script

From a terminal in `Vulkan-Engine/`:

```bat
compile_shaders.bat
```

Expected output:

```
Compiling shaders for Vulkan 1.3...

[legacy]
  raytracer.comp

Success - all shaders compiled.
```

If `glslc` isn't found, reopen the terminal (PATH might not be refreshed after the SDK install). If the shader fails to compile but worked before, it's almost certainly the `--target-env=vulkan1.3` flag exposing an issue — `glslc` 1.3 mode is stricter about some constructs. Address those errors in Part A; for now reverting to `--target-env=vulkan1.2` is a valid temporary workaround.

### Add the .spv path to .gitignore

Compiled SPIR-V is a build artifact and shouldn't be in version control. Add to `.gitignore`:

```
# Compiled shaders
shaders/**/*.spv
```

The double-star matches any depth of subfolders. Commit `.gitignore` after this edit:

```bat
git add .gitignore
git commit -m "Ignore compiled SPIR-V output"
```

Then untrack any `.spv` files that were previously committed:

```bat
git rm --cached shaders/legacy/raytracer.comp.spv
git commit -m "Stop tracking compiled SPIR-V"
```

The files stay on disk; they're just not tracked by git anymore.

## 0.8 — Build Verification Before Part A

Before declaring Part 0 complete and moving on, run through this verification list. **Do not skip checks.** Every failing check means a hidden problem that will surface as confusing errors later in the refactor.

### Verification checklist

- [ ] `echo %VULKAN_SDK%` returns a valid SDK path (e.g. `C:\VulkanSDK\1.4.321.1`)
- [ ] `where glslc` returns the SDK's glslc path
- [ ] `Vulkan-Engine/lib/vulkan/` no longer exists (or `lib/` is gone entirely)
- [ ] Folder structure matches the target in section 0.3
- [ ] `compile_shaders.bat` runs successfully and produces `shaders/legacy/raytracer.comp.spv`
- [ ] Visual Studio opens the solution with no red-X icons
- [ ] `$(VULKAN_SDK)\Include` is in C/C++ Additional Include Directories
- [ ] `$(ProjectDir)include` is in C/C++ Additional Include Directories
- [ ] `$(VULKAN_SDK)\Lib` is in Linker Library Directories
- [ ] The project builds in **Debug x64**
- [ ] The project builds in **Release x64**
- [ ] The executable runs and produces the same visual output as before the restructure
- [ ] No new warnings about missing files or include paths

### Commit

If all checks pass, commit the foundation:

```bat
git add -A
git commit -m "Part 0 complete: project restructured for refactor

- Migrated from bundled lib/vulkan to system VULKAN_SDK
- Split source/headers into include/ and source/ subdirectories
- Moved GPUData/ImageLoader/ModelLoader to include/scene
- Moved MathUtils to include/math
- Repaired Visual Studio project file references
- Updated Additional Include Directories to $(ProjectDir)include
- Relocated raytracer.comp to shaders/legacy/ for transition
- Rewrote compile_shaders.bat to walk subfolders with Vulkan 1.3 target
- Project builds and runs identically to before restructure"
```

You're now ready for Part A.

### What did not change in Part 0

Importantly, Part 0 did *not* change:

- Any rendering behaviour
- Any visual output
- The Vulkan API version targeted by C++ code (still 1.2 — that's Part A Bug 4)
- Any shader source code logic
- Any pipeline / descriptor set layouts
- Any feature

Part 0 was pure foundation work. Every following phase will change behaviour. That sequencing is intentional: if you encounter visual bugs in Part A, you know they came from Part A, not from misconfigured paths in Part 0.

---

# Part A — Setup

## 1. The Branching Strategy with Your Teammate

You said your teammate is OK with the refactor and wants to work on caustics in parallel. Coordinate via git:

```bash
# Start from the current main
git checkout main
git pull

# Create the refactor branch
git checkout -b refactor/radiance-cascades

# Push it so your teammate can see progress
git push -u origin refactor/radiance-cascades
```

**Rules of engagement**:

1. **Your teammate's caustics work stays on `main` or a `feature/caustics` branch.** Don't merge their caustics until your RC refactor is stable.
2. **Communicate before touching shared files.** If you're rewriting `raytracer.comp`, mention it in your group chat so they don't fork at a bad moment.
3. **Merge `main` into your branch weekly** to avoid catastrophic conflicts. Resolve aggressively in your favor on RC-related files; defer to their changes on caustics-only files.
4. **Both branches will need to merge eventually** — design your code so the RC pipeline produces a separate "indirect light" output buffer that caustics can read from. That's the natural integration point.

### Suggested Repository Layout (After Refactor)

```
RT-CPP-Raytracer/
├── Vulkan-Engine/
│   ├── src/
│   │   ├── core/              ← Vulkan boilerplate (extracted from VulkanCore)
│   │   ├── scene/             ← ModelLoader, BVH, Materials, Lights
│   │   ├── gi/                ← NEW: GI module interface + RC implementation
│   │   ├── passes/            ← NEW: each render pass as a class
│   │   ├── debug/             ← NEW: ImGui debug panels
│   │   └── main.cpp
│   ├── shaders/
│   │   ├── common/            ← NEW: shared GLSL headers
│   │   ├── visibility/        ← NEW: primary visibility (G-buffer fill)
│   │   ├── rc/                ← NEW: cascade allocation, trace, merge
│   │   ├── shading/           ← NEW: final gather, composite
│   │   └── tonemap/           ← NEW: tonemapping
│   └── assets/
│       ├── models/            ← existing OBJ files
│       ├── textures/          ← existing texture folders
│       └── scenes/            ← NEW: scene config files (showcase scenes)
```

## 2. Pre-Refactor Cleanup (Bugs to Fix First)

These are issues I spotted in the current code that should be fixed *before* the RC refactor starts. They're small, low-risk, and they'll prevent confusion later.

### Bug 1: BGR Channel Swap on Output

In `raytracer.comp` line 972:

```glsl
// CURRENT (wrong)
imageStore(resultImage, pixelCoords, vec4(finalColor.b, finalColor.g, finalColor.r, 1.0));

// FIX
imageStore(resultImage, pixelCoords, vec4(finalColor.r, finalColor.g, finalColor.b, 1.0));
```

Then check what swapchain format you're using. Look in `VulkanCore::createSwapchain()`:

```cpp
// CURRENT — find this and replace with explicit RGBA
// surfaceFormat.format = ???

// FIX — be explicit
surfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
```

If `VK_FORMAT_R8G8B8A8_UNORM` is unsupported on your hardware (rare), fall back to `VK_FORMAT_B8G8R8A8_UNORM` *and* leave the BGR swap in the shader. Either way, make the choice explicit and document it in a comment.

### Bug 2: No Tonemapping

The shader outputs raw HDR values to an 8-bit BGRA image. Anything above 1.0 clips to white. With Radiance Cascades and emissive surfaces, you will absolutely see clipped highlights.

**Quick fix in the existing shader** (until you split into passes):

```glsl
// At the very end of main(), replace the imageStore with:
vec3 finalColor = accumulatedColor / float(cam.samplesPerPixel);

// Apply Reinhard tonemap
vec3 tonemapped = finalColor / (1.0 + finalColor);

// Apply gamma correction (linear → sRGB)
tonemapped = pow(tonemapped, vec3(1.0 / 2.2));

imageStore(resultImage, pixelCoords, vec4(tonemapped, 1.0));
```

This is the absolute minimum. We'll replace this with proper ACES tonemapping during the refactor (Phase 10).

### Bug 3: Non-Triangle Primitives Don't Participate in Indirect Light

Your scene has spheres, planes, quads, cubes — all tested in O(n) loops in the shader, none in the BVH. For RC, **probes need to see all light-contributing geometry**, so any emissive sphere/cube must be in the same acceleration structure as triangles.

**Decision** (do this during cleanup):

- Keep using these primitives in `main.cpp` for scene authoring convenience (it's easier to write `spheres.push_back(...)` than to model a sphere)
- Add a **tessellation step** that converts them to triangles before scene upload
- After tessellation, only triangles + BVH exist on the GPU

Implementation in Section 5.

### Bug 4: API Version

Bump from 1.2 to 1.3. In `VulkanCore::createInstance()`:

```cpp
// CURRENT
appInfo.apiVersion = VK_API_VERSION_1_2;

// FIX
appInfo.apiVersion = VK_API_VERSION_1_3;
```

This unlocks `synchronization2`, `dynamicRendering`, and cleaner descriptor indexing — all useful for the refactor. Confirm with `vkGetPhysicalDeviceProperties` that the device supports 1.3 (any GPU from 2017 onwards should).

### Bug 5: Samples Per Pixel Semantics

When RC is in place, indirect light is *deterministic and noiseless*. Re-sampling it `samplesPerPixel` times produces identical results.

**Refactor**: split `samplesPerPixel` into two settings:
- `primaryRaysPerPixel` (for anti-aliasing primary rays) — keep
- `indirectSamples` (for RC's final gather) — remove; RC needs only 1

Update `CameraPushConstants` and `main.cpp` accordingly.

### Bug 6: Stack Size in BVH Traversal

The shader uses a stack of size 64 for BVH traversal:

```glsl
int stack[64];
```

For your current scenes this is fine, but make it a `#define` so you can tune it:

```glsl
#define BVH_STACK_SIZE 64
int stack[BVH_STACK_SIZE];
```

Push it to 128 if you ever see crashes/black pixels on deep scenes.

### Cleanup Checklist

Commit each of these as a separate PR/commit so they're easy to review:

- [ ] Commit "Fix RGB channel order on output"
- [ ] Commit "Add basic Reinhard tonemapping"  
- [ ] Commit "Bump API version to 1.3"
- [ ] Commit "Make BVH stack size configurable"
- [ ] Commit "Add primitive tessellation step" (Section 5)
- [ ] Commit "Split SPP into primary/indirect"

After these, the project is **architecturally identical** but visually correct. Now we can start the real refactor.

## 3. Build Configuration Updates

> **Note (v2)**: The folder restructure, Vulkan SDK migration, and Visual Studio project file repair are now covered in **Part 0** and should already be complete. This section only covers the optional library dependencies you may want to add during the refactor.

### Dependencies You May Want to Add

These aren't strictly required for the showcase but make the refactor smoother. Add them when you reach the relevant phase, not all upfront.

- **Vulkan Memory Allocator (VMA)** — AMD GPUOpen's header-only library for buffer/image memory management. Eliminates manual `vkAllocateMemory`/`vkBindBufferMemory` calls. **Add during Phase 3** when you build out the `resources/Buffer` and `resources/Image` wrappers.
  - Source: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  - Add as: drop `vk_mem_alloc.h` into `Vulkan-Engine/third_party/vma/include/` and add that path to Additional Include Directories. Then in **one** `.cpp` file (e.g. a new `source/core/VMA.cpp`) define `VMA_IMPLEMENTATION` before including it.

- **vk-bootstrap** — Charles Giessen's library that compresses Vulkan instance/device creation from ~500 lines to ~30. **Add during Phase 3** when splitting `VulkanCore`.
  - Source: https://github.com/charles-lunarg/vk-bootstrap
  - Add as: drop `VkBootstrap.h` and `VkBootstrap.cpp` into `third_party/vk-bootstrap/` and add both to the project. Header-only-ish (one .cpp file).

- **nlohmann/json** — for the JSON scene loader added in Phase 8 (demo scene authoring).
  - Source: https://github.com/nlohmann/json (single header `json.hpp`)
  - Add as: drop into `third_party/nlohmann/` and include `nlohmann/json.hpp`.

- **stb_image** — already in your project (used by `ImageLoader.cpp`). No change.
- **GLM** — already in your project. No change.
- **ImGui** — already in your project. Confirm `imgui_impl_vulkan.cpp` and `imgui_impl_glfw.cpp` are integrated.

### How to Add Third-Party Libraries to a Visual Studio Project

For each header-only library above:

1. Create `Vulkan-Engine/third_party/<library_name>/` and drop the source files there
2. Right-click project → **Properties** → **C/C++** → **General** → **Additional Include Directories**: add `$(ProjectDir)third_party\<library_name>`
3. If the library has `.cpp` files (e.g. vk-bootstrap), right-click project → **Add → Existing Item** and select them

That's it. No CMake, no package manager. Crude but it works for header-light libraries.

### Vulkan API Version

The C++ side currently targets `VK_API_VERSION_1_2`. This will be bumped to `VK_API_VERSION_1_3` in **Part A Bug 4**, not here. Keep the shader compiler at `--target-env=vulkan1.3` (set in Part 0.7) — it's fine for the shaders to be ahead of the API call version temporarily.

## 4. Folder Restructure

> **Note (v2)**: This section is now covered in **Part 0.3** (folder layout) and **Part 0.4** (Visual Studio project repair). If you arrived here without doing Part 0, jump back and complete it first. The remainder of this section is preserved for cross-reference only.

The folder layout established in Part 0 (`include/` + `source/` split with subfolder organization) is the canonical structure for the rest of the refactor. Each subsequent phase places new files into specific subfolders:

| Phase | Files added | Goes to |
|---|---|---|
| Phase 1 (G-buffer) | `PrimaryVisibilityPass.{hpp,cpp}`, common GLSL headers | `include/passes/`, `source/passes/`, `shaders/common/`, `shaders/visibility/` |
| Phase 1 (tessellation) | `Primitive.{hpp,cpp}`, `BVHBuilder.{hpp,cpp}` | `include/scene/`, `source/scene/` |
| Phase 3 (Vulkan split) | `VulkanContext`, `Swapchain`, `CommandManager`, `Buffer`, `Image`, `Pipeline`, `ShaderModule` | `include/core/`, `include/resources/`, `source/core/`, `source/resources/` |
| Phase 4 (render graph) | `Renderer.{hpp,cpp}` | `include/`, `source/` (project root level inside those folders) |
| Phase 5 (cascade data) | `CascadeStorage.{hpp,cpp}`, `CascadeConfig.hpp` | `include/gi/`, `source/gi/` |
| Phase 6 (probe trace) | `ProbeTracePass.{hpp,cpp}`, `probe_trace.comp` | `include/passes/`, `source/passes/`, `shaders/rc/` |
| Phase 8 (merge) | `CascadeMergePass.{hpp,cpp}`, `cascade_merge.comp` | `include/passes/`, `source/passes/`, `shaders/rc/` |
| Phase 9 (final gather) | `FinalGatherPass.{hpp,cpp}`, `final_gather.comp` | `include/passes/`, `source/passes/`, `shaders/shading/` |
| Phase 10 (tonemap) | `TonemapPass.{hpp,cpp}`, `tonemap.comp` | `include/passes/`, `source/passes/`, `shaders/tonemap/` |
| Phase 11 (debug) | `ImGuiLayer.{hpp,cpp}`, `ProfilerOverlay.{hpp,cpp}` | `include/debug/`, `source/debug/` |
| Phase 12 (sparse migration, dissertation only) | `HashMap.{hpp,cpp}`, `ProbeAllocator.{hpp,cpp}`, allocation shader | `include/gi/`, `source/gi/`, `shaders/rc/` |

When in doubt about where a new file goes, this table is the answer. If a file doesn't fit any of these categories, add a new subfolder rather than dumping it somewhere that doesn't fit conceptually.

## 5. Asset Preparation: Primitive Tessellation

Add a `Primitive.cpp` file with tessellation helpers. The goal: at scene load time, convert all non-triangle primitives to triangle meshes so they participate in the BVH.

### Sphere Tessellation (UV Sphere)

```cpp
// scene/Primitive.cpp

void tessellateSphere(
    const GPUSphere& sphere,
    int materialIndex,
    std::vector<GPUTriangle>& outTriangles,
    int segmentsU = 16,
    int segmentsV = 12)
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    
    for (int v = 0; v <= segmentsV; v++) {
        float phi = M_PI * v / segmentsV;
        for (int u = 0; u <= segmentsU; u++) {
            float theta = 2.0f * M_PI * u / segmentsU;
            glm::vec3 normal(
                sin(phi) * cos(theta),
                cos(phi),
                sin(phi) * sin(theta)
            );
            vertices.push_back(sphere.center + sphere.radius * normal);
            normals.push_back(normal);
        }
    }
    
    auto idx = [&](int u, int v) { return v * (segmentsU + 1) + u; };
    
    for (int v = 0; v < segmentsV; v++) {
        for (int u = 0; u < segmentsU; u++) {
            // Two triangles per quad
            GPUTriangle t1{};
            t1.v0 = vertices[idx(u,   v  )]; t1.n0 = normals[idx(u,   v  )];
            t1.v1 = vertices[idx(u+1, v  )]; t1.n1 = normals[idx(u+1, v  )];
            t1.v2 = vertices[idx(u,   v+1)]; t1.n2 = normals[idx(u,   v+1)];
            t1.isSmooth = 1;
            t1.materialIndex = materialIndex;
            outTriangles.push_back(t1);
            
            GPUTriangle t2{};
            t2.v0 = vertices[idx(u+1, v  )]; t2.n0 = normals[idx(u+1, v  )];
            t2.v1 = vertices[idx(u+1, v+1)]; t2.n1 = normals[idx(u+1, v+1)];
            t2.v2 = vertices[idx(u,   v+1)]; t2.n2 = normals[idx(u,   v+1)];
            t2.isSmooth = 1;
            t2.materialIndex = materialIndex;
            outTriangles.push_back(t2);
        }
    }
}
```

For your scenes, `segmentsU=16, segmentsV=12` gives ~380 triangles per sphere. With 20 spheres in main.cpp, that's ~7600 triangles total — negligible for BVH performance.

### Cube Tessellation

```cpp
void tessellateCube(
    const GPUCube& cube,
    int materialIndex,
    std::vector<GPUTriangle>& outTriangles)
{
    glm::vec3 halfExtents = (cube.boundsMax - cube.boundsMin) * 0.5f;
    glm::vec3 center = cube.center + (cube.boundsMin + cube.boundsMax) * 0.5f;
    
    // 8 corners of the box in local space
    glm::vec3 corners[8];
    for (int i = 0; i < 8; i++) {
        corners[i] = glm::vec3(
            (i & 1) ? halfExtents.x : -halfExtents.x,
            (i & 2) ? halfExtents.y : -halfExtents.y,
            (i & 4) ? halfExtents.z : -halfExtents.z
        );
        // Apply rotation
        corners[i] = MathUtils::rotateVec(corners[i], cube.rotation);
        corners[i] += center;
    }
    
    // 6 faces, 2 triangles each = 12 triangles
    // Face indices (CCW winding from outside)
    static const int faces[6][4] = {
        {0,2,3,1}, // -Z
        {4,5,7,6}, // +Z
        {0,4,6,2}, // -X
        {1,3,7,5}, // +X
        {0,1,5,4}, // -Y
        {2,6,7,3}, // +Y
    };
    static const glm::vec3 faceNormals[6] = {
        {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}
    };
    
    for (int f = 0; f < 6; f++) {
        glm::vec3 normal = MathUtils::rotateVec(faceNormals[f], cube.rotation);
        
        GPUTriangle t1{};
        t1.v0 = corners[faces[f][0]];
        t1.v1 = corners[faces[f][1]];
        t1.v2 = corners[faces[f][2]];
        t1.n0 = t1.n1 = t1.n2 = normal;
        t1.isSmooth = 0;
        t1.materialIndex = materialIndex;
        outTriangles.push_back(t1);
        
        GPUTriangle t2{};
        t2.v0 = corners[faces[f][0]];
        t2.v1 = corners[faces[f][2]];
        t2.v2 = corners[faces[f][3]];
        t2.n0 = t2.n1 = t2.n2 = normal;
        t2.isSmooth = 0;
        t2.materialIndex = materialIndex;
        outTriangles.push_back(t2);
    }
}
```

### Quad and Plane Tessellation

```cpp
void tessellateQuad(
    const GPUQuad& quad,
    int materialIndex,
    std::vector<GPUTriangle>& outTriangles)
{
    glm::vec3 v0 = quad.corner;
    glm::vec3 v1 = quad.corner + quad.edge1;
    glm::vec3 v2 = quad.corner + quad.edge1 + quad.edge2;
    glm::vec3 v3 = quad.corner + quad.edge2;
    
    GPUTriangle t1{};
    t1.v0 = v0; t1.v1 = v1; t1.v2 = v2;
    t1.n0 = t1.n1 = t1.n2 = quad.normalVector;
    t1.isSmooth = 0;
    t1.materialIndex = materialIndex;
    outTriangles.push_back(t1);
    
    GPUTriangle t2{};
    t2.v0 = v0; t2.v1 = v2; t2.v2 = v3;
    t2.n0 = t2.n1 = t2.n2 = quad.normalVector;
    t2.isSmooth = 0;
    t2.materialIndex = materialIndex;
    outTriangles.push_back(t2);
}

void tessellatePlane(
    const GPUPlane& plane,
    int materialIndex,
    std::vector<GPUTriangle>& outTriangles,
    float extent = 1000.0f)
{
    // Convert infinite plane to a very large quad
    glm::vec3 n = plane.normalVector;
    glm::vec3 tangent = abs(n.y) < 0.99f
                        ? glm::normalize(glm::cross(n, glm::vec3(0,1,0)))
                        : glm::normalize(glm::cross(n, glm::vec3(1,0,0)));
    glm::vec3 bitangent = glm::cross(n, tangent);
    
    GPUQuad bigQuad{};
    bigQuad.corner = plane.center - tangent * extent - bitangent * extent;
    bigQuad.edge1 = tangent * extent * 2.0f;
    bigQuad.edge2 = bitangent * extent * 2.0f;
    bigQuad.normalVector = n;
    bigQuad.materialIndex = materialIndex;
    
    tessellateQuad(bigQuad, materialIndex, outTriangles);
}
```

### Wire It Into Scene Loading

In `main.cpp`, after all the primitive `push_back` calls, before `loadScene()`:

```cpp
// At end of main, after spheres/cubes/quads/planes are populated:
for (const auto& sphere : spheres) {
    tessellateSphere(sphere, sphere.materialIndex, triangles);
}
for (const auto& cube : cubes) {
    tessellateCube(cube, cube.materialIndex, triangles);
}
for (const auto& quad : quads) {
    tessellateQuad(quad, quad.materialIndex, triangles);
}
for (const auto& plane : planes) {
    tessellatePlane(plane, plane.materialIndex, triangles);
}

// Clear the non-triangle lists — they no longer go to the GPU
spheres.clear();
cubes.clear();
quads.clear();
planes.clear();

// Rebuild BVH from unified triangle pool
ModelLoader::buildBVH(triangles, bvhNodes);

vulkan.loadScene(materials, spheres, triangles, lights, planes, quads, cubes, bvhNodes);
```

**Now all geometry is triangles, all in the BVH, and ray tracing the BVH is sufficient to find any surface in the scene.** This is the precondition for cascades.

### Visual Sanity Check

Before moving on, render the scene with your existing direct-lighting shader after tessellation. The image should look **identical** to before — same spheres, same shadows, same colors. If you see differences, debug the tessellation before continuing. Common issues:

- **Wrong winding order**: triangles facing inward. Fix by swapping v1/v2.
- **Wrong normals**: cube faces have wrong shading. Fix the `faceNormals` array.
- **Missing materials**: triangles use a wrong material index. Check the loop variable.

Commit this as **"Tessellate all primitives to triangles; remove non-triangle SSBOs"**.

## 5b. (Optional) SAH BVH for Better Performance

If you have time before the showcase, swap the median-split BVH for binned SAH. This is a self-contained CPU-side improvement that doesn't touch the shader.

In `BVHBuilder.cpp`, add an alternative `subdivideSAH()` that replaces median split:

```cpp
struct BinInfo {
    AABB bounds;
    int count = 0;
};

void BVHBuilder::subdivideSAH(int nodeIdx, std::vector<GPUBVHNode>& bvhNodes, 
                               std::vector<GPUTriangle>& triangles, int& nodesUsed) 
{
    auto& node = bvhNodes[nodeIdx];
    if (node.triCount <= 4) return; // leaf
    
    const int NUM_BINS = 16;
    float bestCost = std::numeric_limits<float>::infinity();
    int bestAxis = -1;
    float bestSplit = 0;
    
    for (int axis = 0; axis < 3; axis++) {
        BinInfo bins[NUM_BINS] = {};
        
        // Determine centroid bounds along this axis
        float axisMin = std::numeric_limits<float>::infinity();
        float axisMax = -std::numeric_limits<float>::infinity();
        for (int i = 0; i < node.triCount; i++) {
            float c = triangleCentroid(triangles[node.leftFirst + i])[axis];
            axisMin = std::min(axisMin, c);
            axisMax = std::max(axisMax, c);
        }
        if (axisMax - axisMin < 1e-6f) continue; // degenerate axis
        
        float scale = float(NUM_BINS) / (axisMax - axisMin);
        
        // Bin all triangles
        for (int i = 0; i < node.triCount; i++) {
            const auto& tri = triangles[node.leftFirst + i];
            float c = triangleCentroid(tri)[axis];
            int b = std::min(NUM_BINS - 1, int((c - axisMin) * scale));
            bins[b].count++;
            bins[b].bounds = unionAABB(bins[b].bounds, triangleAABB(tri));
        }
        
        // Sweep left and right
        AABB leftBounds, rightBounds;
        int leftCount[NUM_BINS], rightCount[NUM_BINS];
        AABB leftAABB[NUM_BINS], rightAABB[NUM_BINS];
        
        int leftSum = 0;
        AABB leftAcc;
        for (int i = 0; i < NUM_BINS; i++) {
            leftSum += bins[i].count;
            leftCount[i] = leftSum;
            leftAcc = unionAABB(leftAcc, bins[i].bounds);
            leftAABB[i] = leftAcc;
        }
        
        int rightSum = 0;
        AABB rightAcc;
        for (int i = NUM_BINS - 1; i >= 0; i--) {
            rightSum += bins[i].count;
            rightCount[i] = rightSum;
            rightAcc = unionAABB(rightAcc, bins[i].bounds);
            rightAABB[i] = rightAcc;
        }
        
        // Evaluate SAH for each split position
        for (int splitBin = 0; splitBin < NUM_BINS - 1; splitBin++) {
            if (leftCount[splitBin] == 0 || rightCount[splitBin + 1] == 0) continue;
            
            float cost = 1.0f /* C_trav */
                + (surfaceArea(leftAABB[splitBin]) / surfaceArea(node)) * leftCount[splitBin]
                + (surfaceArea(rightAABB[splitBin + 1]) / surfaceArea(node)) * rightCount[splitBin + 1];
            
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestSplit = axisMin + (splitBin + 1) / scale;
            }
        }
    }
    
    if (bestAxis == -1) return; // can't improve, leaf
    
    // Compare to "no split" cost
    float noSplitCost = float(node.triCount);
    if (bestCost >= noSplitCost) return; // leaf
    
    // Partition triangles
    int i = node.leftFirst;
    int j = i + node.triCount - 1;
    while (i <= j) {
        if (triangleCentroid(triangles[i])[bestAxis] < bestSplit) {
            i++;
        } else {
            std::swap(triangles[i], triangles[j]);
            j--;
        }
    }
    
    int leftCount = i - node.leftFirst;
    if (leftCount == 0 || leftCount == node.triCount) return;
    
    // Recursive subdivide
    int leftChild = nodesUsed++;
    int rightChild = nodesUsed++;
    
    bvhNodes[leftChild].leftFirst = node.leftFirst;
    bvhNodes[leftChild].triCount = leftCount;
    updateNodeBounds(leftChild, bvhNodes, triangles);
    
    bvhNodes[rightChild].leftFirst = i;
    bvhNodes[rightChild].triCount = node.triCount - leftCount;
    updateNodeBounds(rightChild, bvhNodes, triangles);
    
    node.leftFirst = leftChild;
    node.triCount = 0;
    
    subdivideSAH(leftChild, bvhNodes, triangles, nodesUsed);
    subdivideSAH(rightChild, bvhNodes, triangles, nodesUsed);
}
```

Add an `enum { MEDIAN_SPLIT, BINNED_SAH }` choice in `BVHBuilder::Settings` and switch at the top of `subdivide()`. The GPU shader code doesn't change at all.

**For the showcase**: median split is fine if scenes are <50K triangles. SAH becomes valuable only at higher complexity. Defer if time-constrained.

---

# Part B — Architectural Refactor

> **Note (Implementation-Validated):** This section has been updated to reflect what was actually built and tested during the refactor session, including critical bugs discovered and their fixes. The original guide text has been retained and expanded with real pitfalls, measured results, and implementation decisions that were not captured in the planning phase. If something in this section contradicts the earlier plan text, trust this section — it represents ground truth.

## 6. Phase 1: Extract the Mega-Shader into Modules

The current `raytracer.comp` is ~970 lines doing everything. The goal is to split it into reusable GLSL headers that both the monolith and future pass shaders can share.

**Working copy: `shaders/monolith/raytracer.comp`** — this is the file you modify during Phase 1. `shaders/legacy/raytracer.comp` stays frozen. All `#include` directive additions, struct removals, and function extractions happen in the monolith copy only.

### Critical Pitfall: Unicode / UTF-16 BOM in GLSL Files

**Never use a text editor that saves `.comp` or `.glsl` files as UTF-16 with BOM.** Visual Studio, by default, saves files detected as "non-ASCII" in UTF-16 LE with BOM. `glslc` reads shader files as raw bytes and chokes on the BOM (`\xFF\xFE`), producing cryptic "unexpected character" errors on line 1 even when the GLSL source is syntactically valid.

**Fix:** In Visual Studio, every time you create a new `.comp` or `.glsl` file, immediately go to **File → Save [filename] As → Save with Encoding → UTF-8** (no BOM). Alternatively, create files using a terminal (`type nul > shader.comp`) and then open them in VS.

This applies to every file in `shaders/common/`, `shaders/visibility/`, `shaders/rc/`, `shaders/shading/`, and `shaders/tonemap/`. One UTF-16 file silently corrupts the entire include chain.

### Create the Common Header Directory

```
shaders/common/
├── ray.glsl           ← Ray struct, intersection functions
├── bvh.glsl           ← BVH traversal
├── material.glsl      ← Material struct + texture sampling
├── lighting.glsl      ← Lambert, Blinn-Phong, GGX PBR
├── random.glsl        ← PCG random number generation
├── octahedral.glsl    ← Direction encoding
└── push_constants.glsl ← Camera + scene push constants
```

### `shaders/common/ray.glsl`

```glsl
#ifndef RAY_GLSL
#define RAY_GLSL

struct Ray {
    vec3 origin;
    vec3 direction;
    float tMin;
    float tMax;
};

struct HitInfo {
    float t;            // -1 if no hit
    vec3 position;
    vec3 normal;
    vec2 uv;
    int materialIndex;
};

float intersectAABB(Ray ray, vec3 invDir, vec3 bMin, vec3 bMax);
float sphereIntersect(Ray ray, vec3 center, float radius);
float triangleIntersect(Ray ray, vec3 v0, vec3 v1, vec3 v2, out float u, out float v);

#endif
```

Then move the implementations of those functions out of `raytracer.comp` and into this header. Each new shader can `#include` it.

### `shaders/common/bvh.glsl`

```glsl
#ifndef BVH_GLSL
#define BVH_GLSL

#include "ray.glsl"

struct GPUBVHNode {
    vec3 aabbMin;
    int leftFirst;
    vec3 aabbMax;
    int triCount;
};

struct GPUTriangle {
    vec3 v0;
    float p1;
    vec3 v1;
    float p2;
    vec3 v2;
    float p3;
    vec3 n0;
    float p4;
    vec3 n1;
    float p5;
    vec3 n2;
    int isSmooth;
    int materialIndex;
    float p6;
    float p7;
    float p8;
};

// Bindings — assumes set=0, binding=8 for BVH and binding=3 for triangles
// Caller defines these via descriptor set layout
layout(std430, set = 0, binding = 8) readonly buffer BVHBuffer { GPUBVHNode bvhNodes[]; };
layout(std430, set = 0, binding = 3) readonly buffer TriangleBuffer { GPUTriangle triangles[]; };

#define BVH_STACK_SIZE 64

bool traverseBVH(Ray ray, int bvhNodeCount, out HitInfo hit) {
    hit.t = ray.tMax;
    int hitIndex = -1;
    float hitU = 0, hitV = 0;
    
    if (bvhNodeCount == 0) return false;
    
    int stack[BVH_STACK_SIZE];
    int stackPtr = 0;
    stack[stackPtr++] = 0;
    vec3 invDir = 1.0 / ray.direction;
    
    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        GPUBVHNode node = bvhNodes[nodeIdx];
        
        float distAABB = intersectAABB(ray, invDir, node.aabbMin, node.aabbMax);
        if (distAABB >= hit.t) continue;
        
        if (node.triCount > 0) {
            for (int i = 0; i < node.triCount; i++) {
                int triIdx = node.leftFirst + i;
                GPUTriangle tri = triangles[triIdx];
                float u, v;
                float t = triangleIntersect(ray, tri.v0, tri.v1, tri.v2, u, v);
                if (t > ray.tMin && t < hit.t) {
                    hit.t = t;
                    hitIndex = triIdx;
                    hitU = u;
                    hitV = v;
                }
            }
        } else {
            if (stackPtr + 2 <= BVH_STACK_SIZE) {
                stack[stackPtr++] = node.leftFirst;
                stack[stackPtr++] = node.leftFirst + 1;
            }
        }
    }
    
    if (hitIndex == -1) return false;
    
    GPUTriangle tri = triangles[hitIndex];
    float w = 1.0 - hitU - hitV;
    
    hit.position = ray.origin + ray.direction * hit.t;
    if (tri.isSmooth == 1) {
        hit.normal = normalize(w * tri.n0 + hitU * tri.n1 + hitV * tri.n2);
    } else {
        hit.normal = normalize(cross(tri.v1 - tri.v0, tri.v2 - tri.v0));
    }
    hit.uv = vec2(hitU, hitV);
    hit.materialIndex = tri.materialIndex;
    
    return true;
}

#endif
```

### `shaders/common/octahedral.glsl`

```glsl
#ifndef OCTAHEDRAL_GLSL
#define OCTAHEDRAL_GLSL

vec2 octEncode(vec3 dir) {
    dir /= (abs(dir.x) + abs(dir.y) + abs(dir.z));
    vec2 uv = (dir.z >= 0.0)
              ? dir.xy
              : (vec2(1.0) - abs(dir.yx)) * sign(dir.xy);
    return uv * 0.5 + 0.5;
}

vec3 octDecode(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 dir = vec3(uv.xy, 1.0 - abs(uv.x) - abs(uv.y));
    if (dir.z < 0.0) {
        dir.xy = (1.0 - abs(dir.yx)) * sign(dir.xy);
    }
    return normalize(dir);
}

// Sample uv from octahedral resolution index
vec2 octIndexToUV(ivec2 idx, int resolution) {
    return (vec2(idx) + 0.5) / float(resolution);
}

#endif
```

### `shaders/common/random.glsl`

Contains the PI constant, the PCG random number generator, noise utilities, and disk sampling. Everything that produces random or procedural values lives here. `PI` is defined here (not duplicated elsewhere) so any header that needs it can include `random.glsl`.

```glsl
#ifndef RANDOM_GLSL
#define RANDOM_GLSL

const float PI = 3.14159265359;

uint randState;

float rand() {
    randState = randState * 747796405u + 2891336453u;
    uint word = ((randState >> ((randState >> 28u) + 4u)) ^ randState) * 277803737u;
    return float((word >> 22u) ^ word) / 4294967295.0;
}

vec2 randomPointOnDisk() {
    float angle = rand() * 2.0 * PI;
    float r = sqrt(rand());
    return vec2(r * cos(angle), r * sin(angle));
}

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
                   mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
                   mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}

float fbm(vec3 x) {
    float v = 0.0;
    float a = 0.5;
    vec3 shift = vec3(100.0);
    for (int i = 0; i < 5; ++i) {
        v += a * noise(x);
        x = x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

#endif
```

### `shaders/common/material.glsl`

Contains the `GPUMaterial` struct, its SSBO binding, the texture sampler array binding, and `getTBN()` (the tangent-space basis used for normal mapping and parallax). No dependency on other headers — it is a dependency.

Note: `nonuniformEXT` is used when *indexing* `textures[]` in the main shader. The extension declaration (`GL_EXT_nonuniform_qualifier`) stays in the compilable `.comp` file, not here.

```glsl
#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

layout(binding = 9) uniform sampler2D textures[];

struct GPUMaterial {
    vec3 color;
    float ambient;
    vec3 emission;
    float diffuse;
    vec3 color2;
    float specular;

    float reflection;
    float transparency;
    float ior;

    int shadingModel;

    int patternType;
    float roughness;
    float metallic;

    int castShadows;

    int useTexture;
    int albedoIndex;
    int normalMapIndex;
    int roughnessIndex;

    int aoIndex;
    int heightMapIndex;
    float proceduralScale;
    float proceduralWobble;

    float bumpStrength;
    float parallaxScale;
    float p5;
    float p6;
};

layout(std430, binding = 1) readonly buffer MaterialBuffer { GPUMaterial materials[]; };

mat3 getTBN(vec3 n) {
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(up, n));
    vec3 b = cross(n, t);
    return mat3(t, b, n);
}

#endif
```

### `shaders/common/lighting.glsl`

Contains `GPULight`, its SSBO binding, and all shading models: Lambertian, Blinn-Phong, and the full GGX PBR stack. Includes `random.glsl` for `PI` and `material.glsl` for `GPUMaterial` (used as a parameter in the shading functions). Include guards prevent double-inclusion when both are also included by the main shader.

```glsl
#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "random.glsl"
#include "material.glsl"

struct GPULight {
    vec3 position;
    float radius;
    vec3 color;
    float p2;
};

layout(std430, binding = 4) readonly buffer LightBuffer { GPULight lights[]; };

vec3 lambertianShading(GPUMaterial mat, vec3 baseColor, vec3 hitNormal, vec3 lightDir, vec3 lightColor) {
    return baseColor * mat.diffuse * max(dot(hitNormal, lightDir), 0.0) * lightColor;
}

vec3 blingPhongShading(GPUMaterial mat, vec3 lightColor, vec3 hitNormal, vec3 lightDir, vec3 viewDir, float specExp) {
    vec3 halfVector = normalize(lightDir + viewDir);
    return lightColor * mat.specular * pow(max(dot(hitNormal, halfVector), 0.0), specExp);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 pbrShading(GPUMaterial mat, vec3 baseColor, vec3 N, vec3 V, vec3 L, vec3 lightColor) {
    vec3 H = normalize(V + L);
    vec3 F0 = mix(vec3(0.04), baseColor, mat.metallic);

    float NDF = DistributionGGX(N, H, mat.roughness);
    float G   = GeometrySmith(N, V, L, mat.roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  specular    = numerator / denominator;

    vec3  kD    = (vec3(1.0) - F) * (1.0 - mat.metallic);
    float NdotL = max(dot(N, L), 0.0);

    return (kD * baseColor / PI + specular) * lightColor * NdotL;
}

#endif
```

### `shaders/common/push_constants.glsl`

The `CameraData` push constant block. All passes that need camera position, scene counts, or render settings include this. The output image binding (`binding = 0`) stays in each `.comp` file because it is pass-specific.

```glsl
#ifndef PUSH_CONSTANTS_GLSL
#define PUSH_CONSTANTS_GLSL

layout(push_constant) uniform CameraData {
    vec4 camPos;
    vec4 camForward;
    vec4 camRight;
    vec4 camUp;
    int sphereCount;
    int triangleCount;
    int planeCount;
    int quadCount;
    int cubeCount;
    int lightCount;
    int bvhCount;
    int maxDepth;
    int shadowRays;
    int primaryRaysPerPixel;
    float focalDistance;
    float lensRadius;

    vec3 fogColor;
    int enableFog;

    vec3 skyBottomColor;
    int enableSkybox;

    vec3 skyTopColor;
    int enableTextures;
} cam;

#endif
```

### Header dependency map

```
ray.glsl            (no deps)
bvh.glsl        →   ray.glsl
octahedral.glsl     (no deps)
random.glsl         (no deps; owns PI)
material.glsl       (no deps; owns textures[] + GPUMaterial + MaterialBuffer)
lighting.glsl   →   random.glsl, material.glsl
push_constants.glsl (no deps)
```

Include guards handle all transitive double-inclusions automatically.

### Include order in `shaders/monolith/raytracer.comp`

```glsl
#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
layout (local_size_x = 16, local_size_y = 16) in;

#include "ray.glsl"
#include "bvh.glsl"
#include "octahedral.glsl"
#include "random.glsl"
#include "material.glsl"
#include "lighting.glsl"
#include "push_constants.glsl"
```

### What stays in `shaders/monolith/raytracer.comp`

Everything not shared across future passes remains in the monolith file:

| What | Why it stays |
|---|---|
| `layout(binding = 0, rgba8) uniform writeonly image2D resultImage;` | Output image is pass-specific |
| `GPUSphere`, `GPUPlane`, `GPUQuad`, `GPUCube` structs | No shared header for scene geometry yet; extracted in Phase 2+ |
| Their SSBO bindings (2, 5, 6, 7) | Same reason |
| `rotationMatrixXYZ()` | Only needed by `cubeIntersect`; not shared |
| `intersectAABB()`, `sphereIntersect()`, `triangleIntersect()` implementations | Declared in `ray.glsl`, implemented here |
| `planeIntersect()`, `quadIntersect()`, `cubeIntersect()`, `cubeNormal()` | Monolith-specific geometry |
| `const float MIN_DISPLACEMENT` | Local render constant |
| `main()` | Entry point |

**Signature change note**: `ray.glsl` declares `sphereIntersect` and `triangleIntersect` with decomposed parameters (`vec3 center, float radius` and `vec3 v0, v1, v2`) rather than taking the full GPU struct. This makes them usable by any future shader regardless of whether it has `GPUSphere` or `GPUTriangle` in scope. The implementations in `raytracer.comp` must match these signatures, and all call sites become e.g. `sphereIntersect(ray, spheres[i].center, spheres[i].radius)`.

### Compile-Time Glslc Setup

Modern glslc supports `#include` natively:

```cmake
# In CMake, when compiling shaders:
add_custom_command(
    OUTPUT ${OUTPUT_SPIRV}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE}
        -I${CMAKE_SOURCE_DIR}/Vulkan-Engine/shaders/common
        --target-env=vulkan1.3
        ${INPUT_GLSL} -o ${OUTPUT_SPIRV}
    DEPENDS ${INPUT_GLSL}
)
```

Use `glslc` (from Vulkan SDK) instead of `glslangValidator` — better diagnostics, native include support.

### Sanity Check After This Phase

Verify `shaders/monolith/raytracer.comp` still compiles and produces the same image as `shaders/legacy/raytracer.comp` after switching to `#include` of the new common headers. Toggle between `useLegacyRenderer = true` and `false` — the pixel output should be byte-identical.

Commit as **"Extract shader functions into common GLSL headers"**.

## 7. Phase 2: Introduce the G-Buffer

The G-buffer is the biggest architectural change. After this, the renderer outputs the same final image, but **internally** it's now: primary visibility pass → final composite pass. This is the precondition for adding cascades.

### Define the G-Buffer Images

In a new class `passes/GBuffer.{hpp,cpp}`:

```cpp
// passes/GBuffer.hpp
class GBuffer {
public:
    // Position (xyz) + linear depth (w)
    Image position;    // VK_FORMAT_R32G32B32A32_SFLOAT
    
    // Normal (xyz) + roughness (w)
    Image normal;      // VK_FORMAT_R16G16B16A16_SFLOAT
    
    // Albedo (rgb) + metallic (a)
    Image albedo;      // VK_FORMAT_R8G8B8A8_UNORM
    
    // Emissive (rgb) + materialIndex (a)
    Image emissive;    // VK_FORMAT_R16G16B16A16_SFLOAT
    
    // Linear Z, separate for the cascade allocation pass
    Image linearDepth; // VK_FORMAT_R32_SFLOAT
    
    void create(VulkanContext& ctx, VkExtent2D size);
    void destroy(VulkanContext& ctx);
    void transitionForWrite(VkCommandBuffer cmd);
    void transitionForRead(VkCommandBuffer cmd);
};
```

### The Primary Visibility Shader — MANDATORY: Hybrid Analytical + BVH Intersection

> **CRITICAL — Do not skip this section.** The guide originally showed a BVH-only `primary.comp`. That version was implemented and measured: **it runs at 2 FPS (700ms per frame)** with any scene that has room walls, floor, or ceiling as planes or quads.
>
> **Root cause:** Planes and quads (room walls, floor, ceiling) are analytical primitives — they are NOT in the BVH. With BVH-only intersection, every pixel pointing at a wall executes a full BVH miss traversal across all 83K triangles before returning false. In a camera-inside-a-room setup, the vast majority of pixels are wall pixels. The CPU cannot even close the window normally because the GPU is TDRing.
>
> **Fix — measured at 60 FPS:** Test planes and quads analytically first. Store the nearest hit as `analyticalT`. Set `ray.tMax = analyticalT` before calling `traverseBVH`. The BVH now terminates early on every wall pixel (any node beyond the wall distance is immediately rejected). This is a permanent architectural constraint — it must survive every future refactor of `primary.comp`.

Create `shaders/visibility/primary.comp` with the **hybrid analytical + BVH approach**. The G-buffer images live at `set = 1` (not `set = 0`) to avoid colliding with the scene SSBO bindings in `set = 0`.

Bindings 5 and 6 in `set = 0` are declared for planes and quads (matching `sceneDescSetLayout`). The push constants provide `cam.planeCount` and `cam.quadCount` to drive the loops.

```glsl
#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable
layout(local_size_x = 16, local_size_y = 16) in;

#include "../common/ray.glsl"
#include "../common/bvh.glsl"
#include "../common/material.glsl"
#include "../common/push_constants.glsl"

// G-buffer images at set=1 (set=0 is reserved for scene SSBOs)
layout(set = 1, binding = 0, rgba32f) uniform writeonly image2D gPosition;
layout(set = 1, binding = 1, rgba16f) uniform writeonly image2D gNormal;
layout(set = 1, binding = 2, rgba8)   uniform writeonly image2D gAlbedo;
layout(set = 1, binding = 3, rgba16f) uniform writeonly image2D gEmissive;
layout(set = 1, binding = 4, r32f)    uniform writeonly image2D gLinearDepth;

struct GPUPlane {
    vec3 center; float p1;
    vec3 normal; int  materialIndex;
    float p2; float p3; float p4; float p5;
};

struct GPUQuad {
    vec3 corner;       float p1;
    vec3 edge1;        float p2;
    vec3 edge2;        float p3;
    vec3 normalVector; int   materialIndex;
    float p4; float p5; float p6; float p7;
};

layout(std430, set = 0, binding = 5) readonly buffer PlaneBuffer { GPUPlane planes[]; };
layout(std430, set = 0, binding = 6) readonly buffer QuadBuffer  { GPUQuad  quads[];  };

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size  = imageSize(gPosition);
    if (pixel.x >= size.x || pixel.y >= size.y) return;

    vec2 uv = (vec2(pixel) + 0.5) / vec2(size) * 2.0 - 1.0;
    uv.y = -uv.y;
    uv.x *= float(size.x) / float(size.y);
    uv *= tan(radians(60.0 * 0.5));

    vec3 rayDir = normalize(cam.camRight.xyz * uv.x
                          + cam.camUp.xyz    * uv.y
                          + cam.camForward.xyz);
    Ray ray;
    ray.origin    = cam.camPos.xyz;
    ray.direction = rayDir;
    ray.tMin      = 0.001;
    ray.tMax      = 1e6;

    // --- Step 1: Test planes and quads analytically.
    // This is mandatory for bounded-room scenes. Without it, every wall pixel
    // exhausts the full BVH (O(N) miss traversal) → 700ms/frame at 83K triangles.
    float analyticalT      = ray.tMax;
    int   analyticalMat    = -1;
    vec3  analyticalNormal = vec3(0.0);
    vec2  analyticalUV     = vec2(0.0);

    for (int i = 0; i < cam.planeCount; i++) {
        float denom = dot(planes[i].normal, rayDir);
        if (abs(denom) < 1e-6) continue;
        float t = dot(planes[i].center - ray.origin, planes[i].normal) / denom;
        if (t > ray.tMin && t < analyticalT) {
            analyticalT      = t;
            analyticalMat    = planes[i].materialIndex;
            analyticalNormal = (denom < 0.0) ? planes[i].normal : -planes[i].normal;
            analyticalUV     = vec2(0.0);
        }
    }

    for (int i = 0; i < cam.quadCount; i++) {
        float denom = dot(quads[i].normalVector, rayDir);
        if (abs(denom) < 1e-6) continue;
        float t = dot(quads[i].corner - ray.origin, quads[i].normalVector) / denom;
        if (t <= ray.tMin || t >= analyticalT) continue;

        vec3  p    = ray.origin + rayDir * t - quads[i].corner;
        float e1l2 = dot(quads[i].edge1, quads[i].edge1);
        float e2l2 = dot(quads[i].edge2, quads[i].edge2);
        float qu   = dot(p, quads[i].edge1) / e1l2;
        float qv   = dot(p, quads[i].edge2) / e2l2;
        if (qu < 0.0 || qu > 1.0 || qv < 0.0 || qv > 1.0) continue;

        analyticalT      = t;
        analyticalMat    = quads[i].materialIndex;
        analyticalNormal = (denom < 0.0) ? quads[i].normalVector : -quads[i].normalVector;
        analyticalUV     = vec2(qu, qv);
    }

    // --- Step 2: BVH traversal capped to the nearest analytical hit.
    // Wall pixels now exit BVH early; only BVH geometry closer than the wall is tested.
    ray.tMax = analyticalT;
    HitInfo hit;
    bool hasBVH = traverseBVH(ray, cam.bvhCount, hit);

    if (hasBVH) {
        GPUMaterial mat   = materials[hit.materialIndex];
        vec3        color = sampleAlbedo(mat, hit.uv);
        imageStore(gPosition,    pixel, vec4(hit.position,     hit.t));
        imageStore(gNormal,      pixel, vec4(hit.normal,       mat.roughness));
        imageStore(gAlbedo,      pixel, vec4(color,            mat.metallic));
        imageStore(gEmissive,    pixel, vec4(mat.emission,     float(hit.materialIndex)));
        imageStore(gLinearDepth, pixel, vec4(hit.t, 0.0, 0.0, 0.0));
    } else if (analyticalMat >= 0) {
        GPUMaterial mat   = materials[analyticalMat];
        vec3        color = sampleAlbedo(mat, analyticalUV);
        vec3        pos   = ray.origin + rayDir * analyticalT;
        imageStore(gPosition,    pixel, vec4(pos,              analyticalT));
        imageStore(gNormal,      pixel, vec4(analyticalNormal, mat.roughness));
        imageStore(gAlbedo,      pixel, vec4(color,            mat.metallic));
        imageStore(gEmissive,    pixel, vec4(mat.emission,     float(analyticalMat)));
        imageStore(gLinearDepth, pixel, vec4(analyticalT, 0.0, 0.0, 0.0));
    } else {
        vec3 skyColor = mix(cam.skyBottomColor, cam.skyTopColor, 0.5 * (rayDir.y + 1.0));
        imageStore(gPosition,    pixel, vec4(0.0, 0.0, 0.0,  1e6));
        imageStore(gNormal,      pixel, vec4(0.0, 1.0, 0.0,  0.0));
        imageStore(gAlbedo,      pixel, vec4(0.0, 0.0, 0.0,  0.0));
        imageStore(gEmissive,    pixel, vec4(skyColor,       -1.0)); // negative = sky sentinel
        imageStore(gLinearDepth, pixel, vec4(1e6,  0.0, 0.0, 0.0));
    }
}
```

**Preserving this pattern:** Any future refactor of `primary.comp` that removes the analytical pre-test will immediately regress to 2 FPS on any room-style scene. The rule is: planes and quads must always be tested analytically first, cap `ray.tMax`, then call `traverseBVH`.

### The Composite Shader (Temporary)

While cascades aren't implemented yet, you need *something* that reads the G-buffer and produces a final image. Create `shaders/shading/composite_temp.comp`.

**G-buffer bindings use `set = 1`** (matching the `gbufDescSetLayout` created in Phase 2b). Note that `outHDR` is at `set = 1, binding = 5` — the same set as the G-buffer images.

> **MANDATORY: Tonemapping in composite_temp.** The composite shader writes to `outHDR` (R16G16B16A16_SFLOAT). This image is then blitted to the swapchain (R8G8B8A8_UNORM). Without tonemapping, any HDR value above 1.0 (e.g. direct light hitting a bright albedo) clamps to white — the entire scene looks blown out/burned.
>
> Add Reinhard tonemapping + gamma correction at the end of `main()`, before `imageStore`. This is a known-temporary measure; it will be replaced by a proper `TonemapPass` in Part C.

```glsl
#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable
layout(local_size_x = 16, local_size_y = 16) in;

#include "../common/ray.glsl"
#include "../common/bvh.glsl"
#include "../common/material.glsl"
#include "../common/lighting.glsl"
#include "../common/push_constants.glsl"

layout(set = 1, binding = 0, rgba32f) uniform readonly  image2D gPosition;
layout(set = 1, binding = 1, rgba16f) uniform readonly  image2D gNormal;
layout(set = 1, binding = 2, rgba8)   uniform readonly  image2D gAlbedo;
layout(set = 1, binding = 3, rgba16f) uniform readonly  image2D gEmissive;
layout(set = 1, binding = 5, rgba16f) uniform writeonly image2D outHDR;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outHDR);
    if (pixel.x >= size.x || pixel.y >= size.y) return;

    vec4 emissiveData = imageLoad(gEmissive, pixel);
    if (emissiveData.a < 0.0) {
        // Sky pixel — tonemap before storing
        vec3 sky = emissiveData.rgb / (emissiveData.rgb + vec3(1.0));
        sky = pow(max(sky, vec3(0.0)), vec3(1.0 / 2.2));
        imageStore(outHDR, pixel, vec4(sky, 1.0));
        return;
    }

    vec4 posData  = imageLoad(gPosition, pixel);
    vec3 worldPos = posData.xyz;
    vec3 normal   = normalize(imageLoad(gNormal, pixel).xyz);
    vec3 albedo   = imageLoad(gAlbedo, pixel).rgb;

    vec3 color = emissiveData.rgb + albedo * 0.1; // ambient term

    for (int l = 0; l < cam.lightCount; l++) {
        vec3  toLight   = lights[l].position - worldPos;
        vec3  lightDir  = normalize(toLight);
        float lightDist = length(toLight);

        Ray shadowRay;
        shadowRay.origin    = worldPos + normal * 0.001;
        shadowRay.direction = lightDir;
        shadowRay.tMin      = 0.001;
        shadowRay.tMax      = lightDist - 0.01;

        HitInfo shadowHit;
        if (!traverseBVH(shadowRay, cam.bvhCount, shadowHit)) {
            float NdotL = max(0.0, dot(normal, lightDir));
            color += albedo * lights[l].color * NdotL;
        }
    }

    // Reinhard tonemapping + gamma (temporary until TonemapPass is added in Part C)
    color = color / (color + vec3(1.0));
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    imageStore(outHDR, pixel, vec4(color, 1.0));
}
```

After this phase, the renderer is **structurally** identical to before but **architecturally** ready for cascades. The G-buffer is in place; subsequent passes will read it.

Commit as **"Split render into primary visibility + composite passes via G-buffer"**.

---

## 7b. Phase 2b: Wire G-Buffer into VulkanCore (Proof of Concept)

> **Goal**: Make the two new shaders (`primary.comp` and `composite_temp.comp`) actually run inside VulkanCore using raw Vulkan handles — no helper classes, no VMA, no render graph. This is the simplest possible integration that lets you confirm the G-buffer architecture works end-to-end before the bigger Phase 3 cleanup.
>
> **Note (Implementation):** In practice, Phase 2b was done directly against the new `Renderer` class (Phase 4) rather than the legacy VulkanCore, since VulkanCore was already being retired. The concepts and bindings below are correct regardless of which class you wire them into.

### The Binding Conflict Problem

The common headers (`bvh.glsl`, `material.glsl`, `lighting.glsl`) already claim **set=0** bindings (1, 3, 4, 8, 9). If `primary.comp` and `composite_temp.comp` use **set=0** for their G-buffer images (bindings 0–4), they collide — `gNormal` at set=0 binding=1 would alias `materialBuffer`, and `outHDR` at set=0 binding=5 would alias `planeBuffer`.

The fix is **two descriptor sets**:

| Set | Contents | Used by |
|-----|----------|---------|
| `set = 0` (`sceneDescSet`) | Placeholder image (b0), scene SSBOs (b1–b8), texture array (b9) | Both passes |
| `set = 1` (`gbufDescSet`) | G-buffer images b0–b4, hdrImage at b5 | Pass-specific |

The `sceneDescSet` (set=0) is reused as-is for both passes. Extra bindings in the layout that a shader ignores are fine in Vulkan.

Always add `#extension GL_EXT_nonuniform_qualifier : enable` to both pass shaders — `material.glsl` declares a runtime texture array that requires it:

```glsl
// primary.comp — see full listing in Phase 7 above
layout(set = 1, binding = 0, rgba32f) uniform writeonly image2D gPosition;
layout(set = 1, binding = 1, rgba16f) uniform writeonly image2D gNormal;
layout(set = 1, binding = 2, rgba8)   uniform writeonly image2D gAlbedo;
layout(set = 1, binding = 3, rgba16f) uniform writeonly image2D gEmissive;
layout(set = 1, binding = 4, r32f)    uniform writeonly image2D gLinearDepth;

// composite_temp.comp — see full listing in Phase 7 above
layout(set = 1, binding = 0, rgba32f) uniform readonly  image2D gPosition;
layout(set = 1, binding = 1, rgba16f) uniform readonly  image2D gNormal;
layout(set = 1, binding = 2, rgba8)   uniform readonly  image2D gAlbedo;
layout(set = 1, binding = 3, rgba16f) uniform readonly  image2D gEmissive;
layout(set = 1, binding = 5, rgba16f) uniform writeonly image2D outHDR;
```

Note that `linearDepth` (b4) is written by primary but **not** read by composite_temp — it exists for future passes (cascade allocation).

### HDR Image → Swapchain: Use Blit, Not Copy

`outHDR` is `VK_FORMAT_R16G16B16A16_SFLOAT`. The swapchain is `VK_FORMAT_R8G8B8A8_UNORM`. **`vkCmdCopyImage` requires identical formats** — it will produce a Vulkan validation error if the formats differ. Use `vkCmdBlitImage` instead, which handles format conversion automatically.

```cpp
// WRONG — formats are incompatible, validation error fires
vkCmdCopyImage(cmd, hdrImage, ..., swapchainImage, ...);

// CORRECT — blit handles R16G16B16A16_SFLOAT → R8G8B8A8_UNORM
vkCmdBlitImage(cmd,
    hdrImage,     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &blitRegion, VK_FILTER_NEAREST);
```

The blit clamps values to [0, 1] — this is why tonemapping in `composite_temp` is mandatory. Without it, HDR values above 1.0 clip to white after the blit.

`hdrImage` must be created with `VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT`. The swapchain already has `VK_IMAGE_USAGE_TRANSFER_DST_BIT`.

### New Members in `VulkanCore.hpp`

```cpp
// Two-pass G-buffer pipelines
VkPipeline primaryPassPipeline;
VkPipeline compositePassPipeline;
VkPipelineLayout twoPassPipelineLayout;

bool useTwoPassRenderer = false;

// G-buffer descriptor set (set = 1 for pass images)
VkDescriptorSetLayout gbufDescSetLayout;
VkDescriptorSet gbufDescSet;

// G-buffer images (written by primary pass, read by composite pass)
VkImage gbufPosition,  gbufNormal,  gbufAlbedo,  gbufEmissive,  gbufLinearDepth;
VkImageView gbufPositionV, gbufNormalV, gbufAlbedoV, gbufEmissiveV, gbufLinearDepthV;
VkDeviceMemory gbufPositionM, gbufNormalM, gbufAlbedoM, gbufEmissiveM, gbufLinearDepthM;

// HDR output image (written by composite pass, blitted to swapchain)
VkImage hdrImage;
VkImageView hdrImageView;
VkDeviceMemory hdrMemory;
```

Also declare three new private methods and two helpers:

```cpp
void createGBufferImages();
void createTwoPassPipelines();
void createGBufferDescriptorSet();

void createStorageImage(VkFormat format, VkImage& image, VkImageView& view, VkDeviceMemory& memory);
VkPipeline createComputePipelineFromSpv(const std::string& path, VkPipelineLayout layout);
```

### Expand the Descriptor Pool

The pool needs to supply the second descriptor set (`gbufDescSet`) and the additional storage image descriptors:

```cpp
void VulkanCore::createDescriptorPool() {
    vector<VkDescriptorPoolSize> poolSizes(3);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 7; // 1 monolith output + 6 G-buffer/HDR images
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 8;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = 100;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 2; // set=0 (scene) + set=1 (G-buffer images)
    // ...
}
```

### `createStorageImage()` Helper

Extracted as a private method so each G-buffer image doesn't repeat the create/allocate/bind/view pattern:

```cpp
void VulkanCore::createStorageImage(VkFormat format, VkImage& image, VkImageView& view, VkDeviceMemory& memory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { swapChainExtent.width, swapChainExtent.height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    vkCreateImage(device, &imageInfo, nullptr, &image);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    vkBindImageMemory(device, image, memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCreateImageView(device, &viewInfo, nullptr, &view);
}
```

### `createGBufferImages()`

Creates all 6 images then transitions them all to `GENERAL` in **one batched command buffer submission**:

```cpp
void VulkanCore::createGBufferImages() {
    createStorageImage(VK_FORMAT_R32G32B32A32_SFLOAT, gbufPosition,    gbufPositionV,    gbufPositionM);
    createStorageImage(VK_FORMAT_R16G16B16A16_SFLOAT, gbufNormal,      gbufNormalV,      gbufNormalM);
    createStorageImage(VK_FORMAT_R8G8B8A8_UNORM,      gbufAlbedo,      gbufAlbedoV,      gbufAlbedoM);
    createStorageImage(VK_FORMAT_R16G16B16A16_SFLOAT, gbufEmissive,    gbufEmissiveV,    gbufEmissiveM);
    createStorageImage(VK_FORMAT_R32_SFLOAT,           gbufLinearDepth, gbufLinearDepthV, gbufLinearDepthM);
    createStorageImage(VK_FORMAT_R16G16B16A16_SFLOAT, hdrImage,        hdrImageView,     hdrMemory);

    VkCommandBuffer cmd; // allocate one-time command buffer...
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImage toInit[] = { gbufPosition, gbufNormal, gbufAlbedo, gbufEmissive, gbufLinearDepth, hdrImage };
    for (VkImage img : toInit)
        transitionImageLayout(cmd, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // submit, wait idle, free cmd
}
```

### `createComputePipelineFromSpv()` Helper

Extracted to avoid repeating the load/module/stage/create/destroy pattern for each of the two pass pipelines:

```cpp
VkPipeline VulkanCore::createComputePipelineFromSpv(const std::string& path, VkPipelineLayout layout) {
    auto code = readFile(path);
    VkShaderModule mod = createShaderModule(code);

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = mod;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = layout;
    pipelineInfo.stage = stageInfo;

    VkPipeline pipeline;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    vkDestroyShaderModule(device, mod, nullptr);
    return pipeline;
}
```

### `createTwoPassPipelines()`

```cpp
void VulkanCore::createTwoPassPipelines() {
    // set=1 layout: 6 storage images (bindings 0-4 = G-buffer, 5 = HDR output)
    vector<VkDescriptorSetLayoutBinding> imgBindings(6);
    for (uint32_t i = 0; i < 6; i++) {
        imgBindings[i].binding = i;
        imgBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imgBindings[i].descriptorCount = 1;
        imgBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dlci{};
    dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlci.bindingCount = 6;
    dlci.pBindings = imgBindings.data();
    vkCreateDescriptorSetLayout(device, &dlci, nullptr, &gbufDescSetLayout);

    // Pipeline layout: set=0 (existing scene layout) + set=1 (G-buffer images)
    VkDescriptorSetLayout layouts[] = { descriptorSetLayout, gbufDescSetLayout };

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(CameraPushConstants);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 2;
    plci.pSetLayouts = layouts;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pushRange;
    vkCreatePipelineLayout(device, &plci, nullptr, &twoPassPipelineLayout);

    primaryPassPipeline   = createComputePipelineFromSpv("shaders/visibility/primary.comp.spv",      twoPassPipelineLayout);
    compositePassPipeline = createComputePipelineFromSpv("shaders/shading/composite_temp.comp.spv", twoPassPipelineLayout);
}
```

### `createGBufferDescriptorSet()`

```cpp
void VulkanCore::createGBufferDescriptorSet() {
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &gbufDescSetLayout;
    vkAllocateDescriptorSets(device, &ai, &gbufDescSet);

    VkImageView views[6] = { gbufPositionV, gbufNormalV, gbufAlbedoV, gbufEmissiveV, gbufLinearDepthV, hdrImageView };
    vector<VkWriteDescriptorSet> writes(6);
    vector<VkDescriptorImageInfo> imgInfos(6);

    for (uint32_t i = 0; i < 6; i++) {
        imgInfos[i].imageView = views[i];
        imgInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imgInfos[i].sampler = VK_NULL_HANDLE;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = gbufDescSet;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[i].pImageInfo = &imgInfos[i];
    }
    vkUpdateDescriptorSets(device, 6, writes.data(), 0, nullptr);
}
```

### `initVulkan()` Call Order

```cpp
void VulkanCore::initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createSwapchain();
    createSwapchainImageViews();
    createComputeImage();
    createGBufferImages();          // NEW — before texture/scene setup

    createTextureSampler();
    createTextureResources();

    createSceneBuffers();
    createDescriptorSetLayout();
    createComputePipeline();
    createTwoPassPipelines();       // NEW — needs descriptorSetLayout
    createDescriptorPool();         // expanded: maxSets=2, STORAGE_IMAGE=7
    createDescriptorSets();
    createGBufferDescriptorSet();   // NEW — needs pool + gbufDescSetLayout
    createCommandBuffers();
    createSyncObjects();
}
```

### `recordCommandBuffer()` — Two-Pass Branch

The engine code uses the **classic** Vulkan sync API (`vkCmdPipelineBarrier` + `VkMemoryBarrier`), not synchronization2. The inter-pass barrier uses a `VkMemoryBarrier` covering all G-buffer images in one call — no layout transition needed since both passes keep images in `GENERAL`.

```cpp
void VulkanCore::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    // ... build CameraPushConstants pc{} ...

    uint32_t dispatchX = swapChainExtent.width  / 16;
    uint32_t dispatchY = swapChainExtent.height / 16;

    if (useTwoPassRenderer) {
        VkDescriptorSet sets[] = { descriptorSet, gbufDescSet };

        // --- Pass 1: Primary Visibility ---
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, primaryPassPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                twoPassPipelineLayout, 0, 2, sets, 0, nullptr);
        vkCmdPushConstants(cmd, twoPassPipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CameraPushConstants), &pc);
        vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

        // --- Barrier: all G-buffer writes visible to composite reads ---
        VkMemoryBarrier memBarrier{};
        memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &memBarrier, 0, nullptr, 0, nullptr);

        // --- Pass 2: Composite Shading ---
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compositePassPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                twoPassPipelineLayout, 0, 2, sets, 0, nullptr);
        vkCmdPushConstants(cmd, twoPassPipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CameraPushConstants), &pc);
        vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

        // --- Blit hdrImage (R16G16B16A16_SFLOAT) → swapchain (R8G8B8A8_UNORM) ---
        // vkCmdCopyImage cannot be used here — the formats are incompatible.
        // vkCmdBlitImage handles the format conversion and clamps values to [0,1].
        transitionImageLayout(cmd, hdrImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transitionImageLayout(cmd, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageBlit blitRegion{};
        blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blitRegion.srcOffsets[0] = { 0, 0, 0 };
        blitRegion.srcOffsets[1] = { (int32_t)swapChainExtent.width, (int32_t)swapChainExtent.height, 1 };
        blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blitRegion.dstOffsets[0] = { 0, 0, 0 };
        blitRegion.dstOffsets[1] = { (int32_t)swapChainExtent.width, (int32_t)swapChainExtent.height, 1 };

        vkCmdBlitImage(cmd,
            hdrImage,                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blitRegion, VK_FILTER_NEAREST);

        transitionImageLayout(cmd, hdrImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(cmd, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    } else {
        // --- Single-pipeline path (monolith or legacy) — unchanged ---
        VkPipeline activePipeline = useLegacyRenderer ? legacyComputePipeline : computePipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, activePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CameraPushConstants), &pc);
        vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

        transitionImageLayout(cmd, computeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transitionImageLayout(cmd, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.extent = { swapChainExtent.width, swapChainExtent.height, 1 };
        vkCmdCopyImage(cmd, computeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        transitionImageLayout(cmd, computeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(cmd, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
}
```

### Cleanup

Destroyed after the descriptor pool (which implicitly frees `gbufDescSet`), before `vkDestroyDevice`:

```cpp
vkDestroyPipeline(device, primaryPassPipeline, nullptr);
vkDestroyPipeline(device, compositePassPipeline, nullptr);
vkDestroyPipelineLayout(device, twoPassPipelineLayout, nullptr);
vkDestroyDescriptorSetLayout(device, gbufDescSetLayout, nullptr);

vkDestroyImageView(device, gbufPositionV,    nullptr); vkDestroyImage(device, gbufPosition,    nullptr); vkFreeMemory(device, gbufPositionM,    nullptr);
vkDestroyImageView(device, gbufNormalV,      nullptr); vkDestroyImage(device, gbufNormal,      nullptr); vkFreeMemory(device, gbufNormalM,      nullptr);
vkDestroyImageView(device, gbufAlbedoV,      nullptr); vkDestroyImage(device, gbufAlbedo,      nullptr); vkFreeMemory(device, gbufAlbedoM,      nullptr);
vkDestroyImageView(device, gbufEmissiveV,    nullptr); vkDestroyImage(device, gbufEmissive,    nullptr); vkFreeMemory(device, gbufEmissiveM,    nullptr);
vkDestroyImageView(device, gbufLinearDepthV, nullptr); vkDestroyImage(device, gbufLinearDepth, nullptr); vkFreeMemory(device, gbufLinearDepthM, nullptr);
vkDestroyImageView(device, hdrImageView,     nullptr); vkDestroyImage(device, hdrImage,        nullptr); vkFreeMemory(device, hdrMemory,        nullptr);
```

### Sanity Check

Compile shaders (`compile_shaders.bat`), set `useTwoPassRenderer = true`, run. The rendered output should be **visually identical** to the monolith (same Lambertian direct lighting, same shadows, same sky). Materials that use textures will show the raw `mat.color` instead — texture sampling via `sampleAlbedo()` is deferred to Phase 3. Flip back to `false` to confirm the monolith path still works unchanged.

Commit as **"Wire two-pass G-buffer render path into VulkanCore (raw handles)"**.

---

## 8. Phase 3: Decouple Vulkan Initialization

`VulkanCore.cpp` is 1190 lines. Most of it is boilerplate that should never need to change again. Extract into focused classes.

> **Three bugs discovered during Phase 3 that are not obvious from the guide.**  They are documented here permanently because they will recur if you ever rebuild these classes.

### Bug A — Single `renderFinishedSemaphore` (VUID-vkQueueSubmit-pSignalSemaphores-00067)

The original plan described one `renderFinishedSemaphore`. **This triggers a Vulkan validation error** the moment the swapchain returns image index 0 on frame N+1 while the presentation engine is still holding the semaphore from frame N.

`CommandManager` must hold **one semaphore per swapchain image**, not one total:

```cpp
class CommandManager {
public:
    VkCommandPool   pool   = VK_NULL_HANDLE;
    VkCommandBuffer buffer = VK_NULL_HANDLE;
    VkSemaphore     imageAvailableSemaphore = VK_NULL_HANDLE;
    std::vector<VkSemaphore> renderFinishedSemaphores; // one per swapchain image
    VkFence inFlightFence = VK_NULL_HANDLE;

    void create(VulkanContext& ctx, uint32_t swapchainImageCount);
    void destroy(VulkanContext& ctx);
    VkCommandBuffer beginOneTime(VulkanContext& ctx);
    void submitOneTime(VulkanContext& ctx, VkCommandBuffer cmd);
};
```

`create()` takes `swapchainImageCount` and creates that many semaphores:

```cpp
void CommandManager::create(VulkanContext& ctx, uint32_t swapchainImageCount) {
    // ... pool, buffer, fence, imageAvailableSemaphore creation ...
    renderFinishedSemaphores.resize(swapchainImageCount);
    VkSemaphoreCreateInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        vkCreateSemaphore(ctx.device, &si, nullptr, &renderFinishedSemaphores[i]);
    }
}
```

In `drawFrame()`, index by the acquired image:

```cpp
VkSemaphore signalSems[] = { cmdManager.renderFinishedSemaphores[imageIndex] };
```

**Do not add `vkQueueWaitIdle` as a workaround.** It stalls the CPU every frame until the GPU and presentation engine both finish — even when the GPU is fast, this adds ~700ms of apparent camera sluggishness and destroys interactive performance. Fix the semaphores properly.

### Bug B — `lastFrame` Initialized to 0.0f (First-Frame Camera Teleport)

`lastFrame` is a class member defaulting to `0.0f`. The first frame's `deltaTime = glfwGetTime() - 0.0f` equals the entire application startup time (1–2+ seconds). Every key pressed during startup feeds this giant deltaTime into camera movement — the camera teleports across the scene on the first frame.

**Fix: seed `lastFrame` from `glfwGetTime()` immediately before the `while` loop**, not in the class initializer or constructor:

```cpp
void Renderer::mainLoop() {
    lastFrame = (float)glfwGetTime(); // MUST be here, not in class declaration
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime  = currentFrame - lastFrame;
        lastFrame  = currentFrame;
        // ...
    }
}
```

### Bug C — VMA Buffer Cleanup Order (Use-After-Free)

VMA buffers (`Buffer` objects holding a `VmaAllocation`) must be destroyed **before** `vmaDestroyAllocator` is called inside `ctx.shutdown()`. If you let `Buffer` destructors run after the allocator goes away, VMA writes to freed memory.

In `Renderer::cleanup()`, explicitly null all `Buffer` members before calling `ctx.shutdown()`:

```cpp
void Renderer::cleanup() {
    // Destroy VMA-backed buffers before the allocator shuts down
    materialBuffer = Buffer();
    sphereBuffer   = Buffer();
    triangleBuffer = Buffer();
    lightBuffer    = Buffer();
    planeBuffer    = Buffer();
    quadBuffer     = Buffer();
    cubeBuffer     = Buffer();
    bvhBuffer      = Buffer();

    // ... destroy images, pipelines, descriptor sets ...

    cmdManager.destroy(ctx);
    swapchain.destroy(ctx);
    ctx.shutdown();  // vmaDestroyAllocator happens here — all buffers already gone
}
```

This requires `Buffer`'s move assignment operator to null `handle` and `allocation` so the moved-from destructor is a no-op.

### `core/VulkanContext.{hpp,cpp}`

Owns: instance, debug messenger, physical device, logical device, queues.

```cpp
class VulkanContext {
public:
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue presentQueue;
    uint32_t graphicsQueueFamily;
    
    VmaAllocator allocator;
    
    void initialize(GLFWwindow* window);
    void shutdown();
    
private:
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createVMA();
};
```

Use vk-bootstrap to compress this to ~50 lines:

```cpp
void VulkanContext::initialize(GLFWwindow* window) {
    vkb::InstanceBuilder builder;
    auto inst = builder.set_app_name("RC Engine")
                       .request_validation_layers(true)
                       .require_api_version(1, 3, 0)
                       .use_default_debug_messenger()
                       .build();
    instance = inst.value().instance;
    debugMessenger = inst.value().debug_messenger;
    
    glfwCreateWindowSurface(instance, window, nullptr, &surface);
    
    vkb::PhysicalDeviceSelector selector{inst.value()};
    VkPhysicalDeviceVulkan13Features v13{};
    v13.synchronization2 = VK_TRUE;
    v13.dynamicRendering = VK_TRUE;
    VkPhysicalDeviceVulkan12Features v12{};
    v12.bufferDeviceAddress = VK_TRUE;
    v12.descriptorIndexing = VK_TRUE;
    
    auto phys = selector.set_surface(surface)
                        .set_minimum_version(1, 3)
                        .set_required_features_13(v13)
                        .set_required_features_12(v12)
                        .select();
    physicalDevice = phys.value().physical_device;
    
    vkb::DeviceBuilder devBuilder{phys.value()};
    auto dev = devBuilder.build();
    device = dev.value().device;
    
    graphicsQueue = dev.value().get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = dev.value().get_queue_index(vkb::QueueType::graphics).value();
    
    // VMA
    VmaAllocatorCreateInfo aci{};
    aci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    aci.physicalDevice = physicalDevice;
    aci.device = device;
    aci.instance = instance;
    aci.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateAllocator(&aci, &allocator);
}
```

### `core/Swapchain.{hpp,cpp}`

Owns: swapchain, swapchain images, image views.

### `core/CommandManager.{hpp,cpp}`

Owns: command pool, per-frame command buffers, sync objects.

### `resources/Buffer.{hpp,cpp}` and `resources/Image.{hpp,cpp}`

VMA-backed wrappers. Each is RAII (destructor calls vmaDestroyBuffer/Image).

```cpp
class Buffer {
public:
    VkBuffer handle;
    VmaAllocation allocation;
    void* mapped = nullptr;
    size_t size = 0;
    
    Buffer(VmaAllocator alloc, size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage);
    ~Buffer();
    
    void uploadStaged(VkCommandBuffer cmd, const void* data, size_t size, Buffer& staging);
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;
    Buffer(const Buffer&) = delete;
};
```

This removes ~300 lines of manual `vkCreateBuffer + vkAllocateMemory + vkBindBufferMemory` from your codebase.

### `passes/GBuffer.{hpp,cpp}`

With `Image` and `VulkanContext` now defined, implement `GBuffer.cpp` properly. The `.hpp` was written as a stub in Phase 2; now fill in the body:

```cpp
void GBuffer::create(VulkanContext& ctx, VkExtent2D size) {
    auto make = [&](VkFormat fmt) -> Image {
        return Image(ctx.allocator, size.width, size.height, fmt,
                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);
    };
    position    = make(VK_FORMAT_R32G32B32A32_SFLOAT);
    normal      = make(VK_FORMAT_R16G16B16A16_SFLOAT);
    albedo      = make(VK_FORMAT_R8G8B8A8_UNORM);
    emissive    = make(VK_FORMAT_R16G16B16A16_SFLOAT);
    linearDepth = make(VK_FORMAT_R32_SFLOAT);
}

void GBuffer::destroy(VulkanContext& ctx) {
    position.destroy(ctx.allocator);
    normal.destroy(ctx.allocator);
    albedo.destroy(ctx.allocator);
    emissive.destroy(ctx.allocator);
    linearDepth.destroy(ctx.allocator);
}
```

`transitionForWrite()` / `transitionForRead()` emit `VkImageMemoryBarrier2` records via `vkCmdPipelineBarrier2` (synchronization2), changing layout between `UNDEFINED → GENERAL` (write) and `GENERAL → GENERAL` (read after write, access mask only).

### Upgrade `shaders/common/material.glsl`: add `sampleAlbedo()`

Now that the full G-buffer pipeline is wired and the texture path is tested, replace the `mat.color` placeholder in `primary.comp` with a real texture-aware function. Add to the **bottom** of `material.glsl` (after the GPUMaterial struct and existing helpers):

```glsl
// Returns the base color for a surface hit, sampling the albedo texture if present.
// Converts from sRGB to linear by undoing gamma (pow 2.2 approximation).
vec3 sampleAlbedo(GPUMaterial mat, vec2 uv) {
    if (mat.useTexture == 1 && mat.albedoIndex >= 0) {
        vec3 texCol = textureLod(textures[nonuniformEXT(mat.albedoIndex)], uv, 0.0).rgb;
        return pow(max(texCol, vec3(0.0)), vec3(2.2));
    }
    return mat.color;
}
```

Then update `primary.comp` to use it:

```glsl
// Replace:
vec3 baseColor = mat.color; // Phase 2: use raw color; sampleAlbedo() added in Phase 3
// With:
vec3 baseColor = sampleAlbedo(mat, hit.uv);
```

`material.glsl` already has the `#extension GL_EXT_nonuniform_qualifier` via the textures[] binding — add it at the top of the file if it isn't there yet, since `nonuniformEXT()` requires it:

```glsl
#extension GL_EXT_nonuniform_qualifier : enable
```

Commit each class extraction as a separate PR.

## 9. Phase 4: Add a Render Graph (Renderer Class)

Don't overengineer this. A "render graph" for the showcase is a `Renderer` class that owns all Vulkan state and dispatches passes in explicit order. No separate `Pass` classes are needed yet — integrate them into `Renderer` directly.

### Actual Descriptor Set Layout (What Was Implemented)

```
set = 0  (sceneDescSet, sceneDescSetLayout)
    binding 0  — VK_DESCRIPTOR_TYPE_STORAGE_IMAGE   (placeholder; hdrImage used as dummy)
    binding 1  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (materialBuffer)
    binding 2  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (sphereBuffer)
    binding 3  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (triangleBuffer)
    binding 4  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (lightBuffer)
    binding 5  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (planeBuffer)
    binding 6  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (quadBuffer)
    binding 7  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (cubeBuffer)
    binding 8  — VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  (bvhBuffer)
    binding 9  — VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER [100] (textures)

set = 1  (gbufDescSet, gbufDescSetLayout)
    binding 0  — VK_DESCRIPTOR_TYPE_STORAGE_IMAGE   (gPosition  rgba32f)
    binding 1  — VK_DESCRIPTOR_TYPE_STORAGE_IMAGE   (gNormal    rgba16f)
    binding 2  — VK_DESCRIPTOR_TYPE_STORAGE_IMAGE   (gAlbedo    rgba8)
    binding 3  — VK_DESCRIPTOR_TYPE_STORAGE_IMAGE   (gEmissive  rgba16f)
    binding 4  — VK_DESCRIPTOR_TYPE_STORAGE_IMAGE   (gLinearDepth r32f)
    binding 5  — VK_DESCRIPTOR_TYPE_STORAGE_IMAGE   (hdrImage   rgba16f — write target)
```

**Descriptor pool:** `maxSets = 2`, `STORAGE_IMAGE = 7` (1 placeholder + 6 gbuf/hdr), `STORAGE_BUFFER = 8`, `COMBINED_IMAGE_SAMPLER = 100`.

Binding 9 (texture array) needs all 100 slots filled even if fewer textures are loaded — fill unused slots with `hdrImage.view` at `VK_IMAGE_LAYOUT_GENERAL` as a harmless dummy.

### Pipeline Layout

A single `twoPassPipelineLayout` covers both passes:

```cpp
VkDescriptorSetLayout setLayouts[] = { sceneDescSetLayout, gbufDescSetLayout };

VkPushConstantRange pushRange{};
pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
pushRange.offset     = 0;
pushRange.size       = sizeof(CameraPushConstants);

VkPipelineLayoutCreateInfo plci{};
plci.setLayoutCount      = 2;
plci.pSetLayouts         = setLayouts;
plci.pushConstantRangeCount = 1;
plci.pPushConstantRanges = &pushRange;
```

`CameraPushConstants` is ~160 bytes. Most modern GPUs support 256 bytes; the Vulkan spec only guarantees 128 bytes. If targeting older hardware, reduce the struct (e.g. move fog/sky to a UBO).

### `initVulkan()` Call Order

```cpp
void Renderer::initVulkan() {
    ctx.initialize(window);
    swapchain.create(ctx, ctx.surface, { 1280, 720 });
    gbuffer.create(ctx, swapchain.extent);
    cmdManager.create(ctx, (uint32_t)swapchain.images.size()); // pass image count!

    hdrImage = Image(ctx.allocator, width, height,
                     VK_FORMAT_R16G16B16A16_SFLOAT,
                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);

    // Transition all storage images to VK_IMAGE_LAYOUT_GENERAL in one submission
    VkCommandBuffer cmd = cmdManager.beginOneTime(ctx);
    gbuffer.transitionForWrite(cmd);
    transitionImageLayout(cmd, hdrImage.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    cmdManager.submitOneTime(ctx, cmd);

    createTextureSampler();
    createTextureResources();
    createSceneBuffers();
    createSceneDescriptorSetLayout();
    createGBufferDescriptorSetLayout();
    createPipelines();
    createDescriptorPool();
    createSceneDescriptorSet();
    createGBufferDescriptorSet();
}
```

### `recordCommandBuffer()` — Two-Pass Frame

```cpp
void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &bi);

    CameraPushConstants pc = buildPushConstants();
    uint32_t dispatchX = swapchain.extent.width  / 16;
    uint32_t dispatchY = swapchain.extent.height / 16;
    VkDescriptorSet sets[] = { sceneDescSet, gbufDescSet };

    // Pass 1: Primary Visibility (fills G-buffer)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, primaryPassPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, twoPassPipelineLayout, 0, 2, sets, 0, nullptr);
    vkCmdPushConstants(cmd, twoPassPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

    // Barrier: G-buffer writes visible to composite reads
    VkMemoryBarrier memBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &memBarrier, 0, nullptr, 0, nullptr);

    // Pass 2: Composite Shading (reads G-buffer, writes hdrImage)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compositePassPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, twoPassPipelineLayout, 0, 2, sets, 0, nullptr);
    vkCmdPushConstants(cmd, twoPassPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

    // Blit hdrImage (R16G16B16A16_SFLOAT) → swapchain (R8G8B8A8_UNORM)
    // Must use BlitImage, not CopyImage — formats are incompatible for Copy
    transitionImageLayout(cmd, hdrImage.handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transitionImageLayout(cmd, swapchain.images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit blitRegion{};
    blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.srcOffsets[1]  = { (int32_t)swapchain.extent.width, (int32_t)swapchain.extent.height, 1 };
    blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.dstOffsets[1]  = { (int32_t)swapchain.extent.width, (int32_t)swapchain.extent.height, 1 };

    vkCmdBlitImage(cmd,
        hdrImage.handle,              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blitRegion, VK_FILTER_NEAREST);

    transitionImageLayout(cmd, hdrImage.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    transitionImageLayout(cmd, swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(cmd);
}
```

### FPS Measurement Loop

Add this to `mainLoop()` to track performance during development:

```cpp
void Renderer::mainLoop() {
    lastFrame = (float)glfwGetTime(); // seed here — see Bug B in Phase 3
    float fpsTimer  = 0.0f;
    int   frameCount = 0;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime  = currentFrame - lastFrame;
        lastFrame  = currentFrame;

        fpsTimer += deltaTime;
        frameCount++;
        if (fpsTimer >= 1.0f) {
            std::cout << "FPS: " << frameCount
                      << " | Frame: " << (fpsTimer / frameCount * 1000.0f) << " ms\n";
            fpsTimer  = 0.0f;
            frameCount = 0;
        }

        glfwPollEvents();
        processInput();
        updateDynamicData();
        drawFrame();
    }
    vkDeviceWaitIdle(ctx.device);
}
```

This was critical for diagnosing the 700ms/frame issue. Without it, "feels slow" is the only signal. With it, "2 FPS | 700ms" immediately points at a GPU bottleneck, not a camera bug.

### Part B Completion Checklist

Before moving to Part C, verify all of the following:

- [ ] `compile_shaders.bat` compiles `shaders/visibility/primary.comp` and `shaders/shading/composite_temp.comp` without errors
- [ ] No Vulkan validation errors in the debug output (especially `VUID-vkQueueSubmit-pSignalSemaphores-00067`)
- [ ] FPS is 60 (vsync-limited) or >100 uncapped — NOT 2-15 FPS
- [ ] Camera moves smoothly from the first frame (no first-frame snap or teleport)
- [ ] The rendered image is correctly lit — not blown out/all white (tonemapping working)
- [ ] The rendered image is not flat/dark — Lambertian direct lighting visible on all surfaces
- [ ] VMA buffer destructor order warning: no crash or validation error on close
- [ ] `VulkanCore.cpp` is excluded from compilation (not retired from disk, but excluded)

### What composite_temp Deliberately Does NOT Do

`composite_temp.comp` is a known-minimal placeholder. The following are intentional regressions compared to the old monolith, to be restored by Part C passes or separately:

- No specular highlights (needs direct specular term in FinalGatherPass)
- No mirror reflections (needs reflection ray pass or SSR)
- No glass / refraction (needs transmission ray pass)
- No soft shadows (BVH shadow only; hard shadows only)
- No procedural textures or normal maps (code exists in material.glsl; not called)
- No fog (post-process after FinalGather)
- No depth-of-field

The render looks "flat" compared to the monolith because all of the above are missing. **RC specifically replaces the `albedo × 0.1` ambient hack with real indirect radiance from cascade probes.** Everything else on the list is separate work, independent of RC.

Commit as **"Part B complete: Renderer replaces VulkanCore, all Vulkan classes extracted, 60 FPS restored"**.

---

# Part C — Radiance Cascades Implementation (Showcase Scope)

## 10. Showcase-Scope Decision: Dense Grid First

The full Sparse RC (with GPU hash map, eviction, per-pixel level selection) is too complex to deliver for the showcase deadline. **For the showcase, simplify to a dense small cascade grid; defer sparse hash storage to the dissertation build.**

### Showcase RC Scope

- ✅ Hierarchical cascades (4–6 levels)
- ✅ Octahedral direction encoding
- ✅ Top-down cascade merging
- ✅ Indirect diffuse lighting visible in scenes
- ✅ ImGui debug interface
- ⚠️ Dense 3D probe grid (fixed-extent box around camera, e.g. 64×32×64 cells)
- ⚠️ No per-pixel cascade level selection
- ⚠️ No hash map (later)
- ❌ Specular GI
- ❌ Min-max probes
- ❌ Bilinear-3D upscaler (defer to dissertation)

### Why This Works for the Showcase

Your demo scenes are bounded (one or two rooms with a few objects). A 64×32×64 cascade-0 grid at 0.25m spacing covers a 16×8×16m volume — easily containing your Sponza-interior or Cornell-style demo scene. Memory: 64×32×64 × 16 octahedral × 8 bytes = 16 MB for cascade 0. Manageable.

The visual quality difference between dense-bounded RC and full sparse RC is small in bounded scenes — sparsity matters for *open worlds*, not for showcase rooms.

### Document the Simplification

In your dissertation, this becomes "Showcase build: dense bounded volume; Dissertation build: GPU hash map with screen-bound sparsity." Two builds, two papers' worth of contributions.

## 11. Phase 5: Cascade Data Structures

### CPU-Side Configuration

```cpp
// gi/SparseRC/CascadeConfig.hpp
struct CascadeConfig {
    int numCascades = 5;
    int branchingFactor = 2; // α
    
    // Cascade 0 parameters
    glm::vec3 worldOrigin = glm::vec3(-16, -4, -16);  // bottom corner of volume
    glm::ivec3 gridSize0 = glm::ivec3(64, 32, 64);    // probes in cascade 0
    float spacing0 = 0.5f;                            // world space probe spacing
    int octRes0 = 4;                                  // 4×4 octahedral map = 16 directions
    
    // Derived for each cascade level
    glm::ivec3 gridSize(int level) const {
        // halve each axis per level (in 3D, 1/8 probes per level)
        return glm::ivec3(
            std::max(1, gridSize0.x >> level),
            std::max(1, gridSize0.y >> level),
            std::max(1, gridSize0.z >> level)
        );
    }
    
    float spacing(int level) const {
        return spacing0 * float(1 << level);
    }
    
    int octRes(int level) const {
        // double octahedral resolution per level (4× directions)
        return octRes0 << level;
    }
    
    float intervalStart(int level) const {
        return (level == 0) ? 0.0f : intervalEnd(level - 1);
    }
    
    float intervalEnd(int level) const {
        return spacing0 * float(1 << (branchingFactor * level));
    }
};
```

### GPU-Side Storage

For each cascade level, one `Buffer` holding the octahedral radiance + transmittance:

```cpp
// gi/SparseRC/CascadeStorage.hpp
class CascadeStorage {
public:
    std::vector<Buffer> cascadeBuffers;  // one per cascade level
    CascadeConfig config;
    
    void initialize(VulkanContext& ctx, const CascadeConfig& cfg);
    
    size_t getBufferSize(int level) const {
        glm::ivec3 g = config.gridSize(level);
        int probeCount = g.x * g.y * g.z;
        int dirCount = config.octRes(level) * config.octRes(level);
        return size_t(probeCount) * dirCount * sizeof(uint64_t); // FP16 RGBA = 8 bytes
    }
};

void CascadeStorage::initialize(VulkanContext& ctx, const CascadeConfig& cfg) {
    config = cfg;
    cascadeBuffers.clear();
    cascadeBuffers.reserve(cfg.numCascades);
    
    for (int i = 0; i < cfg.numCascades; i++) {
        size_t size = getBufferSize(i);
        cascadeBuffers.emplace_back(
            ctx.allocator, size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
    }
}
```

For the showcase config (64×32×64 base, α=2, 5 levels):

| Level | Grid Size | Probes | OctRes | Dirs/Probe | Bytes/Probe | Total Size |
|---|---|---|---|---|---|---|
| 0 | 64×32×64 | 131072 | 4 | 16 | 128 | 16 MB |
| 1 | 32×16×32 | 16384 | 8 | 64 | 512 | 8 MB |
| 2 | 16×8×16 | 2048 | 16 | 256 | 2048 | 4 MB |
| 3 | 8×4×8 | 256 | 32 | 1024 | 8192 | 2 MB |
| 4 | 4×2×4 | 32 | 64 | 4096 | 32768 | 1 MB |
| **Total** | | | | | | **~31 MB** |

Memory budget is healthy.

### Indexing Math in GLSL

```glsl
// shaders/common/cascade_layout.glsl
#ifndef CASCADE_LAYOUT_GLSL
#define CASCADE_LAYOUT_GLSL

// Per-cascade push constants:
// pc.cascadeLevel
// pc.gridSize (ivec3)
// pc.worldOrigin (vec3)
// pc.spacing (float)
// pc.octRes (int)

ivec3 probeFromIndex(int probeIndex, ivec3 gridSize) {
    return ivec3(
        probeIndex % gridSize.x,
        (probeIndex / gridSize.x) % gridSize.y,
        probeIndex / (gridSize.x * gridSize.y)
    );
}

int probeToIndex(ivec3 probe, ivec3 gridSize) {
    return probe.x + probe.y * gridSize.x + probe.z * gridSize.x * gridSize.y;
}

vec3 probeWorldPos(ivec3 probe, vec3 worldOrigin, float spacing) {
    return worldOrigin + (vec3(probe) + 0.5) * spacing;
}

int probeStorageOffset(ivec3 probe, ivec3 gridSize, int octRes) {
    int probeIdx = probeToIndex(probe, gridSize);
    return probeIdx * octRes * octRes;
}

#endif
```

## 12. Phase 6: Cascade 0 Ray Tracing

Start by tracing only cascade 0 (with a single hardcoded level). Get this working visually before adding higher cascades.

### `shaders/rc/probe_trace.comp`

```glsl
#version 460
#extension GL_GOOGLE_include_directive : require
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#include "../common/ray.glsl"
#include "../common/bvh.glsl"
#include "../common/material.glsl"
#include "../common/octahedral.glsl"
#include "../common/cascade_layout.glsl"
#include "../common/push_constants.glsl"

layout(push_constant) uniform CascadePC {
    layout(offset = 256) // after the camera push constants
    int cascadeLevel;
    int octRes;
    ivec3 gridSize;
    vec3 worldOrigin;
    float spacing;
    float intervalStart;
    float intervalEnd;
} pc;

layout(set = 0, binding = 10, std430) buffer CascadeBuffer {
    uint64_t cascadeData[];  // packed FP16 RGBA per direction per probe
};

uint64_t packRadianceTransmittance(vec3 L, float T) {
    // Pack as 4xFP16
    uvec2 packed = uvec2(
        packHalf2x16(L.xy),
        packHalf2x16(vec2(L.z, T))
    );
    return uint64_t(packed.y) << 32 | uint64_t(packed.x);
}

void main() {
    ivec3 probe = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(probe, pc.gridSize))) return;
    
    vec3 worldPos = probeWorldPos(probe, pc.worldOrigin, pc.spacing);
    int storageBase = probeStorageOffset(probe, pc.gridSize, pc.octRes);
    
    // Loop over all directions for this probe
    for (int dy = 0; dy < pc.octRes; dy++) {
        for (int dx = 0; dx < pc.octRes; dx++) {
            vec2 uv = octIndexToUV(ivec2(dx, dy), pc.octRes);
            vec3 dir = octDecode(uv);
            
            Ray ray;
            ray.origin = worldPos;
            ray.direction = dir;
            ray.tMin = pc.intervalStart;
            ray.tMax = pc.intervalEnd;
            
            vec3 radiance = vec3(0.0);
            float transmittance = 1.0;
            
            HitInfo hit;
            if (traverseBVH(ray, cam.bvhCount, hit)) {
                GPUMaterial mat = materials[hit.materialIndex];
                vec3 emissive = mat.emission;
                vec3 albedo = sampleAlbedo(mat, hit.uv);
                
                radiance = emissive;
                transmittance = 0.0; // surface is opaque
            }
            
            int storageIdx = storageBase + dy * pc.octRes + dx;
            cascadeData[storageIdx] = packRadianceTransmittance(radiance, transmittance);
        }
    }
}
```

Dispatch:

```cpp
// passes/ProbeTracePass.cpp
void ProbeTracePass::execute(VkCommandBuffer cmd, const Scene& scene, 
                              CascadeStorage& cascades, int level) 
{
    auto& cfg = cascades.config;
    glm::ivec3 grid = cfg.gridSize(level);
    
    CascadePC pc{};
    pc.cascadeLevel = level;
    pc.octRes = cfg.octRes(level);
    pc.gridSize = grid;
    pc.worldOrigin = cfg.worldOrigin;
    pc.spacing = cfg.spacing(level);
    pc.intervalStart = cfg.intervalStart(level);
    pc.intervalEnd = cfg.intervalEnd(level);
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 256, sizeof(pc), &pc);
    
    uint32_t groupX = (grid.x + 7) / 8;
    uint32_t groupY = (grid.y + 7) / 8;
    uint32_t groupZ = grid.z; // one Z slice at a time, since z is part of probe index
    
    vkCmdDispatch(cmd, groupX, groupY, groupZ);
}
```

### Visual Sanity Check

After implementing this, write a quick **debug shader** that visualizes cascade 0:

```glsl
// shaders/debug/visualize_cascade.comp
// Read cascade 0 at the camera's position, render the octahedral map as a 2D image
```

You should see octahedral maps with emissive surfaces showing as bright spots in the directions they exist. This proves the trace pass is working before you add merging.

## 13. Phase 7: Multi-Cascade Hierarchy

Once cascade 0 works, extend to all cascade levels. The dispatch loop is straightforward — call `ProbeTracePass::execute()` once per level. Each level has different intervals and grid sizes, but the shader is identical.

Verify each cascade visually:
- Cascade 0: short rays, near-field
- Cascade 4: long rays, far-field

In your debug shader, allow toggling which cascade level to visualize.

## 14. Phase 8: Cascade Merging

### `shaders/rc/cascade_merge.comp`

```glsl
#version 460
#extension GL_GOOGLE_include_directive : require
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#include "../common/octahedral.glsl"
#include "../common/cascade_layout.glsl"

layout(push_constant) uniform MergePC {
    int currentLevel;
    int parentLevel;
    int currentOctRes;
    int parentOctRes;
    ivec3 currentGridSize;
    ivec3 parentGridSize;
    vec3 worldOrigin;
    float currentSpacing;
    float parentSpacing;
} pc;

layout(set = 0, binding = 10, std430) buffer CurrentCascade {
    uint64_t currentData[];
};
layout(set = 0, binding = 11, std430) readonly buffer ParentCascade {
    uint64_t parentData[];
};

vec4 unpackRadianceTransmittance(uint64_t packed) {
    uint hi = uint(packed >> 32);
    uint lo = uint(packed & 0xFFFFFFFFul);
    vec2 xy = unpackHalf2x16(lo);
    vec2 zw = unpackHalf2x16(hi);
    return vec4(xy.x, xy.y, zw.x, zw.y);
}

uint64_t packRadianceTransmittance(vec3 L, float T) {
    // ... same as in probe_trace
}

vec4 mergeIntervals(vec4 near, vec4 far) {
    // near = (Lr, Lg, Lb, transmittance)
    // far  = (Lr, Lg, Lb, transmittance)
    return vec4(
        near.rgb + near.a * far.rgb,
        near.a * far.a
    );
}

vec4 sampleParentOctahedral(int parentProbeIdx, vec3 direction) {
    vec2 uv = octEncode(direction);
    vec2 idx = uv * float(pc.parentOctRes) - 0.5;
    ivec2 i0 = ivec2(floor(idx));
    vec2 f = idx - vec2(i0);
    
    // Bilinear in direction space
    vec4 acc = vec4(0);
    for (int dy = 0; dy <= 1; dy++) {
        for (int dx = 0; dx <= 1; dx++) {
            ivec2 sampleIdx = clamp(i0 + ivec2(dx, dy), ivec2(0), ivec2(pc.parentOctRes - 1));
            float wx = dx == 0 ? (1.0 - f.x) : f.x;
            float wy = dy == 0 ? (1.0 - f.y) : f.y;
            int storageIdx = parentProbeIdx * pc.parentOctRes * pc.parentOctRes 
                           + sampleIdx.y * pc.parentOctRes + sampleIdx.x;
            acc += unpackRadianceTransmittance(parentData[storageIdx]) * (wx * wy);
        }
    }
    return acc;
}

void main() {
    ivec3 probe = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(probe, pc.currentGridSize))) return;
    
    vec3 worldPos = probeWorldPos(probe, pc.worldOrigin, pc.currentSpacing);
    int currentStorageBase = probeStorageOffset(probe, pc.currentGridSize, pc.currentOctRes);
    
    // Find the 8 parent probes (trilinear in space)
    vec3 parentSpace = (worldPos - pc.worldOrigin) / pc.parentSpacing - 0.5;
    ivec3 parentBase = ivec3(floor(parentSpace));
    vec3 fParent = parentSpace - vec3(parentBase);
    
    // For each of my directions, merge with the appropriate parent samples
    for (int dy = 0; dy < pc.currentOctRes; dy++) {
        for (int dx = 0; dx < pc.currentOctRes; dx++) {
            vec2 uv = octIndexToUV(ivec2(dx, dy), pc.currentOctRes);
            vec3 dir = octDecode(uv);
            
            // Trilinear sample of parent in space
            vec4 parentMerged = vec4(0);
            float weightSum = 0;
            for (int pz = 0; pz <= 1; pz++) {
                for (int py = 0; py <= 1; py++) {
                    for (int px = 0; px <= 1; px++) {
                        ivec3 pProbe = parentBase + ivec3(px, py, pz);
                        if (any(lessThan(pProbe, ivec3(0))) || 
                            any(greaterThanEqual(pProbe, pc.parentGridSize))) continue;
                        
                        float wx = px == 0 ? (1.0 - fParent.x) : fParent.x;
                        float wy = py == 0 ? (1.0 - fParent.y) : fParent.y;
                        float wz = pz == 0 ? (1.0 - fParent.z) : fParent.z;
                        float w = wx * wy * wz;
                        
                        int parentProbeIdx = probeToIndex(pProbe, pc.parentGridSize);
                        vec4 sampled = sampleParentOctahedral(parentProbeIdx, dir);
                        parentMerged += sampled * w;
                        weightSum += w;
                    }
                }
            }
            if (weightSum > 0) parentMerged /= weightSum;
            
            // Load my current interval
            int storageIdx = currentStorageBase + dy * pc.currentOctRes + dx;
            vec4 myInterval = unpackRadianceTransmittance(currentData[storageIdx]);
            
            // Merge: my near interval + parent's far interval, attenuated by my transmittance
            vec4 merged = mergeIntervals(myInterval, parentMerged);
            
            currentData[storageIdx] = packRadianceTransmittance(merged.rgb, merged.a);
        }
    }
}
```

Dispatch in top-down order: cascade N-1 has no parent, then merge N-2 with N-1, then N-3 with N-2, etc.

## 15. Phase 9: Final Gather

### `shaders/shading/final_gather.comp`

```glsl
#version 460
#extension GL_GOOGLE_include_directive : require
layout(local_size_x = 16, local_size_y = 16) in;

#include "../common/octahedral.glsl"
#include "../common/cascade_layout.glsl"

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D gPosition;
layout(set = 0, binding = 1, rgba16f) uniform readonly image2D gNormal;
layout(set = 0, binding = 2, rgba8)   uniform readonly image2D gAlbedo;
layout(set = 0, binding = 3, rgba16f) uniform readonly image2D gEmissive;
layout(set = 0, binding = 10, std430) readonly buffer Cascade0 { uint64_t cascade0Data[]; };
layout(set = 0, binding = 5, rgba16f) uniform writeonly image2D outHDR;

layout(push_constant) uniform PC {
    vec3 cascadeWorldOrigin;
    float cascade0Spacing;
    ivec3 cascade0GridSize;
    int cascade0OctRes;
} pc;

vec3 sampleProbeIrradiance(ivec3 probe, vec3 normal) {
    int storageBase = probeStorageOffset(probe, pc.cascade0GridSize, pc.cascade0OctRes);
    int dirCount = pc.cascade0OctRes * pc.cascade0OctRes;
    
    vec3 irradiance = vec3(0);
    for (int d = 0; d < dirCount; d++) {
        int dx = d % pc.cascade0OctRes;
        int dy = d / pc.cascade0OctRes;
        vec2 uv = octIndexToUV(ivec2(dx, dy), pc.cascade0OctRes);
        vec3 dir = octDecode(uv);
        float NdotL = max(0.0, dot(dir, normal));
        if (NdotL == 0) continue;
        
        vec4 interval = unpackRadianceTransmittance(cascade0Data[storageBase + d]);
        irradiance += interval.rgb * NdotL;
    }
    return irradiance * (2.0 * 3.14159265 / float(dirCount));
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outHDR);
    if (pixel.x >= size.x || pixel.y >= size.y) return;
    
    vec4 emissive = imageLoad(gEmissive, pixel);
    if (emissive.a < 0.0) {
        // Sky
        imageStore(outHDR, pixel, vec4(emissive.rgb, 1.0));
        return;
    }
    
    vec4 posData = imageLoad(gPosition, pixel);
    vec3 worldPos = posData.xyz;
    vec3 normal = imageLoad(gNormal, pixel).xyz;
    vec3 albedo = imageLoad(gAlbedo, pixel).rgb;
    
    // Trilinear sample of cascade 0
    vec3 cascadeSpace = (worldPos - pc.cascadeWorldOrigin) / pc.cascade0Spacing - 0.5;
    ivec3 base = ivec3(floor(cascadeSpace));
    vec3 f = cascadeSpace - vec3(base);
    
    vec3 indirect = vec3(0);
    float weightSum = 0;
    for (int pz = 0; pz <= 1; pz++) {
        for (int py = 0; py <= 1; py++) {
            for (int px = 0; px <= 1; px++) {
                ivec3 probe = base + ivec3(px, py, pz);
                if (any(lessThan(probe, ivec3(0))) || any(greaterThanEqual(probe, pc.cascade0GridSize))) continue;
                
                float wx = px == 0 ? (1.0 - f.x) : f.x;
                float wy = py == 0 ? (1.0 - f.y) : f.y;
                float wz = pz == 0 ? (1.0 - f.z) : f.z;
                float w = wx * wy * wz;
                
                indirect += sampleProbeIrradiance(probe, normal) * w;
                weightSum += w;
            }
        }
    }
    if (weightSum > 0) indirect /= weightSum;
    
    vec3 color = emissive.rgb + albedo * indirect;
    
    // TODO: add direct light loop (from composite_temp.comp)
    
    imageStore(outHDR, pixel, vec4(color, 1.0));
}
```

After this, you should see **noiseless indirect light** in your scenes. Move emissive objects around — color bleeding should follow instantly with no temporal lag.

## 16. Phase 10: Tonemap & Composite

### `shaders/tonemap/tonemap.comp`

```glsl
#version 460
layout(local_size_x = 16, local_size_y = 16) in;

layout(set = 0, binding = 0, rgba16f) uniform readonly  image2D inHDR;
layout(set = 0, binding = 1, rgba8)   uniform writeonly image2D outLDR;

// ACES tonemap (Narkowicz approximation)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outLDR);
    if (pixel.x >= size.x || pixel.y >= size.y) return;
    
    vec3 hdr = imageLoad(inHDR, pixel).rgb;
    
    // Exposure (push constant if you want it controllable)
    hdr *= 1.0;
    
    // Tonemap
    vec3 ldr = ACESFilm(hdr);
    
    // Gamma correction (linear → sRGB)
    ldr = pow(ldr, vec3(1.0 / 2.2));
    
    imageStore(outLDR, pixel, vec4(ldr, 1.0));
}
```

Dispatch into the swapchain image. Done.

---

# Part D — Polish for Showcase

## 17. ImGui Debug Interface (Showcase Edition)

Add the debug panels that will impress at the showcase. Visitors will *see* the system working.

```cpp
void DebugUI::draw(Renderer& renderer) {
    ImGui::Begin("Radiance Cascades Debug");
    
    // Performance
    auto stats = renderer.lastStats();
    ImGui::Text("Frame: %.2f ms (%.1f FPS)", stats.frameTime, 1000.0f / stats.frameTime);
    if (ImGui::CollapsingHeader("Per-Pass Timings")) {
        ImGui::Text("  Primary visibility: %.2f ms", stats.primaryMs);
        ImGui::Text("  Cascade trace:      %.2f ms (sum)", stats.traceMs);
        ImGui::Text("  Cascade merge:      %.2f ms (sum)", stats.mergeMs);
        ImGui::Text("  Final gather:       %.2f ms", stats.gatherMs);
        ImGui::Text("  Tonemap:            %.2f ms", stats.tonemapMs);
    }
    
    // Cascade configuration (live tunable!)
    if (ImGui::CollapsingHeader("Cascade Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        changed |= ImGui::SliderInt("Cascades", &renderer.cfg.numCascades, 1, 6);
        changed |= ImGui::SliderInt("Branching α", &renderer.cfg.branchingFactor, 1, 3);
        changed |= ImGui::SliderFloat("Spacing", &renderer.cfg.spacing0, 0.1f, 2.0f);
        changed |= ImGui::SliderInt("Oct Res 0", &renderer.cfg.octRes0, 2, 16);
        if (changed) renderer.recreateCascades();
    }
    
    // Debug visualization
    if (ImGui::CollapsingHeader("Visualization")) {
        ImGui::Combo("Show", &renderer.debugMode, 
            "Final\0Albedo\0Normal\0Depth\0Emissive\0Cascade 0\0Cascade 1\0Cascade 2\0");
        ImGui::Checkbox("Disable direct light", &renderer.disableDirect);
        ImGui::Checkbox("Disable indirect light", &renderer.disableIndirect);
    }
    
    // Scene controls
    if (ImGui::CollapsingHeader("Scene")) {
        ImGui::ColorEdit3("Sky Top", &renderer.skyTop.x);
        ImGui::ColorEdit3("Sky Bottom", &renderer.skyBottom.x);
        // ... emissive intensities, light positions
    }
    
    ImGui::End();
}
```

### Visualization Modes That Demo Well

- **Final** — what the user normally sees
- **Cascade N** — visualize the cascade buffer as a 2D atlas of octahedral maps. Striking visual; shows the algorithm internals
- **Indirect only** — show *only* the indirect light contribution. Makes color bleeding super obvious
- **Direct only** — for comparison; shows what the scene looks like without GI

At the showcase, switching between "Direct only" and "Final" with one toggle is the most impactful demo. Visitors immediately see what RC adds.

## 18. Demo Scene Authoring

Build 2–3 strong demo scenes:

### Scene 1: "Color Bleeding Box"
- Cornell-box style: 5-sided white room with one bright red wall and one bright green wall
- A neutral grey teapot or bunny in the middle
- Single warm overhead light
- **What it shows**: color bleeding from walls onto the model. The hallmark RC result.

### Scene 2: "Emissive Objects"
- Small dark interior with no other light source
- 3–4 brightly emissive objects (your existing shuttle model + a doughnut + a cube, each with different colors)
- **What it shows**: indirect light from emissive objects illuminates everything; no light wells. Soft shadows.

### Scene 3: "Outdoor with Sky"
- Sky-only lighting (no point lights)
- Sponza atrium or similar with arches
- **What it shows**: realistic ambient occlusion under the arches; bright openings.

Save these as JSON files that your scene loader can parse:

```json
{
  "name": "color_bleeding_box",
  "camera": { "pos": [0, 2, -5], "lookAt": [0, 1, 0] },
  "ambient_color": [0.1, 0.1, 0.1],
  "materials": [
    { "name": "red_wall", "color": [1, 0.1, 0.1], "emission": [0, 0, 0] },
    { "name": "green_wall", "color": [0.1, 1, 0.1], "emission": [0, 0, 0] },
    { "name": "white_wall", "color": [0.9, 0.9, 0.9] },
    { "name": "neutral_model", "color": [0.7, 0.7, 0.7] }
  ],
  "models": [
    { "path": "assets/models/model_teapot.obj", "material": "neutral_model", "transform": [...] }
  ],
  "lights": [
    { "position": [0, 4, 0], "color": [10, 9, 8], "radius": 0.1 }
  ]
}
```

A JSON-driven scene loader (using nlohmann/json) takes ~150 lines of C++ and pays for itself many times over during demo prep.

## 19. Performance Profiling

For the showcase, you need to *see* the frame time live. Add a frame time graph in ImGui:

```cpp
static std::deque<float> frameTimes;
frameTimes.push_back(stats.frameTime);
if (frameTimes.size() > 240) frameTimes.pop_front();

std::vector<float> frameData(frameTimes.begin(), frameTimes.end());
ImGui::PlotLines("Frame Time (ms)", frameData.data(), frameData.size(), 
                 0, nullptr, 0.0f, 50.0f, ImVec2(0, 80));
```

Goal for showcase: stable 60 FPS (16ms) on a mid-range GPU at 1080p with all demo scenes. If you can hit this, your dissertation introduction can claim "real-time" credibly.

## 20. Visual Sanity Checks

Before showing the project to anyone, run through this checklist:

- [ ] **Direct-only mode** matches your pre-RC build (no regression in the simple case)
- [ ] **Indirect-only mode** shows soft color spread, no obvious banding or seams
- [ ] **Toggle GI** produces a visible, instant change in the image (no lag)
- [ ] **Move an emissive object** — indirect light follows immediately (no temporal artifacts)
- [ ] **Move the camera quickly** — no flickering or "popping" of probes  
- [ ] **Sky-lit scene** has visible occlusion under overhangs
- [ ] **No NaN or black pixels** anywhere (sometimes happens with zero-radius lights, division by zero, etc.)
- [ ] **HDR clipping is acceptable** (very bright emissives shouldn't completely white out the screen)

If any of these fail, debug before the showcase.

---

# Part E — Migration Path to Dissertation Build

After the showcase, fork the project (or use a feature branch) for the dissertation version. The dissertation version is the showcase version *plus* these additions:

## 21. From Dense Grid to Sparse Hash Map

Replace `CascadeStorage` (which uses one Buffer per level with dense indexing) with a `HashMap` class implementing the algorithm in dissertation guide Part V Section 26.

**The shaders barely change** — only the function `probeStorageOffset()` is replaced with `lookupProbe()` returning a slot index.

Add a new allocation pass that runs before the trace passes.

## 22. Adding DDGI and SSGI Comparison Baselines

Implement the `GIStrategy` interface (dissertation guide Section 45). Add `DDGIRenderer` and `SSGIRenderer` as alternative implementations. Add a ComboBox in ImGui to switch between them at runtime.

DDGI implementation: ~2 weeks. The Majercik 2019 paper is your spec.

SSGI implementation: ~3–5 days. Standard screen-space stochastic raymarcher with temporal accumulation.

## 23. Reference Path Tracer for RMSE

Add another `GIStrategy` implementation that runs full Monte Carlo path tracing for N samples per pixel (N = 1000+). Run it once per scene offline, save the HDR output as ground truth. Compute RMSE in your real-time renderer against this stored image.

## 24. Benchmark Infrastructure

Add a "benchmark mode" flag to `main.cpp` that:
1. Disables ImGui rendering
2. Plays a recorded camera path
3. Captures per-pass timings + VRAM + RMSE for each frame
4. Outputs a CSV

Write a Python script to plot results. Your dissertation's Figures 5-12 will come from this.

---

## Closing Notes

This refactor is significant — probably 6–8 weeks of focused work for the showcase scope, another 6–8 weeks for the dissertation extensions. Plan accordingly.

**Two pieces of practical advice**:

1. **Don't try to be a hero on the BVH builder.** Median split is fine for the showcase. SAH is optional. Hardware RT extensions are *definitely* optional. The novelty is in the cascade algorithm, not the ray tracing backend.

2. **The G-buffer split is the most dangerous refactor.** Take your time on it. Verify pixel-identical output before moving on. Every subsequent step depends on that foundation being correct.

When you're stuck, reference the dissertation study guide (companion document) for the theory. When you're stuck on Vulkan specifically, the Vulkan Tutorial site (vulkan-tutorial.com) and the VkGuide site (vkguide.dev) are the two best free resources.

Good luck with the showcase. Send me the demo when it's running.

