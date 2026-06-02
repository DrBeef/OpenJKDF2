# OpenJKDF2 VR - Quest 3 Build

This folder contains the Android Studio project for building OpenJKDF2 VR for Meta Quest 3.

## Prerequisites

1. **Android Studio** (Arctic Fox 2020.3.1 or newer)
2. **Android SDK** (API level 32+)
3. **Android NDK** (version 26.1.10909125 — pinned by `build.gradle`'s `ndkVersion`)
4. **CMake** (3.22.1+, install via Android Studio SDK Manager)

## Setup

1. Open Android Studio
2. Select "Open an Existing Project"
3. Navigate to `OpenJKDF2/packaging/quest-vr` and open it
4. Wait for Gradle sync to complete
5. If prompted, install any missing SDK components

### First-time Setup

1. Copy `local.properties.template` to `local.properties`
2. Edit `local.properties` and set your Android SDK path
3. Android Studio should auto-detect or download the NDK

## Building

1. Select `Build > Make Project` (or press Ctrl+F9)
2. Wait for the native code to compile (this takes a while the first time)
3. Select `Build > Build Bundle(s) / APK(s) > Build APK(s)`

The APK will be generated at:
`build/outputs/apk/debug/OpenJKDF2-VR-debug.apk`

## Installing on Quest 3

### Via ADB
```bash
adb install build/outputs/apk/debug/OpenJKDF2-VR-debug.apk
```

### Via SideQuest
1. Connect your Quest 3 to your PC
2. Open SideQuest
3. Drag and drop the APK onto the SideQuest window

## Game Assets

You need to copy your Jedi Knight game files to the Quest. The app looks for them in:
`/sdcard/Android/data/org.openjkdf2.vr/files/`

Copy these folders from your Jedi Knight installation:
- `Episode/`
- `Resource/`
- `MUSIC/` (optional, for music)

## Troubleshooting

### Build fails with CMake errors
- Make sure CMake 3.22.1 is installed via Android Studio SDK Manager
- Check that NDK 26.1.10909125 is installed

### App crashes on startup
- Check logcat for errors: `adb logcat | grep -i openjkdf2`
- Ensure game assets are copied to the correct location

### VR not working
- Make sure you have an OpenXR runtime installed (Quest has this built-in)
- Check that the app has VR permissions in Quest settings

## Project Structure

```
quest-vr/
├── AndroidManifest.xml    # VR app manifest with Quest metadata
├── build.gradle           # Gradle build config with CMake integration
├── src/main/
│   ├── java/              # Java sources (VRActivity)
│   └── res/               # Android resources
├── assets/                # Game assets (shaders, etc.)
└── gradle/                # Gradle wrapper
```

The native code is built from the root `CMakeLists.txt` with:
- `PLAT_ANDROID_ARM64=TRUE`
- `TARGET_USE_VR=TRUE`
