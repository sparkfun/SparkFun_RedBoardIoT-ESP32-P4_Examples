// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SparkFun Electronics
// dg_esp.c - doomgeneric platform layer for the SparkFun IoT RedBoard ESP32-P4
//
//   video : Doom's 320x200 frame, integer-scaled into the DSI panel's frame buffer
//           (7" 1024x600: 3x = 960x600; 4.3" 480x800: 2x = 640x400, rotated to landscape),
//           with the touch controls drawn on top.
//   input : controls.c (touch overlay + Qwiic Joystick)
//   files : DOOM1.WAD lives in the read-only FAT partition "wad", mounted at /wad
//   exit  : quitting from Doom's menu restarts Doom; a fatal error stops with the
//           reason in the serial log (instead of a reboot loop).
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_attr.h"
#include "freertos/semphr.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "doomgeneric.h"
#include "ws_dsi_panel.h"
#include "controls.h"
#include "sdkconfig.h"

static const char *TAG = "doom";

static ws_panel_t s_p;
static uint8_t *s_fb;          // the hidden buffer we draw into
static uint8_t *s_front;       // the one being scanned out
static SemaphoreHandle_t s_vsync;
static int PW, PH;            // physical panel size
static int CW, CH;            // landscape canvas size
static int ROT;               // canvas -> panel rotation
static int S, OX, OY;         // integer scale and offset of the 320x200 picture in the canvas
static uint8_t *s_line;       // one scaled output line

// ---------------------------------------------------------------------------
// leaving Doom
// ---------------------------------------------------------------------------
// doomgeneric is built with exit=dg_exit, so this is where Doom's I_Quit (code 0)
// and I_Error (non-zero) end up.
void dg_exit(int code)
{
    if (s_fb) { memset(s_fb, 0, (size_t)PW * PH * 3); esp_cache_msync(s_fb, (size_t)PW * PH * 3, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED); }
    if (code == 0) {
        ESP_LOGI(TAG, "Doom quit - restarting");
        esp_restart();
    }
    ESP_LOGE(TAG, "Doom stopped (%d) - see the messages above. Press RESET to try again.", code);
    for (;;) vTaskDelay(portMAX_DELAY);
}

// ---------------------------------------------------------------------------
// video
// ---------------------------------------------------------------------------
static inline uint8_t *px_at(int cx, int cy)   // canvas -> frame buffer byte pointer
{
    int dx, dy;
    switch (ROT) {
    case 90:  dx = PW - 1 - cy; dy = cx; break;
    case 180: dx = PW - 1 - cx; dy = PH - 1 - cy; break;
    case 270: dx = cy; dy = PH - 1 - cx; break;
    default:  dx = cx; dy = cy; break;
    }
    return s_fb + ((size_t)dy * PW + dx) * 3;
}

static void plot(int cx, int cy, uint32_t rgb, uint8_t a)
{
    if (cx < 0 || cy < 0 || cx >= CW || cy >= CH) return;
    uint8_t *p = px_at(cx, cy);   // bytes B,G,R
    int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    p[0] = (uint8_t)((p[0] * (255 - a) + b * a) / 255);
    p[1] = (uint8_t)((p[1] * (255 - a) + g * a) / 255);
    p[2] = (uint8_t)((p[2] * (255 - a) + r * a) / 255);
}

// Copy Doom's 320x200 XRGB frame into the panel, scaled by S and rotated by ROT.
// Each output line is built once and then memcpy'd S times.
static void blit(void)
{
    const uint32_t *src = DG_ScreenBuffer;
    if (ROT == 0 || ROT == 180) {
        const int n = DOOMGENERIC_RESX * S;
        for (int sy = 0; sy < DOOMGENERIC_RESY; sy++) {
            const uint32_t *row = src + sy * DOOMGENERIC_RESX;
            for (int i = 0; i < n; i++) {
                uint32_t c = row[(ROT == 0 ? i : n - 1 - i) / S];
                s_line[i * 3 + 0] = c & 255; s_line[i * 3 + 1] = (c >> 8) & 255; s_line[i * 3 + 2] = (c >> 16) & 255;
            }
            for (int k = 0; k < S; k++) {
                int cy = OY + sy * S + k;
                int dy = ROT == 0 ? cy : PH - 1 - cy;
                int dx = ROT == 0 ? OX : PW - OX - n;
                memcpy(s_fb + ((size_t)dy * PW + dx) * 3, s_line, n * 3);
            }
        }
    } else {
        // portrait panel: a source COLUMN becomes a panel row
        const int n = DOOMGENERIC_RESY * S;
        for (int sx = 0; sx < DOOMGENERIC_RESX; sx++) {
            for (int i = 0; i < n; i++) {
                int sy = (ROT == 90 ? n - 1 - i : i) / S;
                uint32_t c = src[sy * DOOMGENERIC_RESX + sx];
                s_line[i * 3 + 0] = c & 255; s_line[i * 3 + 1] = (c >> 8) & 255; s_line[i * 3 + 2] = (c >> 16) & 255;
            }
            for (int k = 0; k < S; k++) {
                int cx = OX + sx * S + k;
                int dy = ROT == 90 ? cx : PH - 1 - cx;
                int dx = ROT == 90 ? PW - OY - n : OY;
                memcpy(s_fb + ((size_t)dy * PW + dx) * 3, s_line, n * 3);
            }
        }
    }
}

static bool IRAM_ATTR on_vsync(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *e, void *ctx)
{
    BaseType_t woke = pdFALSE;
    xSemaphoreGiveFromISR(s_vsync, &woke);
    return woke == pdTRUE;
}

void DG_Init(void)
{
    ws_panel_set_fb_count(2);               // double buffer: draw hidden, then flip
    if (ws_panel_start(&s_p) != ESP_OK) { ESP_LOGE(TAG, "no display"); dg_exit(-1); }
    PW = s_p.width; PH = s_p.height;
    s_front = s_p.fb;
    s_fb = s_p.fb2 ? s_p.fb2 : s_p.fb;
    s_vsync = xSemaphoreCreateBinary();
    esp_lcd_dpi_panel_event_callbacks_t cbs = { .on_vsync = on_vsync };
    esp_lcd_dpi_panel_register_event_callbacks(s_p.panel, &cbs, NULL);
    bool portrait = PH > PW;
    ROT = portrait ? 90 : 180;                 // upright for both supported panels
#if CONFIG_DOOM_FLIP
    ROT = (ROT + 180) % 360;
#endif
    int trot = portrait ? ROT : (ROT + 180) % 360;
    CW = portrait ? PH : PW;
    CH = portrait ? PW : PH;
    S = CW / DOOMGENERIC_RESX;
    if (CH / DOOMGENERIC_RESY < S) S = CH / DOOMGENERIC_RESY;
    OX = (CW - DOOMGENERIC_RESX * S) / 2;
    OY = (CH - DOOMGENERIC_RESY * S) / 2;
    s_line = heap_caps_malloc(DOOMGENERIC_RESX * 4 * 3, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    memset(s_fb, 0, (size_t)PW * PH * 3);
    ESP_LOGI(TAG, "%s: %s, canvas %dx%d, Doom at %dx scale (%dx%d) offset %d,%d, rotation %d",
             s_p.name, s_p.fb2 ? "double-buffered" : "single buffer", CW, CH, S, DOOMGENERIC_RESX * S, DOOMGENERIC_RESY * S, OX, OY, ROT);

    controls_layout_t l = { .W = CW, .H = CH };
    controls_init(&l, s_p.touch, s_p.i2c, PW, PH, trot);
}

void DG_DrawFrame(void)
{
    controls_update();
    // Compose the whole frame in the hidden buffer: game, then the overlay on top.
    if (OX || OY) memset(s_fb, 0, (size_t)PW * PH * 3);   // clear borders (old overlay pixels)
    blit();
    controls_draw(plot);
    if (s_p.fb2) {
        // Flip: draw_bitmap on one of the panel's own buffers just writes back the
        // cache and makes it the scanned-out buffer from the next frame on.
        xSemaphoreTake(s_vsync, 0);
        esp_lcd_panel_draw_bitmap(s_p.panel, 0, 0, PW, PH, s_fb);
        xSemaphoreTake(s_vsync, pdMS_TO_TICKS(40));       // wait until the switch has happened
        uint8_t *t = s_front; s_front = s_fb; s_fb = t;
    } else {
        esp_cache_msync(s_fb, (size_t)PW * PH * 3, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    }
}

void DG_SleepMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1)); }
uint32_t DG_GetTicksMs(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
int DG_GetKey(int *pressed, unsigned char *key) { return controls_pop(pressed, key) ? 1 : 0; }
void DG_SetWindowTitle(const char *title) { ESP_LOGI(TAG, "%s", title); }

// ---------------------------------------------------------------------------
static void doom_task(void *arg)
{
    static char *argv[] = { "doom", "-iwad", "/wad/DOOM1.WAD", NULL };
    doomgeneric_Create(3, argv);
    extern int joybspeed;
    joybspeed = 29;          // the classic "always run" trick (>= MAX_JOY_BUTTONS)
    for (;;) doomgeneric_Tick();
}

void app_main(void)
{
    const esp_vfs_fat_mount_config_t mc = { .max_files = 4, .format_if_mount_failed = false };
    if (esp_vfs_fat_spiflash_mount_ro("/wad", "wad", &mc) != ESP_OK) {
        ESP_LOGE(TAG, "can't mount the WAD partition - was the full image flashed?");
        dg_exit(-3);
    }
    xTaskCreatePinnedToCore(doom_task, "doom", 64 * 1024, NULL, 5, NULL, 1);
}
