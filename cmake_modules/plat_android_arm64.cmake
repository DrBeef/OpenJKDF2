include(cmake_modules/target_android_all.cmake)

macro(plat_initialize)
    message( STATUS "Targeting Android ARM64" )

    set(BIN_NAME "openjkdf2-armv8a")

    add_definitions(-DARCH_64BIT)
    add_definitions(-DTARGET_ANDROID)
    add_definitions(-DLINUX)
    #add_definitions(-DSTDSOUND_NULL)

    include(cmake_modules/plat_feat_full_sdl2.cmake)
    set(TARGET_USE_PHYSFS FALSE)
    set(TARGET_USE_CURL FALSE)
    set(TARGET_BUILD_TESTS FALSE)
    set(TARGET_FIND_OPENAL FALSE)
    set(TARGET_USE_GAMENETWORKINGSOCKETS FALSE)
    
    set(TARGET_ANDROID TRUE)
    set(TARGET_ANDROID_ARM64 TRUE)

    # FFmpeg support for Android
    if(TARGET_USE_FFMPEG)
        include(cmake_modules/build_ffmpeg_android.cmake)
        if(FFMPEG_ANDROID_FOUND)
            add_definitions(-DUSE_FFMPEG_VIDEO)
            message(STATUS "FFmpeg MP4 video support enabled for Android")
        else()
            message(WARNING "FFmpeg not available for Android - disabling MP4 video support")
            set(TARGET_USE_FFMPEG FALSE)
        endif()
    endif()

    # gl4es provides OpenGL to OpenGL ES translation for standard Android builds
    # Quest VR uses native OpenGL ES 3 directly for better performance
    include_directories(${PROJECT_SOURCE_DIR}/lib/freeglut/include)

    if(TARGET_USE_VR)
        # Native GLES3 for Quest VR - no GL4ES translation layer
        add_definitions(-DTARGET_ANDROID_NATIVE_GLES)
    else()
        # GL4ES for standard Android builds
        add_definitions(-DGL4ES)
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=c11 -fshort-wchar -Werror=implicit-function-declaration -Wno-unused-variable -Wno-parentheses")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fshort-wchar")
    add_link_options(-fshort-wchar)
endmacro()

macro(plat_specific_deps)
    if(TARGET_USE_VR)
        # Quest VR: Native GLES3, no gl4es dependency
        set(SDL2_COMMON_LIBS SDL2main SDL::SDL ${SDL_MIXER_DEPS} SDL::Mixer OpenAL::OpenAL)
    else()
        # Standard Android: Use gl4es for OpenGL translation
        set(SDL2_COMMON_LIBS SDL2main SDL::SDL ${SDL_MIXER_DEPS} SDL::Mixer OpenAL::OpenAL gl4es::gl4es)
        # Add gl4es include directory for GL headers
        include_directories(${GL4ES_INCLUDE_DIRS})
    endif()
endmacro()

