# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OpenJKDF2 is a function-by-function reimplementation of Jedi Knight: Dark Forces II in C. The codebase is reverse-engineered from the original game, with ~57% completion by code weight (96% excluding rasterizer). Original symbol names and structures are preserved where known.

## Build Commands

| Platform | Command |
|----------|---------|
| Linux 64-bit | `./build_linux64.sh` |
| Windows (MinGW) | `./build_win64.sh` |
| Windows (MSVC) | `mkdir build && cd build && cmake .. -G "Visual Studio 17 2022" -A x64 && cmake --build . --config Release` |
| Windows PCVR | `mkdir build_vr && cd build_vr && cmake .. -G "Visual Studio 17 2022" -A x64 -DTARGET_USE_VR=ON && cmake --build . --config Release` |
| macOS | `./.github/build_macos.sh` |
| Android | `./build_android.sh` |
| Quest VR | `./build_quest_vr.sh` |

To enable tests: `export TARGET_BUILD_TESTS=1` before building.

### Android/Quest VR Build Environment
Android SDK location: `C:\Users\simon\AppData\Local\Android\Sdk`

For Quest VR builds, set these environment variables:
```bash
export ANDROID_HOME="C:/Users/simon/AppData/Local/Android/Sdk"
export ANDROID_NDK_HOME="C:/Users/simon/AppData/Local/Android/Sdk/ndk/26.1.10909125"
./build_quest_vr.sh
```

## Architecture

### Module Organization
All functions and globals use a module prefix matching their file (e.g., `sithThing_` for sithThing.c, `stdMath_` for stdMath.c). This naming is mandatory.

### Key Subsystems

- **src/Main/** - Entry points, game state machine, frame loop
- **src/Engine/** - Rendering (sithRender), physics (sithPhysics), collision (sithCollision), camera
- **src/World/** - Level data, sectors, surfaces, models, materials
- **src/Platform/** - Platform abstraction layer:
  - `GL/` - OpenGL rendering backend (std3D.c)
  - `VR/` - OpenXR VR integration (stdVR.c, stdVR_OpenXR.cpp)
  - `SDL2/` - Cross-platform windowing and input
- **src/Cog/** - COG scripting language (lexer, parser, VM, built-in functions)
- **src/AI/** - NPC AI state machine and pathfinding
- **src/Gameplay/** - Player mechanics, inventory, saber combat
- **src/Devices/** - Input (sithControl), audio (sithSound)
- **src/Gui/** - Menus and HUD rendering

### Entity System
Everything in the game world is a `sithThing` - players, NPCs, items, projectiles, effects. Things are created from templates and belong to sectors for spatial partitioning.

### Rendering Pipeline
The renderer is abstracted through `std3D.h`. The OpenGL implementation lives in `Platform/GL/std3D.c`. VR rendering uses per-eye framebuffers managed by `stdVR_PrepareEyeBuffer()`/`stdVR_FinishEyeBuffer()`.

## Code Style (Hungarian Notation Required)

### Variable Prefixes
- `p` - pointer (`pThing`, `pWorld`)
- `a` - array (`aSectors`, `aThings`)
- `ap` - array of pointers
- `b` - boolean (`bStartup`, `bEnabled`)
- `fn` - function pointer
- `num` or `n` - count

### Type Prefixes
- Structs: `s` prefix (e.g., `sSithCvar`)
- Typedefs: `t` prefix (e.g., `tSithCvar`)
- Exception: `rd*` types (RenderDroid) are not refactored

### Change Annotations
- Bug fixes: `// Added:`, `// Removed:`, `// Altered:`
- New features: wrap in `#ifdef QOL_IMPROVEMENTS`
- Resource limits: define in `engine_config.h`

## Key Files

- `src/types.h` - Core type definitions
- `src/engine_config.h` - Engine limits and configuration
- `src/Main/Main.c` - Application startup
- `src/Main/sithMain.c` - Engine initialization order
- `src/Platform/GL/std3D.c` - OpenGL rendering (171KB)
- `src/Platform/VR/stdVR_OpenXR.cpp` - VR implementation (109KB)
- `src/Engine/sithThing.c` - Entity system (67KB)
- `src/Cog/sithCogExec.c` - Script execution

## VR Build Instructions

### Android VR Build (Meta Quest + Pico 4/Neo3)

The same APK works on both Meta Quest and Pico devices - uses Khronos OpenXR loader.

**Supported devices:**
- Meta Quest 2, Quest Pro, Quest 3
- Pico 4, Pico 4 Ultra, Pico Neo3

**Prerequisites:**
- Android SDK: `C:\Users\simon\AppData\Local\Android\Sdk`
- Android NDK: `C:\Users\simon\AppData\Local\Android\Sdk\ndk\26.1.10909125`

**Step 1: Build native libraries**
```bash
cd /c/DEV/GitHub/Public/OpenJKDF2
export ANDROID_HOME="C:/Users/simon/AppData/Local/Android/Sdk"
export ANDROID_NDK_HOME="C:/Users/simon/AppData/Local/Android/Sdk/ndk/26.1.10909125"
./build_quest_vr.sh
```

**Step 2: Build APK**
```bash
cd packaging/quest-vr
./gradlew.bat assembleDebug
```

**Step 3: Install on device**
```bash
"C:/Users/simon/AppData/Local/Android/Sdk/platform-tools/adb.exe" install -r "C:/DEV/GitHub/Public/OpenJKDF2/packaging/quest-vr/build/outputs/apk/debug/JKDF2-XR-debug.apk"
```

**Troubleshooting:**
- If gradle fails with locked file errors, stop the daemon first: `./gradlew.bat --stop`
- APK output location: `packaging/quest-vr/build/outputs/apk/debug/JKDF2-XR-debug.apk`

### PC VR Build (Windows)

**Step 1: Configure with CMake**
```bash
cd /c/DEV/GitHub/Public/OpenJKDF2
mkdir -p build_pcvr && cd build_pcvr
cmake .. -G "Visual Studio 17 2022" -A x64 -DTARGET_USE_VR=ON
```

**Step 2: Build**
```bash
cmake --build . --config Release
```

**Output:** `C:\DEV\GitHub\Public\OpenJKDF2\build_pcvr\Release\jkdf2xr.exe`
(The CMake *target* is still `openjkdf2-64`; the VR build's output is renamed to `jkdf2xr.exe` via `OUTPUT_NAME` in `cmake_modules/plat_msvc.cmake`, gated on `TARGET_USE_VR`.)

### Packaging a PCVR Release (JKDF2-XR)

`packaging/pcvr/package-pcvr.ps1` bundles the PCVR build into a single distributable
zip that end users extract, drop their game files into, and run. No game assets are
included — users supply their own copy of JKDF2.

**Prerequisite:** build the PCVR target first (see *PC VR Build* above) so
`build_pcvr/Release/jkdf2xr.exe` exists and is current.

**Run it** (from the repo root):
```powershell
powershell -ExecutionPolicy Bypass -File packaging\pcvr\package-pcvr.ps1
```

**Output:** `dist/JKDF2-XR-PCVR-v<version>.zip` (the `dist/` folder is gitignored).
- The version comes from `cmake_modules/version.cmake` → `OPENJKDF2VR_PROJECT_VERSION`
  (so it's in the zip's *filename* and a `JKDF2-XR-v<version>.txt` marker inside).
- The zip contains a single `JKDF2-XR/` folder (no version, for clean overwrites)
  with: the exe, the 4 runtime DLLs (OpenAL32/exchndl/mgwhelp/symsrv, from the
  `build_pcvr` root), the engine `resource/` (shaders/ui/ssl — from the repo, NOT
  the user's game GOBs), `jkdf2xr_vr_weapons.json`, and `HOW-TO-PLAY.txt`.

**Optional params:** `-BuildDir` (default `build_pcvr`), `-OutputDir` (default `dist`),
`-WeaponsJson` (default `packaging/pcvr/files/jkdf2xr_vr_weapons.json`).

**Weapon offsets (golden source):** `packaging/pcvr/files/jkdf2xr_vr_weapons.json` is
the single canonical weapon-alignment file. The PCVR package bundles it directly, and
the Quest build pulls it in via the gradle `copyWeapons` task — so both platforms ship
identical, pre-tuned alignment. To update offsets, edit that one file (or re-copy from a
tuned runtime `C:\DEV\JKDF2-XR\jkdf2xr_vr_weapons.json`); do not hand-edit the
`packaging/quest-vr/assets/` copy (it's overwritten from the golden source at build time).

To cut a versioned release, bump `OPENJKDF2VR_PROJECT_VERSION` in
`cmake_modules/version.cmake`, rebuild PCVR, then re-run the packaging script.

### VR Technical Notes

- **Android VR** (Quest/Pico) uses MultiView (GL_OVR_multiview2) for single-pass stereo rendering
- **PC VR** uses traditional per-eye rendering (two render passes)
- MultiView shader code is wrapped in `#ifdef MULTIVIEW_ENABLED`
- Key shader: `resource/shaders/default_v.glsl`
- VR stereo uses depth-dependent parallax: `pos.x += eyeOffset / (mvpDepth + 0.0001)`
- OpenXR SDK version: 1.1.54 (Khronos)
- Controller profiles: Oculus Touch, Pico 4, Pico Neo3, Valve Index, HTC Vive, KHR Simple

### MultiView Implementation Details (Quest/Pico)

**FBO Architecture (RazeXR-style):**
- Creates one FBO per swapchain image with color and depth permanently attached
- Each FBO has its own depth texture array (not shared)
- Textures are attached at creation time and never detached during rendering
- Just switches which FBO is bound based on acquired swapchain image index

**UBO Buffer Orphaning (Critical for Pico):**
- MultiView eye matrices are stored in UBOs (`std3D_viewMatricesUBO`, `std3D_projMatricesUBO`)
- Must use buffer orphaning (`glBufferData(NULL)` before `glBufferSubData`) when updating UBOs
- Without orphaning, Pico's tiled GPU may read stale data while CPU updates the buffer for the next frame
- This causes a split-screen effect where some tiles show wrong eye perspective
- Quest's Adreno driver handles this synchronization internally, but Pico's does not

**Shader Considerations:**
- Use direct array indexing with `gl_ViewID_OVR`: `u_viewMatrices[gl_ViewID_OVR]`
- Avoid if-else branching on `gl_ViewID_OVR` as it can cause issues on tiled GPUs
- Use `glInvalidateFramebuffer` on depth attachment before swapchain release (tiled GPU optimization)

## Testing

Build with tests enabled, then run the test executable:
```bash
export TARGET_BUILD_TESTS=1
./build_linux64.sh
./build_linux64/rle_test
```

VR can be tested without a headset:
```bash
./jkdf2xr.exe -vrtest -vrframes 600
```
