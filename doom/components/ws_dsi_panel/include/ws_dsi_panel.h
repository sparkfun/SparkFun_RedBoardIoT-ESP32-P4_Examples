// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 SparkFun Electronics
// ws_dsi_panel.h - Waveshare MIPI-DSI touch panels on an ESP32-P4, auto-detected
//
//  4.3-DSI-TOUCH-A : ST7701S, 480 x 800 portrait, 2 lanes @ 500 Mbps, 30 MHz DPI
//  7-DSI-TOUCH-C   : EK79007, 1024 x 600 landscape, 2 lanes @ 1000 Mbps, 52 MHz DPI
//
// Both carry a panel MCU at I2C 0x45 (reg 0x95 = power/reset, reg 0x96 = backlight)
// and a GT911 touch controller at 0x5D or 0x14. Init sequences and timings come
// from Waveshare's own ESP32-P4 BSP (waveshare/esp32_p4_platform).
//
// Detection: after the MCU powers the panel, the GT911's configuration block
// (registers 0x8048-0x804B) holds the panel's X/Y resolution - 480x800 or 1024x600.
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "driver/i2c_master.h"

typedef enum {
    WS_PANEL_UNKNOWN = 0,
    WS_PANEL_4_3_A,     // 4.3-DSI-TOUCH-A (also 4-DSI-TOUCH-A)
    WS_PANEL_7_C,       // 7-DSI-TOUCH-C
} ws_panel_type_t;

typedef struct {
    ws_panel_type_t         type;
    const char             *name;
    int                     width, height;   // native frame buffer size
    esp_lcd_panel_handle_t  panel;
    esp_lcd_touch_handle_t  touch;           // NULL if the GT911 was not found
    i2c_master_bus_handle_t i2c;
    uint8_t                *fb;              // RGB888 frame buffer, bytes B,G,R
    uint8_t                *fb2;             // second frame buffer (only if ws_panel_set_fb_count(2))
    uint8_t                 mcu_found;
    uint8_t                 touch_addr;
    uint8_t                 detected;        // 1 = type came from the GT911, 0 = forced / fallback
    uint16_t                gt_x, gt_y;      // resolution read from the GT911 (0 if unread)
} ws_panel_t;

// Ask for 1 (default) or 2 frame buffers before ws_panel_start(). With two, draw into the
// hidden one and show it with esp_lcd_panel_draw_bitmap(p->panel, 0, 0, w, h, buf).
void      ws_panel_set_fb_count(int n);
esp_err_t ws_panel_start(ws_panel_t *p);
esp_err_t ws_panel_backlight(ws_panel_t *p, uint8_t level);   // 0..255
