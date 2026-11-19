#pragma once
#include <stdint.h>

// ARGB8888 from ThorVG is premultiplied. Both backgrounds beneath a part are opaque.
static inline uint32_t over(uint32_t src, uint32_t dst)
{
    unsigned alpha = src >> 24;
    if (alpha == 255) return src;
    if (!alpha) return dst;
    unsigned inv = 255 - alpha;
    unsigned rb = ((((dst & 0xff00ff) * inv) >> 8) & 0xff00ff);
    unsigned g = ((((dst & 0x00ff00) * inv) >> 8) & 0x00ff00);
    return 0xff000000u | ((src & 0xffffff) + rb + g);
}

static inline uint16_t pixel_rgb565(uint32_t argb)
{
    return ((argb >> 8) & 0xf800) | ((argb >> 5) & 0x07e0) | ((argb >> 3) & 0x001f);
}

static inline uint32_t pixel_from_rgb565(uint16_t color)
{
    unsigned r = color >> 11, g = (color >> 5) & 63, b = color & 31;
    return 0xff000000u | ((r*8 + r/4) << 16) | ((g*4 + g/16) << 8) | (b*8 + b/4);
}

static inline uint32_t pixel_mix(uint32_t a, uint32_t b, unsigned w)
{
    const uint32_t mask = 0x00ff00ff;
    uint32_t rb = (((a & mask)*(256-w) + (b & mask)*w) >> 8) & mask;
    uint32_t ag = ((((a>>8) & mask)*(256-w) + ((b>>8) & mask)*w) >> 8) & mask;
    return rb | (ag << 8);
}
