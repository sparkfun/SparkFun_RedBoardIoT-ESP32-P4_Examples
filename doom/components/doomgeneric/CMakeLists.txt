# doomgeneric (https://github.com/ozkl/doomgeneric), GPL-2.0, built straight from the
# external/doomgeneric submodule. The ESP32-P4 platform layer lives in main/.
set(DG_DIR ${CMAKE_CURRENT_LIST_DIR}/../../external/doomgeneric/doomgeneric)
if(NOT EXISTS ${DG_DIR}/doomgeneric.c)
    message(FATAL_ERROR "external/doomgeneric is empty - run: git submodule update --init")
endif()

# Every engine source except the other platforms' front ends and SDL/Allegro sound.
# The engine is built without FEATURE_SOUND, so there's no sound or music.
file(GLOB DOOM_SRCS "${DG_DIR}/*.c")
list(FILTER DOOM_SRCS EXCLUDE REGEX "/(doomgeneric_[a-z]+|i_sdl[a-z]*|i_allegro[a-z]*|gusconf|icon)\\.c$")

idf_component_register(SRCS ${DOOM_SRCS} INCLUDE_DIRS ${DG_DIR} REQUIRES esp_system)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    DOOMGENERIC_RESX=320 DOOMGENERIC_RESY=200
    NORMALUNIX LINUX _DEFAULT_SOURCE
    exit=dg_exit)          # Doom quitting / I_Error -> main/dg_esp.c instead of a reboot loop
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -w -Wno-error -fno-strict-aliasing)
