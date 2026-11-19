#pragma once
#include "display.h"
#include "pixel.h"
#include <assert.h>

static uint16_t panel[DISPLAY_SIZE][DISPLAY_SIZE];
static uint32_t swipe[RENDER_SIZE*RENDER_SIZE];
static unsigned transfers;

void display_rgb565_region(int x, int y, int w, int h, const uint8_t *pixels)
{
    assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x+w <= DISPLAY_SIZE && y+h <= DISPLAY_SIZE);
    assert((x | y | w | h) % 2 == 0);
    for (int row = y; row < y+h; ++row) {
        for (int col = x; col < x+w; ++col, pixels += 2)
            panel[row][col] = pixels[0] | (pixels[1] << 8);
    }
    ++transfers;
}

static void assert_swipe_matches_panel(void)
{
    for (int y = 0; y < RENDER_SIZE; ++y)
        for (int x = 0; x < RENDER_SIZE; ++x)
            assert(pixel_rgb565(swipe[y*RENDER_SIZE+x]) == panel[2*y][2*x]);
}

static int last_scan_width;

void display_scanlines(int x, int y, int width, int height, display_scanline_fn paint, void *context)
{
    assert(x >= 0 && y >= 0 && x+width <= DISPLAY_SIZE && y+height <= DISPLAY_SIZE);
    uint16_t line[DISPLAY_SIZE];
    for (int row = y; row < y+height; ++row) {
        paint(context, x, row, width, line);
        for (int i = 0; i < width; ++i)
            panel[DISPLAY_SIZE-1-x-i][row] = __builtin_bswap16(line[i]);
    }
    last_scan_width = width;
    ++transfers;
}
