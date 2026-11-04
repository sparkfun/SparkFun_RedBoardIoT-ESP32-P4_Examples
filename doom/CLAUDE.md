# SparkFun_ESP32P4_Doom

Doom (doomgeneric) on the SparkFun IoT RedBoard - ESP32-P4 with a Waveshare 7-DSI-TOUCH-C or
4.3-DSI-TOUCH-A panel. ESP-IDF >= 6.0 project, target esp32p4. User-facing docs are in README.md.

## Layout

- `main/`: platform layer. `dg_esp.c` (video scale/rotate, WAD mount, exit), `controls.c` (touch overlay +
  Qwiic Joystick at I2C 0x20), `font_data.c` (generated, don't edit).
- `components/doomgeneric/CMakeLists.txt` builds the engine straight from the `external/doomgeneric`
  submodule (ozkl/doomgeneric). It filters out other platforms' front ends, SDL/Allegro sound,
  `gusconf.c` and `icon.c`. Built without `FEATURE_SOUND`: there's no audio support.
  Keep the submodule unmodified.
- `components/ws_dsi_panel/`: panel auto-detect + DSI init (Apache-2.0, Waveshare-derived tables).
  Espressif LCD/touch drivers come from the component registry, pinned in its `idf_component.yml`.
- `bin/doom_full.bin`: prebuilt merged image, flashed at 0x0. `wad/DOOM1.WAD`: shareware v1.9.

## Gotchas

- `wad/` is packed whole into the FAT image (`fatfs_create_rawflash_image` in main/CMakeLists.txt),
  so put nothing else in that directory.
- doomgeneric is compiled with `exit=dg_exit`: code 0 (Quit) restarts the board, non-zero halts with the
  error in the log, so there's no reboot loop.
- Partitions (`partitions.csv`): factory app at 0x10000 (2 MB), `wad` FAT at 0x210000 (5 MB).
- After any source change that should ship, rebuild and regenerate the image:
  `cd build && python -m esptool --chip esp32p4 merge-bin -o ../bin/doom_full.bin @flash_args`
- Font: `python3 tools/gen_font.py` (Pillow + DejaVu Sans Mono Bold) regenerates `main/font_data.c`.
- Project-authored files carry SPDX tags and `Copyright (c) 2026 SparkFun Electronics`: MIT, except ws_dsi_panel
  (Apache-2.0). The built firmware is GPL-2.0.
  Keep third-party license files (LICENSE, main/FONT_LICENSE.txt) in place.
- This project lives in `doom/` of the SparkFun_RedBoardIoT-ESP32-P4_Examples repo; the doomgeneric submodule
  is registered in the repo-root `.gitmodules`. Keep this example Doom-only: don't add references to other apps.
