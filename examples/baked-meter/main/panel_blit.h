#pragma once
#include "display.h"
#include "pixel.h"
#include <stdbool.h>

// Optionally mirror each even-aligned native patch into the half-resolution swipe frame.
static inline void panel_blit(const uint8_t *source, int x, int y, int w, int h, uint32_t *swipe, bool on_panel)
{
    for (int row = 0; swipe && row < h; row += 2) {
        uint32_t *dst = swipe + (y+row)/2*RENDER_SIZE + x/2;
        const uint8_t *src = source + 2*row*w;
        for (int col = 0; col < w/2; ++col, src += 4) {
            dst[col] = pixel_from_rgb565(src[0] | (src[1] << 8));
        }
    }
    if (on_panel) display_rgb565_region(x, y, w, h, source);
}
