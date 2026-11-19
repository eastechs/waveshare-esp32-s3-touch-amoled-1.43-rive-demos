#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DISPLAY_SIZE = 466, RENDER_SIZE = DISPLAY_SIZE / 2 };

// Initialize once. Call display functions from one owning task.
void display_init(void);
void display_set_brightness(uint8_t level);
void display_frame(const uint32_t *pixels);
// Update a source rectangle of a full half-resolution frame (same rotation and 2x scale).
void display_frame_region(const uint32_t *pixels, int x, int y, int width, int height);
// Draw a native-resolution patch from tightly packed little-endian RGB565.
void display_rgb565_region(int x, int y, int width, int height, const uint8_t *pixels);

// Fill panel-coordinate scanlines directly in alternating DMA buffers, in wire byte order.
typedef void (*display_scanline_fn)(void *context, int x, int y, int width, uint16_t *out);
void display_scanlines(int x, int y, int width, int height, display_scanline_fn paint, void *context);

// Paint independent groups of complete panel rows on the two cores. Groups start
// and end on an even row so a shader can render 2x2 blocks directly into DMA.
typedef void (*display_rows_fn)(const void *context, int y, int count, uint16_t *out);
void display_parallel_rows(display_rows_fn paint, const void *context);

#ifdef __cplusplus
}
#endif
