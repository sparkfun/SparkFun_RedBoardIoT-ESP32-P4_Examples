// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SparkFun Electronics
// controls.h - on-screen touch buttons + SparkFun Qwiic Joystick -> Doom key events
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_lcd_touch.h"

typedef struct {
    int W, H;          // landscape canvas size in pixels (1024x600 or 800x480)
} controls_layout_t;

void controls_init(const controls_layout_t *l, esp_lcd_touch_handle_t touch, i2c_master_bus_handle_t i2c,
                   int panel_w, int panel_h, int touch_rotation);
// Read touch + joystick, turn changes into queued key events. Call once per frame.
void controls_update(void);
// Pop one key event; returns false when the queue is empty.
bool controls_pop(int *pressed, unsigned char *key);
bool controls_joystick_present(void);

// Overlay drawing: called by the video code for each control so it can paint
// outlines/labels in canvas coordinates.
typedef void (*plot_fn)(int cx, int cy, uint32_t rgb, uint8_t alpha);
void controls_draw(plot_fn plot);
