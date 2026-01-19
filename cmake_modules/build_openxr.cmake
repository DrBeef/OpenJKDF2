# OpenXR SDK fetch and build configuration

include(FetchContent)

FetchContent_Declare(
    openxr_loader
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG release-1.0.34
    GIT_SHALLOW TRUE
)

# Configure OpenXR build options
set(BUILD_ALL_EXTENSIONS OFF CACHE BOOL "" FORCE)
set(BUILD_LOADER ON CACHE BOOL "" FORCE)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)

if(TARGET_ANDROID)
    # Android: Build dynamic loader for Quest
    set(DYNAMIC_LOADER ON CACHE BOOL "" FORCE)
    # Disable Vulkan requirement for Android OpenGL ES builds
    set(BUILD_WITH_XLIB_HEADERS OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_XCB_HEADERS OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_WAYLAND_HEADERS OFF CACHE BOOL "" FORCE)
else()
    # Desktop: Build static loader
    set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
endif()

FetchContent_GetProperties(openxr_loader)
if(NOT openxr_loader_POPULATED)
    FetchContent_Populate(openxr_loader)
    add_subdirectory(${openxr_loader_SOURCE_DIR} ${openxr_loader_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(OPENXR_INCLUDE_DIR ${openxr_loader_SOURCE_DIR}/include)
set(OPENXR_LIBRARY openxr_loader)
