# OpenJKDF2 VR Build Guide

This guide provides detailed instructions for building OpenJKDF2 with VR (OpenXR) support on Windows using Visual Studio 2022.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Version Requirements](#version-requirements)
- [Step-by-Step Build Instructions](#step-by-step-build-instructions)
- [Build Configuration Options](#build-configuration-options)
- [Troubleshooting](#troubleshooting)
- [Testing Your Build](#testing-your-build)

---

## Prerequisites

### Required Software

| Software | Version | Download Link | Notes |
|----------|---------|---------------|-------|
| Visual Studio 2022 | 17.0+ | [Download](https://visualstudio.microsoft.com/vs/) | Community edition works |
| CMake | 3.20+ | [Download](https://cmake.org/download/) | Or use VS built-in CMake |
| Python | 3.8+ | [Download](https://www.python.org/downloads/) | Must be in PATH |
| Git | 2.30+ | [Download](https://git-scm.com/downloads) | For cloning and submodules |
| OpenAL SDK | 1.1 | [Download](https://www.openal.org/downloads/) | Required for audio |

### Visual Studio Workloads

When installing Visual Studio 2022, ensure these workloads are selected:

1. **Desktop development with C++**
   - MSVC v143 build tools (or later)
   - Windows 10/11 SDK (10.0.19041.0 or later)
   - C++ CMake tools for Windows

2. **Individual Components** (verify these are installed):
   - C++ core features
   - C++ 2022 Redistributable Update
   - Windows Universal CRT SDK

### Python Package

Install the required Python package:

```powershell
pip install cogapp
```

Or if using Python 3:
```powershell
pip3 install cogapp
```

### OpenAL SDK Setup

1. Download and install OpenAL 1.1 SDK from [openal.org](https://www.openal.org/downloads/)
2. Add system environment variable:
   - Variable name: `OPENALDIR`
   - Variable value: `C:\Program Files (x86)\OpenAL 1.1 SDK`

---

## Version Requirements

### Minimum Versions
| Component | Minimum Version | Recommended Version |
|-----------|-----------------|---------------------|
| CMake | 3.20 | 3.25+ |
| Visual Studio | 2022 (17.0) | 2022 (17.8+) |
| Python | 3.8 | 3.10+ |
| Windows SDK | 10.0.19041.0 | 10.0.22621.0 |
| OpenXR loader SDK | 1.1.54 | 1.1.54 (auto-fetched) |
| OpenXR **runtime API** | 1.0 | 1.0 (requested at `xrCreateInstance`) |

> **OpenXR 1.1 vs 1.0 — important:** The *loader source* that gets compiled in is
> OpenXR-SDK `release-1.1.54` (set in `cmake_modules/build_openxr.cmake`), but the
> application always **requests API version 1.0** at runtime
> (`createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0` in
> `src/Platform/VR/stdVR_OpenXR.cpp`). This is deliberate: SteamVR does not yet support
> OpenXR 1.1, so requesting 1.0 keeps PCVR working across SteamVR, Oculus, and other
> runtimes. The 1.1.54 loader is backward-compatible and honors the 1.0 request. Do **not**
> change this to `XR_CURRENT_API_VERSION` (which the 1.1 headers define as 1.1) — that
> reproduces the `LoaderInstance::CreateInstance ... CreateInstance call failed` error on
> SteamVR. On desktop, CMake also prefers a system-installed OpenXR (≥1.0) via
> `find_package` and only fetches 1.1.54 if none is found.

### C++ Standard Requirements
- C11 for C code
- C++17 for C++ code (required for OpenXR)

---

## Step-by-Step Build Instructions

### Step 1: Clone the Repository

```powershell
git clone https://github.com/Team-Beef-Studios/OpenJKDF2.git
cd OpenJKDF2
git checkout standalone-support
```

### Step 2: Initialize Submodules

```powershell
git submodule update --init --recursive
```

This downloads required dependencies:
- SDL2
- SDL_mixer
- OpenAL Soft
- GLEW
- nlohmann/json
- And others

### Step 3: Create Build Directory

```powershell
mkdir build_vr
cd build_vr
```

### Step 4: Configure with CMake

#### Option A: Command Line (Recommended)

```powershell
cmake .. -G "Visual Studio 17 2022" -A x64 -DTARGET_USE_VR=ON
```

#### Option B: Visual Studio GUI

1. Open Visual Studio 2022
2. Select **File → Open → Folder**
3. Navigate to the OpenJKDF2 directory
4. Wait for CMake configuration to complete
5. In the CMake Settings, add:
   - `TARGET_USE_VR`: `ON`
6. Save and let CMake reconfigure

#### Option C: CMake GUI

1. Open CMake GUI
2. Set source directory: `C:\path\to\OpenJKDF2`
3. Set build directory: `C:\path\to\OpenJKDF2\build_vr`
4. Click **Configure**
5. Select "Visual Studio 17 2022" and "x64"
6. After configuration, check `TARGET_USE_VR`
7. Click **Configure** again
8. Click **Generate**
9. Click **Open Project**

### Step 5: Build the Project

#### Command Line Build

```powershell
cmake --build . --config Release --target openjkdf2-64
```

Or for Debug build:
```powershell
cmake --build . --config Debug --target openjkdf2-64
```

#### Visual Studio Build

1. Open `build_vr\OpenJKDF2.sln`
2. Set configuration to **Release** (or Debug)
3. Set platform to **x64**
4. Right-click **openjkdf2-64** in Solution Explorer
5. Select **Build**

### Step 6: Locate Output Files

After successful build, find your executable at:
```
build_vr\Release\jkdf2xr.exe
```

Required DLLs will be in the same directory:
- `OpenAL32.dll`
- `exchndl.dll`, `mgwhelp.dll`, `symsrv.dll` (crash handling)

---

## Build Configuration Options

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `TARGET_USE_VR` | `OFF` | Enable OpenXR VR support |
| `PLAT_MSVC` | Auto-detected | Target Windows with MSVC |
| `CMAKE_BUILD_TYPE` | `Release` | Build type (Release/Debug/RelWithDebInfo) |

### Enabling VR Support

VR support is enabled by setting `TARGET_USE_VR=ON`. This:
1. Adds `PLATFORM_VR` and `TARGET_USE_OPENXR` compile definitions
2. Provides the OpenXR loader: on desktop, uses a system-installed OpenXR (≥1.0) if found, otherwise auto-fetches OpenXR-SDK 1.1.54 via FetchContent. The app requests **API version 1.0** at runtime regardless (see the OpenXR 1.1 vs 1.0 note above).
3. Includes VR source files from `src/Platform/VR/`
4. Links the OpenXR loader library

### Build Types

| Type | Use Case | Optimizations | Debug Info |
|------|----------|---------------|------------|
| `Release` | Distribution | Full | None |
| `Debug` | Development | None | Full |
| `RelWithDebInfo` | Profiling | Full | Full |

---

## Troubleshooting

### Common Issues

#### "cogapp not found"
```
Error: cog command not found
```
**Solution**: Ensure Python Scripts directory is in PATH:
```powershell
# Find Python Scripts path
python -c "import sys; print(sys.prefix + '\\Scripts')"

# Add to PATH (run as Administrator)
setx PATH "%PATH%;C:\Users\YourName\AppData\Local\Programs\Python\Python310\Scripts"
```

#### "OpenAL not found"
```
Error: Could not find OpenAL
```
**Solution**:
1. Verify OpenAL SDK is installed
2. Check `OPENALDIR` environment variable is set correctly
3. Restart Visual Studio/terminal after setting environment variable

#### CMake Configuration Fails Multiple Times
This is normal for first-time configuration. Keep clicking **Configure** until it succeeds. Usually takes 2-3 attempts.

#### "Cannot find Windows SDK"
**Solution**:
1. Open Visual Studio Installer
2. Modify your VS2022 installation
3. Ensure "Windows 10 SDK" or "Windows 11 SDK" is checked
4. Install and restart

#### Submodule Errors
```
fatal: No url found for submodule path 'lib/xxx'
```
**Solution**:
```powershell
git submodule sync
git submodule update --init --recursive --force
```

#### Link Errors with OpenXR
```
LNK2019: unresolved external symbol xrCreateInstance
```
**Solution**: Ensure `TARGET_USE_VR=ON` is set and CMake has been reconfigured.

### Build Warnings

The following warnings are expected and can be ignored:
```
LINK : warning LNK4044: unrecognized option '/static'; ignored
LINK : warning LNK4044: unrecognized option '/static-libgcc'; ignored
LINK : warning LNK4044: unrecognized option '/static-libstdc++'; ignored
```
These are MinGW flags that don't apply to MSVC builds.

---

## Testing Your Build

### Basic Test

1. Copy `jkdf2xr.exe` to your Jedi Knight game directory:
   ```
   C:\GOG Games\Star Wars Jedi Knight - Dark Forces 2\
   ```

2. Copy required DLLs (OpenAL32.dll, etc.) to the same directory

3. Launch the game:
   ```powershell
   cd "C:\GOG Games\Star Wars Jedi Knight - Dark Forces 2"
   .\jkdf2xr.exe
   ```

### VR Test Mode

For automated VR testing without a headset, use the VR test arguments:
```powershell
.\jkdf2xr.exe -vrtest -vrframes 600
```

This runs for 600 frames (~10 seconds at 60fps) with simulated VR input.

### Verify VR is Enabled

When launching with a VR headset connected:
1. The game should detect your VR runtime (SteamVR, Oculus, etc.)
2. Check `vr_debug.log` in the game directory for VR initialization messages
3. The headset should display the game in stereo 3D

### Switching VR Runtimes

To switch between VR runtimes (e.g., Oculus vs SteamVR), modify the Windows Registry:

```powershell
# For Oculus/Meta Quest
reg add "HKLM\SOFTWARE\Khronos\OpenXR\1" /v ActiveRuntime /t REG_SZ /d "C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json" /f

# For SteamVR
reg add "HKLM\SOFTWARE\Khronos\OpenXR\1" /v ActiveRuntime /t REG_SZ /d "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json" /f
```

---

## Directory Structure

After a successful build, your directory should look like:
```
OpenJKDF2/
├── build_vr/
│   ├── Release/
│   │   ├── jkdf2xr.exe      # Main executable
│   │   ├── OpenAL32.dll          # Audio library
│   │   ├── exchndl.dll           # Crash handler
│   │   ├── mgwhelp.dll
│   │   └── symsrv.dll
│   ├── _deps/
│   │   └── openxr_loader-build/  # Auto-fetched OpenXR
│   └── OpenJKDF2.sln             # VS solution file
├── src/
│   └── Platform/
│       └── VR/                   # VR-specific source files
└── cmake_modules/
    ├── plat_feat_vr.cmake        # VR build configuration
    └── build_openxr.cmake        # OpenXR fetch rules
```

---

## Additional Resources

- [OpenJKDF2 Main Repository](https://github.com/shinyquagsire23/OpenJKDF2)
- [OpenXR Specification](https://www.khronos.org/openxr/)
- [SDL2 Documentation](https://wiki.libsdl.org/)
- [Visual Studio CMake Documentation](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio)

---

## Version History

| Date | Changes |
|------|---------|
| 2026-06-02 | Updated OpenXR SDK to 1.1.54; corrected clone URL/branch to Team-Beef-Studios `standalone-support`. Note: 1.1.54 defaults `XR_CURRENT_API_VERSION` to 1.1, but SteamVR only supports OpenXR 1.0 — `xrCreateInstance` requests `XR_API_VERSION_1_0`. |
| 2026-01-19 | Initial VR build guide |
