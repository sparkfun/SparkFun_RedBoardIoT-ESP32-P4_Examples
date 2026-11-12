# SparkFun RedBoard IoT - ESP32-P4 Examples

Examples and demos for the **SparkFun IoT RedBoard - ESP32-P4**.

Each example is in its own folder, with a README covering the hardware it needs, how to wire it up, and how to build and run it. Most examples are self-contained [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/) projects, and some are [MicroPython](https://github.com/sparkfun/micropython/releases) scripts.

## Examples

| Example | Description | Hardware beyond the RedBoard | Platform |
|---|---|---|---|
| [Doom](doom/) | The original 1993 Doom (shareware episode) running full-screen, with multi-touch controls and optional Qwiic Joystick support. Includes a prebuilt image you can flash without installing ESP-IDF. | Waveshare 7" or 4.3" MIPI-DSI touch panel | ESP-IDF v6.0+ |
| [ESP-Brookesia Phone](esp_brookesia_phone/) | A phone-style touch UI built with [ESP-Brookesia](https://github.com/espressif/esp-brookesia), with apps that use the display, camera, Wi-Fi and SD card. Ported from Espressif's [esp-dev-kits](https://github.com/espressif/esp-dev-kits) example. | Waveshare 4.3" MIPI-DSI touch panel, MIPI-CSI camera | ESP-IDF v5.5.3+ |
| [People in Space](people_in_space/) | Connects over Ethernet or Wi-Fi, downloads the list of people currently in space, and prints their names. Optionally shows them on a Qwiic SerLCD, with the button to step through names. | None (optional Qwiic SerLCD) | MicroPython |

## Getting started

Clone the repository with its submodules:

```bash
git clone --recursive https://github.com/sparkfun/SparkFun_RedBoardIoT-ESP32-P4_Examples.git
```

If you already cloned it without `--recursive`, run `git submodule update --init`.

Then open the folder for the example you want and follow its README. A typical ESP-IDF build looks like this:

```bash
cd <example folder>
idf.py set-target esp32p4
idf.py build
idf.py -p <port> flash monitor
```

Examples don't all use the same ESP-IDF version, so check the table above before you build. MicroPython examples don't need a build: flash the MicroPython firmware once, then run the script on the board.

## Adding an example

1. Put the ESP-IDF project or MicroPython script in its own top-level folder.
2. Give it a README covering the required hardware, hookup, ESP-IDF or MicroPython version, and build and run steps.
3. For ESP-IDF projects, add a `.gitignore` that excludes `build/`, `managed_components/`, `dependencies.lock` and `sdkconfig`. Commit `sdkconfig.defaults` instead.
4. Add any third-party code that should stay unmodified as a git submodule under the example folder.
5. Add a row to the [Examples](#examples) table above.
