# Build gl4es - OpenGL to OpenGL ES translation layer
# Only used for Android builds

set(GL4ES_ROOT ${CMAKE_BINARY_DIR}/gl4es)
set(GL4ES_SOURCE_DIR ${CMAKE_BINARY_DIR}/gl4es-src)

# gl4es provides standard GL headers that translate to GLES
# Headers are in the source tree include/ directory
# Library is built in the source tree lib/ directory (gl4es CMakeLists.txt uses CMAKE_SOURCE_DIR)
set(GL4ES_FOUND TRUE)
set(GL4ES_INCLUDE_DIRS ${GL4ES_SOURCE_DIR}/include)
# gl4es builds libGL.so.1 in the source lib directory
set(GL4ES_LIBRARIES ${GL4ES_SOURCE_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}GL${CMAKE_SHARED_LIBRARY_SUFFIX}.1)

# Output directory for Android packaging - this is where Gradle looks for native libs
# Define this BEFORE ExternalProject_Add so it's available for BUILD_BYPRODUCTS
# Use CMAKE_BINARY_DIR as fallback if CMAKE_LIBRARY_OUTPUT_DIRECTORY is not set
if(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(GL4ES_OUTPUT_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
else()
    set(GL4ES_OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif()
set(GL4ES_OUTPUT_LIBRARY ${GL4ES_OUTPUT_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}GL${CMAKE_SHARED_LIBRARY_SUFFIX})

set(GL4ES_CMAKE_MAKE_PROGRAM_ARG "")
if(CMAKE_MAKE_PROGRAM)
    set(GL4ES_CMAKE_MAKE_PROGRAM_ARG "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
endif()

set(GL4ES_ANDROID_ABI_ARG "")
if(ANDROID_ABI)
    set(GL4ES_ANDROID_ABI_ARG "-DANDROID_ABI=${ANDROID_ABI}")
endif()

# Force Android defines through C flags since NDK toolchain may conflict with gl4es ANDROID option
set(GL4ES_C_FLAGS "-O3 -DANDROID -DUSE_ANDROID_LOG -DNOX11 -DNO_GBM -DDEFAULT_ES=2")

ExternalProject_Add(
    gl4es_ext
    GIT_REPOSITORY      https://github.com/ptitSeb/gl4es.git
    GIT_TAG             master
    GIT_SHALLOW         TRUE
    SOURCE_DIR          ${GL4ES_SOURCE_DIR}
    BINARY_DIR          ${GL4ES_ROOT}
    INSTALL_DIR         ${GL4ES_ROOT}
    UPDATE_DISCONNECTED TRUE
    CMAKE_ARGS          --toolchain ${CMAKE_TOOLCHAIN_FILE}
                        --install-prefix ${GL4ES_ROOT}
                        -DCMAKE_BUILD_TYPE:STRING=Release
                        "-DCMAKE_C_FLAGS:STRING=${GL4ES_C_FLAGS}"
                        -DNOX11:BOOL=TRUE
                        -DNOEGL:BOOL=FALSE
                        -DUSE_ANDROID_LOG:BOOL=TRUE
                        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
                        ${GL4ES_ANDROID_ABI_ARG}
                        ${GL4ES_CMAKE_MAKE_PROGRAM_ARG}
    BUILD_BYPRODUCTS    ${GL4ES_LIBRARIES} ${GL4ES_OUTPUT_LIBRARY}
)

# Copy gl4es library to output directory for Android APK packaging
# gl4es builds libGL.so.1, we copy it as libGL.so for Android compatibility
ExternalProject_Add_Step(
    gl4es_ext copy_library
    DEPENDEES build
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GL4ES_LIBRARIES}
        ${GL4ES_OUTPUT_LIBRARY}
    COMMENT "Copying gl4es library to output directory for APK packaging"
    ALWAYS FALSE
)

# Create imported target pointing to the copied library
if(NOT TARGET gl4es::gl4es)
    add_library(gl4es::gl4es SHARED IMPORTED)
endif()
add_dependencies(gl4es::gl4es gl4es_ext)
file(MAKE_DIRECTORY ${GL4ES_INCLUDE_DIRS})
set_target_properties(
    gl4es::gl4es PROPERTIES
    IMPORTED_LOCATION ${GL4ES_OUTPUT_LIBRARY}
)
target_include_directories(gl4es::gl4es INTERFACE ${GL4ES_INCLUDE_DIRS})
target_link_directories(gl4es::gl4es INTERFACE ${GL4ES_OUTPUT_DIR})
