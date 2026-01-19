set(SYMBOLS_FILE ${PROJECT_SOURCE_DIR}/symbols.syms)
set(GLOBALS_H ${CMAKE_CURRENT_BINARY_DIR}/generated/globals.h)
set(GLOBALS_C ${CMAKE_CURRENT_BINARY_DIR}/generated/globals.c)
set(GLOBALS_H_COG ${PROJECT_SOURCE_DIR}/src/globals.h.cog)
set(GLOBALS_C_COG ${PROJECT_SOURCE_DIR}/src/globals.c.cog)

make_directory(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# On Windows host (including cross-compiling for Android), use system python
if(CMAKE_HOST_WIN32)
    set(PYTHON_EXE "python")
    # Don't add python to DEPENDS on Windows - it's a system command, not a build target
    set(PYTHON_DEPENDS "")
    set(COGAPP_DEPENDS "")
else()
    set(PYTHON_EXE "${CMAKE_CURRENT_BINARY_DIR}/cogapp_venv/bin/python3")
    set(PYTHON_DEPENDS "${PYTHON_EXE}")
    set(COGAPP_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cogapp_venv/bin/cog")
endif()

list(JOIN EMBEDDED_RESOURCES "+" EMBEDDED_RESOURCES_SEPARATED)

# All of our pre-build steps
add_custom_command(
    OUTPUT ${GLOBALS_C}
    COMMAND ${PYTHON_EXE} -m cogapp -d -D symbols_fpath="${SYMBOLS_FILE}" -D project_root="${PROJECT_SOURCE_DIR}" -D embedded_resources="${EMBEDDED_RESOURCES_SEPARATED}" -o ${GLOBALS_C} ${GLOBALS_C_COG}
    DEPENDS ${SYMBOLS_FILE} ${GLOBALS_C_COG} ${GLOBALS_H} ${EMBEDDED_RESOURCES} ${PYTHON_DEPENDS} ${COGAPP_DEPENDS}
)

# Only create venv on non-Windows hosts
if(NOT CMAKE_HOST_WIN32)
    add_custom_command(
        OUTPUT ${PYTHON_EXE}
        COMMAND python3 -m venv ${CMAKE_CURRENT_BINARY_DIR}/cogapp_venv
    )
    add_custom_command(
        OUTPUT ${COGAPP_DEPENDS}
        COMMAND ${PYTHON_EXE} -m pip install cogapp
        DEPENDS ${PYTHON_EXE}
    )
endif()

add_custom_command(
    OUTPUT ${GLOBALS_H}
    COMMAND ${PYTHON_EXE} -m cogapp -d -D symbols_fpath="${SYMBOLS_FILE}" -D project_root="${PROJECT_SOURCE_DIR}" -D embedded_resources="${EMBEDDED_RESOURCES_SEPARATED}" -o ${GLOBALS_H} ${GLOBALS_H_COG}
    DEPENDS ${SYMBOLS_FILE} ${GLOBALS_H_COG} ${PYTHON_DEPENDS} ${COGAPP_DEPENDS} ${EMBEDDED_RESOURCES}
)

add_custom_command(
    PRE_BUILD
    OUTPUT ${BIN_NAME}
    DEPENDS ${GLOBALS_C} ${GLOBALS_H}
)

# HACK
list(REMOVE_ITEM ENGINE_SOURCE_FILES ${GLOBALS_C})
list(APPEND ENGINE_SOURCE_FILES ${GLOBALS_C})