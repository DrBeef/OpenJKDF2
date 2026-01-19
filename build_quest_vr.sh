#!/bin/sh
# Build script for OpenJKDF2 VR on Meta Quest 3
# Requires: Android NDK, Android SDK, Java 17

set -e

export OPENJKDF2_RELEASE_COMMIT=$(git log -1 --format="%H")
export OPENJKDF2_RELEASE_COMMIT_SHORT=$(git rev-parse --short=8 HEAD)

# Java setup (macOS)
if [ -x /usr/libexec/java_home ]; then
    export JAVA_HOME=$(/usr/libexec/java_home -v 17)
fi

# Find NDK toolchain
if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "Error: ANDROID_NDK_HOME not set"
    exit 1
fi

NDK_TOOLCHAIN_BINS=$(dirname $(find "$ANDROID_NDK_HOME/" -name "aarch64-linux-android31-clang" | head -1))
PATH=$PATH:$NDK_TOOLCHAIN_BINS

BUILD_DIR="build_quest_vr"
mkdir -p $BUILD_DIR && cd $BUILD_DIR
OPENJKDF2_BUILD_DIR=$(pwd)

# Prevent macOS headers from getting linked in
export -n SDKROOT MACOSX_DEPLOYMENT_TARGET CPLUS_INCLUDE_PATH C_INCLUDE_PATH

# Find Ninja explicitly
NINJA_PATH=""
if [ -n "$ANDROID_HOME" ]; then
    NINJA_PATH=$(find "$ANDROID_HOME/cmake" -name "ninja*" -type f 2>/dev/null | head -1)
    if [ -n "$NINJA_PATH" ]; then
        echo "Found Ninja at: $NINJA_PATH"
    fi
fi

echo "=== Configuring CMake for Quest VR ==="
cmake .. \
    -G "Ninja" \
    -DCMAKE_MAKE_PROGRAM="$NINJA_PATH" \
    --toolchain $(pwd)/../cmake_modules/toolchain_android_aarch64.cmake \
    -DTARGET_USE_VR=TRUE \
    -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "CMake configuration failed"
    exit 1
fi

echo "=== Building OpenJKDF2 VR ==="
"$NINJA_PATH" openjkdf2-armv8a

if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

cd ..

echo "=== Packaging Quest VR APK ==="
QUEST_PACKAGE_DIR="packaging/quest-vr"

# Copy native libraries
mkdir -p $QUEST_PACKAGE_DIR/app/src/main/jniLibs/arm64-v8a

cp $OPENJKDF2_BUILD_DIR/libopenjkdf2-armv8a.so $QUEST_PACKAGE_DIR/app/src/main/jniLibs/arm64-v8a/libmain.so
cp $OPENJKDF2_BUILD_DIR/openal/libopenal.so $QUEST_PACKAGE_DIR/app/src/main/jniLibs/arm64-v8a/libopenal.so
cp $OPENJKDF2_BUILD_DIR/SDL/libSDL2.so $QUEST_PACKAGE_DIR/app/src/main/jniLibs/arm64-v8a/libSDL2.so
cp $OPENJKDF2_BUILD_DIR/SDL_mixer/libSDL2_mixer.so $QUEST_PACKAGE_DIR/app/src/main/jniLibs/arm64-v8a/libSDL2_mixer.so

# Copy OpenXR loader if built
if [ -f "$OPENJKDF2_BUILD_DIR/_deps/openxr_loader-build/src/loader/libopenxr_loader.so" ]; then
    cp $OPENJKDF2_BUILD_DIR/_deps/openxr_loader-build/src/loader/libopenxr_loader.so \
       $QUEST_PACKAGE_DIR/app/src/main/jniLibs/arm64-v8a/libopenxr_loader.so
fi

echo "=== Build Complete ==="
echo "Native libraries copied to: $QUEST_PACKAGE_DIR/app/src/main/jniLibs/arm64-v8a/"
echo ""
echo "Next steps:"
echo "1. Set up the Quest VR Android project (Gradle, Java sources, etc.)"
echo "2. Run './gradlew assembleDebug' in the quest-vr package directory"
echo "3. Install the APK on your Quest 3 using 'adb install'"
