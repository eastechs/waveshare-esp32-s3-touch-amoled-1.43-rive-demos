#pragma once
#include "display.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { VU_METER_FPS = 25, VU_METER_SECONDS = 8 };
enum {
    VU_METER_PATCH_X = 76, VU_METER_PATCH_Y = 134,
    VU_METER_PATCH_WIDTH = 314, VU_METER_PATCH_HEIGHT = 190,
    VU_METER_BAND_BYTES = 2*DISPLAY_SIZE*VU_METER_PATCH_HEIGHT,
};

typedef struct {
    const uint8_t *art;
    uint8_t *band;
    uint32_t elapsed_ms;
    unsigned pose;
} vu_meter_t;

bool vu_meter_valid(const uint8_t *art, size_t size);
void vu_meter_init(vu_meter_t *meter, const uint8_t *art, uint8_t *band);
void vu_meter_enter(const vu_meter_t *meter, uint32_t *swipe, bool on_panel);
void vu_meter_advance(vu_meter_t *meter, uint32_t delta_ms, uint32_t *swipe, bool on_panel);
