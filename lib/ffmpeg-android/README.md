# FFmpeg for Android ARM64

This directory should contain pre-built FFmpeg libraries for Android ARM64 (aarch64).

## Directory Structure

```
lib/ffmpeg-android/
├── include/
│   ├── libavcodec/
│   │   └── avcodec.h (and other headers)
│   ├── libavformat/
│   │   └── avformat.h (and other headers)
│   ├── libavutil/
│   │   └── avutil.h (and other headers)
│   ├── libswscale/
│   │   └── swscale.h (and other headers)
│   └── libswresample/
│       └── swresample.h (and other headers)
└── lib/arm64-v8a/
    ├── libavcodec.so (or libavcodec.a)
    ├── libavformat.so (or libavformat.a)
    ├── libavutil.so (or libavutil.a)
    ├── libswscale.so (or libswscale.a)
    └── libswresample.so (or libswresample.a)
```

## Getting FFmpeg for Android

### Option 1: FFmpeg-Kit (Recommended)

Download from: https://github.com/arthenica/ffmpeg-kit/releases

1. Download the latest `ffmpeg-kit-min-X.X-android-aar.zip` (minimal build, LGPL)
2. Extract the AAR file (it's a ZIP)
3. Inside, find `jni/arm64-v8a/` for the .so files
4. Headers are in the `headers/` directory

### Option 2: Build from Source

```bash
# Clone FFmpeg
git clone https://git.ffmpeg.org/ffmpeg.git
cd ffmpeg

# Configure for Android ARM64
./configure \
    --target-os=android \
    --arch=aarch64 \
    --enable-cross-compile \
    --cross-prefix=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android- \
    --cc=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android31-clang \
    --enable-shared \
    --disable-static \
    --disable-doc \
    --disable-programs \
    --disable-everything \
    --enable-decoder=h264,hevc,aac,mp3 \
    --enable-demuxer=mov,mp4,m4a \
    --enable-protocol=file

make -j$(nproc)
make install DESTDIR=./output
```

### Option 3: Pre-built from Mobile FFmpeg

Download from: https://github.com/ArtifexSoftware/mupdf-android-viewer/tree/master/libmupdf/jni

## Quick Setup Script

If you have FFmpeg-kit AAR downloaded:

```bash
# Extract the AAR
unzip ffmpeg-kit-min-6.0-2-android-aar.zip -d ffmpeg-kit

# Copy headers
cp -r ffmpeg-kit/headers/* lib/ffmpeg-android/include/

# Copy ARM64 libraries
cp ffmpeg-kit/jni/arm64-v8a/*.so lib/ffmpeg-android/lib/arm64-v8a/
```

## Verification

After setup, you should have these files:
- `include/libavcodec/avcodec.h`
- `include/libavformat/avformat.h`
- `lib/arm64-v8a/libavcodec.so`
- etc.

CMake will automatically detect FFmpeg when rebuilding.
