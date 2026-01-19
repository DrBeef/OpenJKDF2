# VR/OpenXR feature configuration for OpenJKDF2
# Added: VR support build configuration

if(TARGET_ANDROID)
    # Android/Quest VR: Build OpenXR loader for Android
    message(STATUS "Configuring VR for Android/Quest...")

    # Build the OpenXR loader for Android
    include(build_openxr)
else()
    # Desktop VR: Build or find OpenXR loader
    find_package(OpenXR 1.0 QUIET)

    if(NOT OpenXR_FOUND)
        message(STATUS "OpenXR not found on system, fetching from source...")
        include(build_openxr)
    else()
        message(STATUS "Using system OpenXR: ${OpenXR_VERSION}")
        set(OPENXR_INCLUDE_DIR ${OpenXR_INCLUDE_DIRS})
        set(OPENXR_LIBRARY ${OpenXR_LIBRARIES})
    endif()
endif()

# Add VR source files
file(GLOB VR_SRCS
    ${PROJECT_SOURCE_DIR}/src/Platform/VR/*.c
    ${PROJECT_SOURCE_DIR}/src/Platform/VR/*.cpp
)
list(APPEND ENGINE_SOURCE_FILES ${VR_SRCS})

# Add VR include directories
include_directories(${OPENXR_INCLUDE_DIR})

# Add compile definitions for VR support
add_compile_definitions(PLATFORM_VR)
add_compile_definitions(TARGET_USE_OPENXR)

# Link OpenXR library
macro(plat_vr_link_deps)
    if(TARGET_ANDROID)
        # On Android/Quest, link against the built OpenXR loader
        target_link_libraries(${BIN_NAME} PRIVATE ${OPENXR_LIBRARY})
        target_link_libraries(${BIN_NAME} PRIVATE EGL GLESv3)
    else()
        target_link_libraries(${BIN_NAME} PRIVATE ${OPENXR_LIBRARY})
    endif()
endmacro()

message(STATUS "VR support enabled with OpenXR")
