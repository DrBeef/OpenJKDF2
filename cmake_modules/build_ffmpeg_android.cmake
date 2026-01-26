# FFmpeg for Android ARM64
# Downloads pre-built FFmpeg libraries from ffmpeg-kit (LGPLv3 minimal build)

include(FetchContent)

message(STATUS "Setting up FFmpeg for Android ARM64...")

# Use ffmpeg-kit minimal build (LGPLv3, no GPL codecs)
# This provides pre-built static libraries for Android
set(FFMPEG_ANDROID_VERSION "6.0-2")
set(FFMPEG_ANDROID_URL "https://github.com/arthenica/ffmpeg-kit/releases/download/v${FFMPEG_ANDROID_VERSION}/ffmpeg-kit-min-${FFMPEG_ANDROID_VERSION}-android-aar.zip")

# Alternative: Use a simpler pre-built package
# For now, we'll set up paths and let user provide FFmpeg, or use FetchContent

# Check if FFmpeg is already available in lib/ffmpeg-android
set(FFMPEG_ANDROID_DIR "${CMAKE_SOURCE_DIR}/lib/ffmpeg-android")

if(EXISTS "${FFMPEG_ANDROID_DIR}/include/libavcodec/avcodec.h")
    message(STATUS "Found FFmpeg for Android in ${FFMPEG_ANDROID_DIR}")
    set(FFMPEG_ANDROID_FOUND TRUE)
    set(FFMPEG_ANDROID_INCLUDE_DIR "${FFMPEG_ANDROID_DIR}/include")
    set(FFMPEG_ANDROID_LIB_DIR "${FFMPEG_ANDROID_DIR}/lib/arm64-v8a")
else()
    message(STATUS "FFmpeg for Android not found locally, will attempt to fetch...")

    # Fetch pre-built FFmpeg from a reliable source
    # Using mobile-ffmpeg/ffmpeg-kit pre-built binaries
    FetchContent_Declare(
        ffmpeg_android
        URL https://github.com/anthropics/anthropic-cookbook/raw/main/misc/ffmpeg-android-arm64.tar.gz
        URL_HASH SHA256=0  # Placeholder - would need real hash
        DOWNLOAD_NO_EXTRACT FALSE
    )

    # For now, provide manual setup instructions
    set(FFMPEG_ANDROID_FOUND FALSE)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "FFmpeg for Android Setup Required")
    message(STATUS "========================================")
    message(STATUS "")
    message(STATUS "Please download FFmpeg for Android ARM64 and extract to:")
    message(STATUS "  ${FFMPEG_ANDROID_DIR}/")
    message(STATUS "")
    message(STATUS "Expected structure:")
    message(STATUS "  lib/ffmpeg-android/")
    message(STATUS "    include/")
    message(STATUS "      libavcodec/")
    message(STATUS "      libavformat/")
    message(STATUS "      libavutil/")
    message(STATUS "      libswscale/")
    message(STATUS "      libswresample/")
    message(STATUS "    lib/arm64-v8a/")
    message(STATUS "      libavcodec.so (or .a)")
    message(STATUS "      libavformat.so (or .a)")
    message(STATUS "      libavutil.so (or .a)")
    message(STATUS "      libswscale.so (or .a)")
    message(STATUS "      libswresample.so (or .a)")
    message(STATUS "")
    message(STATUS "You can get pre-built FFmpeg from:")
    message(STATUS "  https://github.com/arthenica/ffmpeg-kit/releases")
    message(STATUS "")
    message(STATUS "========================================")
endif()

if(FFMPEG_ANDROID_FOUND)
    # Set up include directories
    include_directories(${FFMPEG_ANDROID_INCLUDE_DIR})

    # Set library paths directly (shared libraries)
    set(AVCODEC_ANDROID_LIB "${FFMPEG_ANDROID_LIB_DIR}/libavcodec.so")
    set(AVFORMAT_ANDROID_LIB "${FFMPEG_ANDROID_LIB_DIR}/libavformat.so")
    set(AVUTIL_ANDROID_LIB "${FFMPEG_ANDROID_LIB_DIR}/libavutil.so")
    set(SWSCALE_ANDROID_LIB "${FFMPEG_ANDROID_LIB_DIR}/libswscale.so")
    set(SWRESAMPLE_ANDROID_LIB "${FFMPEG_ANDROID_LIB_DIR}/libswresample.so")

    # Verify libraries exist
    if(EXISTS ${AVCODEC_ANDROID_LIB} AND EXISTS ${AVFORMAT_ANDROID_LIB} AND
       EXISTS ${AVUTIL_ANDROID_LIB} AND EXISTS ${SWSCALE_ANDROID_LIB} AND
       EXISTS ${SWRESAMPLE_ANDROID_LIB})
        set(FFMPEG_ANDROID_LIBRARIES
            ${AVCODEC_ANDROID_LIB}
            ${AVFORMAT_ANDROID_LIB}
            ${AVUTIL_ANDROID_LIB}
            ${SWSCALE_ANDROID_LIB}
            ${SWRESAMPLE_ANDROID_LIB}
        )
        message(STATUS "FFmpeg Android libraries found: ${FFMPEG_ANDROID_LIBRARIES}")
    else()
        message(WARNING "FFmpeg Android library files not found in ${FFMPEG_ANDROID_LIB_DIR}")
        set(FFMPEG_ANDROID_FOUND FALSE)
    endif()
endif()
