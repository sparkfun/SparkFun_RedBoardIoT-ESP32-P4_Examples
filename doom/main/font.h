// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SparkFun Electronics
// font.h - baked anti-aliased glyphs for the touch control labels (see tools/gen_font.py)
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t         w, h;          // cell size the glyphs were rendered for
    uint16_t        count;
    uint16_t        bytes_per_glyph;
    const uint16_t *codepoints;    // sorted
    const uint8_t  *bitmaps;       // count * bytes_per_glyph, 4bpp, high nibble first
} font_t;

extern const font_t font_10x20;

static inline const uint8_t *font_glyph(const font_t *f, uint16_t cp)
{
    int lo = 0, hi = (int)f->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        uint16_t v = f->codepoints[mid];
        if (v == cp) return f->bitmaps + (uint32_t)mid * f->bytes_per_glyph;
        if (v < cp) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}
