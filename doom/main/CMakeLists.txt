idf_component_register(
    SRCS dg_esp.c controls.c font_data.c
    INCLUDE_DIRS .
    REQUIRES doomgeneric ws_dsi_panel fatfs esp_driver_i2c esp_timer esp_mm)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)

# DOOM1.WAD (shareware v1.9, unmodified) -> read-only FAT image for the "wad"
# partition. `idf.py flash` writes it too.
fatfs_create_rawflash_image(wad ${CMAKE_CURRENT_LIST_DIR}/../wad FLASH_IN_PROJECT)
