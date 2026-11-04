// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 SparkFun Electronics
// ws_dsi_panel.c - see header. Init commands/timings copied from
// waveshare/esp32_p4_platform (BSP_LCD_TYPE_480_800_4_3_INCH_A and
// BSP_LCD_TYPE_1024_600_7_INCH_C), Apache-2.0.
#include <string.h>
#include "ws_dsi_panel.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "ws_panel";
static int s_fbs = 1;
void ws_panel_set_fb_count(int n) { s_fbs = n == 2 ? 2 : 1; }

static const st7701_lcd_init_cmd_t st7701_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x17, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x40, 0xC9, 0x94, 0x0E, 0x10, 0x05, 0x0B, 0x09, 0x08, 0x26, 0x04, 0x52, 0x10, 0x69, 0x6B, 0x69}, 16, 0},
    {0xB1, (uint8_t[]){0x40, 0xD2, 0x98, 0x0C, 0x92, 0x07, 0x09, 0x08, 0x07, 0x25, 0x02, 0x0E, 0x0C, 0x6E, 0x78, 0x55}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x4E}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

static esp_err_t mcu_write(ws_panel_t *p, uint8_t reg, uint8_t val)
{
    i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x45, .scl_speed_hz = 100000 };
    i2c_master_dev_handle_t dev;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(p->i2c, &dc, &dev), TAG, "add 0x45");
    uint8_t b[2] = { reg, val };
    esp_err_t e = i2c_master_transmit(dev, b, 2, 50);
    i2c_master_bus_rm_device(dev);
    return e;
}

esp_err_t ws_panel_backlight(ws_panel_t *p, uint8_t level)
{
    return mcu_write(p, 0x96, level);
}

static uint8_t find_gt911(ws_panel_t *p)
{
    if (i2c_master_probe(p->i2c, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 50) == ESP_OK) return ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
    if (i2c_master_probe(p->i2c, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 50) == ESP_OK) return ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    return 0;
}

// Read the GT911's configured output resolution (0x8048: X lo/hi, Y lo/hi)
static bool read_gt911_res(ws_panel_t *p, uint8_t addr, uint16_t *x, uint16_t *y)
{
    i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr, .scl_speed_hz = 100000 };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(p->i2c, &dc, &dev) != ESP_OK) return false;
    uint8_t reg[2] = { 0x80, 0x48 }, r[4] = { 0 };
    esp_err_t e = i2c_master_transmit_receive(dev, reg, 2, r, 4, 100);
    uint8_t pid_reg[2] = { 0x81, 0x40 }, pid[5] = { 0 };
    i2c_master_transmit_receive(dev, pid_reg, 2, pid, 4, 100);
    i2c_master_bus_rm_device(dev);
    if (e != ESP_OK) return false;
    *x = r[0] | (r[1] << 8);
    *y = r[2] | (r[3] << 8);
    ESP_LOGI(TAG, "GT911 @0x%02X product '%s', configured %ux%u", addr, (char *)pid, *x, *y);
    return true;
}

static ws_panel_type_t type_from_res(uint16_t x, uint16_t y)
{
    uint16_t a = x < y ? x : y, b = x < y ? y : x;   // short side, long side
    if (a == 480 && b == 800) return WS_PANEL_4_3_A;
    if (a == 600 && b == 1024) return WS_PANEL_7_C;
    return WS_PANEL_UNKNOWN;
}

esp_err_t ws_panel_start(ws_panel_t *p)
{
    memset(p, 0, sizeof(*p));

    i2c_master_bus_config_t bc = {
        .i2c_port = -1, .sda_io_num = CONFIG_WS_PANEL_I2C_SDA, .scl_io_num = CONFIG_WS_PANEL_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bc, &p->i2c), TAG, "i2c bus");

    // 1. panel MCU: power + release resets, backlight off->full (Waveshare BSP order)
    p->mcu_found = i2c_master_probe(p->i2c, 0x45, 100) == ESP_OK;
    if (!p->mcu_found) ESP_LOGW(TAG, "panel MCU 0x45 not answering - check 5V and the FPC");
    mcu_write(p, 0x95, 0x11);
    mcu_write(p, 0x95, 0x17);
    mcu_write(p, 0x96, 0x00);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 2. DSI PHY power
#if CONFIG_WS_PANEL_LDO_CHAN >= 0
    esp_ldo_channel_handle_t ldo;
    esp_ldo_channel_config_t lc = { .chan_id = CONFIG_WS_PANEL_LDO_CHAN, .voltage_mv = 2500 };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&lc, &ldo), TAG, "LDO");
#endif

    // 3. which panel is this?
    vTaskDelay(pdMS_TO_TICKS(150));          // GT911 boots after the MCU releases reset
    uint8_t taddr = find_gt911(p);
#if CONFIG_WS_PANEL_FORCE_7_C
    p->type = WS_PANEL_7_C;
#elif CONFIG_WS_PANEL_FORCE_4_3_A
    p->type = WS_PANEL_4_3_A;
#else
    if (taddr && read_gt911_res(p, taddr, &p->gt_x, &p->gt_y)) p->type = type_from_res(p->gt_x, p->gt_y);
    p->detected = p->type != WS_PANEL_UNKNOWN;
    if (!p->detected) {
        ESP_LOGW(TAG, "could not identify the panel (GT911 %s, %ux%u) - assuming 4.3-DSI-TOUCH-A. "
                      "Force it in menuconfig -> Waveshare DSI panel if that's wrong.",
                 taddr ? "found" : "missing", p->gt_x, p->gt_y);
        p->type = WS_PANEL_4_3_A;
    }
#endif
    const bool seven = p->type == WS_PANEL_7_C;
    p->name   = seven ? "Waveshare 7-DSI-TOUCH-C" : "Waveshare 4.3-DSI-TOUCH-A";
    p->width  = seven ? 1024 : 480;
    p->height = seven ? 600 : 800;
    ESP_LOGI(TAG, "panel: %s (%s)", p->name, p->detected ? "auto-detected" : "forced/fallback");

    // 4. DSI bus, 2 lanes
    esp_lcd_dsi_bus_handle_t bus;
    esp_lcd_dsi_bus_config_t dbc = { .bus_id = 0, .num_data_lanes = 2, .phy_clk_src = 0,
                                     .lane_bit_rate_mbps = seven ? 1000 : 500 };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&dbc, &bus), TAG, "dsi bus");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_dbi_io_config_t dio = { .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8 };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(bus, &dio, &io), TAG, "dbi io");

    // 5. controller + DPI video
    esp_lcd_panel_dev_config_t pc = {
        .bits_per_pixel = 24,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .reset_gpio_num = -1,
    };
    if (seven) {
        static esp_lcd_dpi_panel_config_t dpi = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB888);
        dpi.num_fbs = s_fbs;
        static ek79007_vendor_config_t vc;
        vc.mipi_config.dsi_bus = bus;
        vc.mipi_config.dpi_config = &dpi;
        vc.mipi_config.lane_num = 2;
        pc.vendor_config = &vc;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ek79007(io, &pc, &p->panel), TAG, "ek79007");
    } else {
        static esp_lcd_dpi_panel_config_t dpi = {
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = 30,
            .virtual_channel = 0,
            .in_color_format = LCD_COLOR_FMT_RGB888,
            .num_fbs = 1,
            .video_timing = {
                .h_size = 480, .v_size = 800,
                .hsync_back_porch = 42, .hsync_pulse_width = 12, .hsync_front_porch = 42,
                .vsync_back_porch = 2, .vsync_pulse_width = 8, .vsync_front_porch = 60,
            },
        };
        dpi.num_fbs = s_fbs;
        static st7701_vendor_config_t vc;
        vc.init_cmds = st7701_init_cmds;
        vc.init_cmds_size = sizeof(st7701_init_cmds) / sizeof(st7701_init_cmds[0]);
        vc.mipi_config.dsi_bus = bus;
        vc.mipi_config.dpi_config = &dpi;
        vc.flags.use_mipi_interface = 1;
        pc.vendor_config = &vc;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(io, &pc, &p->panel), TAG, "st7701");
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(p->panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(p->panel), TAG, "init");
    // The ST7701 driver implements display-on; the EK79007 driver doesn't (its init
    // already ends with sleep-out and Waveshare's BSP never calls it), so
    // ESP_ERR_NOT_SUPPORTED here is expected on the 7" and must not abort.
    esp_err_t on = esp_lcd_panel_disp_on_off(p->panel, true);
    if (on != ESP_OK && on != ESP_ERR_NOT_SUPPORTED) ESP_RETURN_ON_ERROR(on, TAG, "disp on");
    if (s_fbs == 2) {
        ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(p->panel, 2, (void **)&p->fb, (void **)&p->fb2), TAG, "fb");
        memset(p->fb2, 0, (size_t)p->width * p->height * 3);
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(p->panel, 1, (void **)&p->fb), TAG, "fb");
    }
    memset(p->fb, 0, (size_t)p->width * p->height * 3);

    mcu_write(p, 0x96, 0xFF);   // backlight full

    // 6. GT911 touch
    if (!taddr) taddr = find_gt911(p);
    if (taddr) {
        esp_lcd_panel_io_handle_t tio;
        esp_lcd_panel_io_i2c_config_t tc = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        tc.dev_addr = taddr;
        tc.scl_speed_hz = 400000;
        esp_lcd_touch_config_t tcfg = {
            .x_max = p->width, .y_max = p->height,
            .rst_gpio_num = -1, .int_gpio_num = -1,
        };
        if (esp_lcd_new_panel_io_i2c(p->i2c, &tc, &tio) == ESP_OK &&
            esp_lcd_touch_new_i2c_gt911(tio, &tcfg, &p->touch) == ESP_OK) {
            p->touch_addr = taddr;
        }
    }
    ESP_LOGI(TAG, "%s up: %dx%d fb=%p, MCU %s, touch %s (0x%02X)", p->name, p->width, p->height, p->fb,
             p->mcu_found ? "ok" : "MISSING", p->touch ? "ok" : "not found", p->touch_addr);
    return ESP_OK;
}
