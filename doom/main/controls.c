// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SparkFun Electronics
// controls.c - touch overlay + Qwiic Joystick for Doom
//
// Touch (multi-touch, GT911 reports up to 5 points):
//   left thumb : virtual stick  - up/down = walk, left/right = turn
//   FIRE       : shoot (hold for continuous fire); also ENTER in menus
//   USE        : open doors / switches; also 'y' to confirm prompts
//   WPN        : cycle weapons 1..7
//   MAP        : automap (TAB)
//   MENU       : ESC (Doom's menu)
//
// SparkFun Qwiic Joystick (COM-15168, I2C 0x20) if plugged in:
//   stick = walk/turn, click = FIRE. Registers 0x03/0x04 = X (10-bit, MSB/LSB<<6),
//   0x05/0x06 = Y, 0x07 = current button (0 = pressed).
#include "controls.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "doomkeys.h"
#include "font.h"
#include "sdkconfig.h"

static const char *TAG = "controls";

typedef struct {
    const char *label;
    float fx, fy, fr;          // position / radius as fractions of canvas H (x as fraction of W)
    int   key, key2;           // keys sent while held (key2 optional)
    int   cx, cy, r;           // resolved canvas pixels
    bool  down, was_down;
    int64_t down_since;
} button_t;

enum { B_FIRE, B_USE, B_WPN, B_MAP, B_MENU, B_COUNT };
static button_t btn[B_COUNT] = {
    [B_FIRE] = { "FIRE", 0.87f, 0.66f, 0.14f, KEY_FIRE, KEY_ENTER },
    [B_USE]  = { "USE",  0.71f, 0.74f, 0.09f, KEY_USE, 'y' },
    [B_WPN]  = { "WPN",  0.93f, 0.40f, 0.075f, 0, 0 },
    [B_MAP]  = { "MAP",  0.17f, 0.11f, 0.065f, KEY_TAB, 0 },
    [B_MENU] = { "MENU", 0.07f, 0.11f, 0.065f, KEY_ESCAPE, 0 },
};

// virtual stick
static int  st_cx, st_cy, st_r;
static bool st_active;
static float st_dx, st_dy;     // -1..1

static int W, H, PW, PH, TROT;
static esp_lcd_touch_handle_t s_tp;

// Qwiic Joystick
static i2c_master_dev_handle_t s_joy;
static bool  joy_ok;
static float joy_x, joy_y;     // -1..1, +y = forward
static bool  joy_btn;

// key state + event queue
#define NKEYS 256
static uint8_t key_now[NKEYS], key_was[NKEYS];
static struct { uint8_t pressed, key; } q[64];
static int q_head, q_tail;
static int weapon = 1, pulse_key;

static void push(int pressed, int key)
{
    int n = (q_head + 1) % 64;
    if (n == q_tail) return;
    q[q_head].pressed = (uint8_t)pressed;
    q[q_head].key = (uint8_t)key;
    q_head = n;
}

bool controls_pop(int *pressed, unsigned char *key)
{
    if (q_tail == q_head) return false;
    *pressed = q[q_tail].pressed;
    *key = q[q_tail].key;
    q_tail = (q_tail + 1) % 64;
    return true;
}

bool controls_joystick_present(void) { return joy_ok; }

static int joy_reg(uint8_t reg)
{
    uint8_t v = 0;
    if (i2c_master_transmit_receive(s_joy, &reg, 1, &v, 1, 20) != ESP_OK) return -1;
    return v;
}

void controls_init(const controls_layout_t *l, esp_lcd_touch_handle_t touch, i2c_master_bus_handle_t i2c,
                   int panel_w, int panel_h, int touch_rotation)
{
    W = l->W; H = l->H; PW = panel_w; PH = panel_h; TROT = touch_rotation;
    s_tp = touch;
    for (int i = 0; i < B_COUNT; i++) {
        btn[i].cx = (int)(btn[i].fx * W);
        btn[i].cy = (int)(btn[i].fy * H);
        btn[i].r  = (int)(btn[i].fr * H);
    }
    st_cx = (int)(0.15f * W);
    st_cy = (int)(0.70f * H);
    st_r  = (int)(0.17f * H);

    if (i2c && i2c_master_probe(i2c, 0x20, 50) == ESP_OK) {
        i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x20, .scl_speed_hz = 100000 };
        if (i2c_master_bus_add_device(i2c, &dc, &s_joy) == ESP_OK) {
            joy_ok = true;
            int x = (joy_reg(0x03) << 8 | joy_reg(0x04)) >> 6, y = (joy_reg(0x05) << 8 | joy_reg(0x06)) >> 6;
            ESP_LOGI(TAG, "Qwiic Joystick found at 0x20 (rest: x=%d y=%d, button reg=%d)", x, y, joy_reg(0x07));
        }
    }
    if (!joy_ok) ESP_LOGI(TAG, "no Qwiic Joystick at 0x20 - touch controls only");
}

static void read_joystick(void)
{
    if (!joy_ok) return;
    int xm = joy_reg(0x03), xl = joy_reg(0x04), ym = joy_reg(0x05), yl = joy_reg(0x06), b = joy_reg(0x07);
    if (xm < 0 || xl < 0 || ym < 0 || yl < 0 || b < 0) { joy_x = joy_y = 0; joy_btn = false; return; }
    float x = ((((xm << 8) | xl) >> 6) - 512) / 512.f;
    float y = (512 - (((ym << 8) | yl) >> 6)) / 512.f;   // pushing the stick away from you = forward
#if CONFIG_DOOM_JOY_INVERT_X
    x = -x;
#endif
#if CONFIG_DOOM_JOY_INVERT_Y
    y = -y;
#endif
    joy_x = x;
    joy_y = y;
#if CONFIG_DOOM_JOY_BUTTON_ACTIVE_HIGH
    joy_btn = b != 0;
#else
    joy_btn = b == 0;
#endif
}

static void touch_to_canvas(int px, int py, int *cx, int *cy)
{
    switch (TROT) {
    case 90:  *cx = py;          *cy = PW - 1 - px; break;
    case 180: *cx = PW - 1 - px; *cy = PH - 1 - py; break;
    case 270: *cx = PH - 1 - py; *cy = px;          break;
    default:  *cx = px;          *cy = py;          break;
    }
}

void controls_update(void)
{
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < B_COUNT; i++) { btn[i].was_down = btn[i].down; btn[i].down = false; }
    st_active = false;
    st_dx = st_dy = 0;

    // ---- touch ----
    // The GT911 only has fresh data ~every 10 ms; a read in between comes back with
    // zero points, which looked like the finger lifting and made buttons flicker.
    // So keep the last points until the panel has reported "no touch" for 80 ms.
    static esp_lcd_touch_point_data_t last[5];
    static uint8_t last_n;
    static int64_t last_seen;
    if (s_tp && esp_lcd_touch_read_data(s_tp) == ESP_OK) {
        esp_lcd_touch_point_data_t pts[5];
        uint8_t n = 0;
        if (esp_lcd_touch_get_data(s_tp, pts, &n, 5) != ESP_OK) n = 0;
        if (n) { memcpy(last, pts, sizeof(pts[0]) * n); last_n = n; last_seen = now; }
        else if (now - last_seen < 80000) { memcpy(pts, last, sizeof(pts[0]) * last_n); n = last_n; }
        for (int k = 0; k < n; k++) {
            int cx, cy;
            touch_to_canvas(pts[k].x, pts[k].y, &cx, &cy);
            bool used = false;
            for (int i = 0; i < B_COUNT && !used; i++) {
                int dx = cx - btn[i].cx, dy = cy - btn[i].cy, rr = btn[i].r * 5 / 4;
                if (dx * dx + dy * dy <= rr * rr) { btn[i].down = true; used = true; }
            }
            if (!used && !st_active && cx < W * 45 / 100 && cy > H * 25 / 100) {
                st_active = true;
                float dx = (float)(cx - st_cx) / st_r, dy = (float)(cy - st_cy) / st_r;
                float m = sqrtf(dx * dx + dy * dy);
                if (m > 1.f) { dx /= m; dy /= m; }
                st_dx = dx; st_dy = dy;
            }
        }
    }
    read_joystick();

    // ---- desired key state ----
    memset(key_now, 0, sizeof key_now);
    const float T = 0.30f;
    float fwd = -st_dy, turn = st_dx;
    if (joy_ok && (fabsf(joy_x) > T || fabsf(joy_y) > T)) { fwd = joy_y; turn = joy_x; }
    if (fwd > T)   key_now[KEY_UPARROW] = 1;
    if (fwd < -T)  key_now[KEY_DOWNARROW] = 1;
    if (turn > T)  key_now[KEY_RIGHTARROW] = 1;
    if (turn < -T) key_now[KEY_LEFTARROW] = 1;
    if (joy_btn) btn[B_FIRE].down = true;

    for (int i = 0; i < B_COUNT; i++) {
        if (btn[i].down && !btn[i].was_down) btn[i].down_since = now;
        if (btn[i].down) {
            if (btn[i].key)  key_now[btn[i].key] = 1;
            if (btn[i].key2) key_now[btn[i].key2] = 1;
        }
    }
    // WPN: one short pulse of the next weapon number per press
    if (pulse_key) { push(0, pulse_key); pulse_key = 0; }
    if (btn[B_WPN].down && !btn[B_WPN].was_down) {
        weapon = weapon % 7 + 1;
        pulse_key = '0' + weapon;
        push(1, pulse_key);
    }

    for (int k = 1; k < NKEYS; k++) {
        if (key_now[k] != key_was[k]) push(key_now[k], k);
        key_was[k] = key_now[k];
    }
}

// ---------------------------------------------------------------------------
// overlay drawing
// ---------------------------------------------------------------------------
static void ring(plot_fn plot, int cx, int cy, int r, int thick, uint32_t c, uint8_t a)
{
    int r2o = r * r, r2i = (r - thick) * (r - thick);
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r2o && d >= r2i) plot(cx + x, cy + y, c, a);
        }
}

static void disc(plot_fn plot, int cx, int cy, int r, uint32_t c, uint8_t a)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) plot(cx + x, cy + y, c, a);
}

static void text(plot_fn plot, int cx, int cy, const char *s, uint32_t c, uint8_t alpha)
{
    const font_t *f = &font_10x20;
    int n = (int)strlen(s);
    int x0 = cx - n * f->w / 2, y0 = cy - f->h / 2;
    for (int i = 0; i < n; i++) {
        const uint8_t *g = font_glyph(f, (uint8_t)s[i]);
        if (!g) continue;
        for (int y = 0; y < f->h; y++)
            for (int x = 0; x < f->w; x++) {
                int idx = y * f->w + x;
                uint8_t b = g[idx >> 1];
                uint8_t a = (idx & 1) ? (b & 15) : (b >> 4);
                if (a) plot(x0 + i * f->w + x, y0 + y, c, (uint8_t)(a * 17 * alpha / 255));
            }
    }
}

void controls_draw(plot_fn plot)
{
    // Outline-only, mostly transparent so the game stays visible underneath.
    const uint32_t ink = 0xFFF1DC, hot = 0xFF9A3C;
    ring(plot, st_cx, st_cy, st_r, 2, ink, 70);
    int kx = st_cx + (int)(st_dx * st_r * 0.6f), ky = st_cy + (int)(st_dy * st_r * 0.6f);
    ring(plot, kx, ky, st_r / 3, 2, st_active ? hot : ink, st_active ? 160 : 70);
    for (int i = 0; i < B_COUNT; i++) {
        button_t *b = &btn[i];
        if (b->down) disc(plot, b->cx, b->cy, b->r, hot, 45);
        ring(plot, b->cx, b->cy, b->r, 2, b->down ? hot : ink, b->down ? 180 : 80);
        text(plot, b->cx, b->cy, b->label, b->down ? 0xFFFFFF : ink, b->down ? 230 : 110);
    }
}
