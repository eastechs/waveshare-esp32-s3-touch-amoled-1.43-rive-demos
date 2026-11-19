#include "vu_meter.h"
#include "panel_blit.h"
#include <string.h>

enum {
    PATCH_PIXELS = VU_METER_PATCH_WIDTH*VU_METER_PATCH_HEIGHT,
    BASE_BYTES = 2*DISPLAY_SIZE*DISPLAY_SIZE,
    TOP_BYTES = 2*DISPLAY_SIZE*VU_METER_PATCH_Y,
    BAND_BOTTOM = VU_METER_PATCH_Y+VU_METER_PATCH_HEIGHT,
    POSES = VU_METER_FPS*VU_METER_SECONDS,
    PERIOD_MS = 1000*VU_METER_SECONDS,
    INDEX_BYTES = 4*(POSES+1),
    PATCH_BYTES = 4,
};
_Static_assert(PATCH_PIXELS <= UINT16_MAX, "VU meter pixel offsets must fit in 16 bits");
_Static_assert((VU_METER_PATCH_Y | VU_METER_PATCH_HEIGHT) % 2 == 0,
               "VU meter band must align with the swipe buffer");
_Static_assert(VU_METER_PATCH_X + VU_METER_PATCH_WIDTH <= DISPLAY_SIZE && (int)BAND_BOTTOM < DISPLAY_SIZE,
               "VU meter patches must fit the panel");

static uint16_t read16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t pose_offset(const uint8_t *art, unsigned pose)
{
    const uint8_t *p = art+BASE_BYTES+4*pose;
    return read16(p) | ((uint32_t)read16(p+2) << 16);
}

bool vu_meter_valid(const uint8_t *art, size_t size)
{
    if (size < BASE_BYTES+INDEX_BYTES || pose_offset(art, 0) != 0) return false;
    size_t length = size-BASE_BYTES-INDEX_BYTES;
    if (!length || pose_offset(art, POSES) != length) return false;
    const uint8_t *patches = art+BASE_BYTES+INDEX_BYTES;
    uint32_t previous = 0;
    for (unsigned pose = 1; pose <= POSES; ++pose) {
        uint32_t offset = pose_offset(art, pose);
        if (offset < previous || offset > length || offset % PATCH_BYTES) return false;
        for (uint32_t i = previous; i < offset; i += PATCH_BYTES)
            if (read16(patches+i) >= PATCH_PIXELS) return false;
        previous = offset;
    }
    return true;
}

static void apply_pose(vu_meter_t *meter, unsigned pose, bool restore)
{
    const uint8_t *art = meter->art;
    const uint8_t *patches = art+BASE_BYTES+INDEX_BYTES;
    const uint8_t *end = patches+pose_offset(art, pose+1);
    for (const uint8_t *patch = patches+pose_offset(art, pose); patch != end; patch += PATCH_BYTES) {
        unsigned pixel = read16(patch);
        unsigned y = pixel/VU_METER_PATCH_WIDTH;
        unsigned x = VU_METER_PATCH_X + pixel%VU_METER_PATCH_WIDTH;
        unsigned byte = 2*(y*DISPLAY_SIZE+x);
        const uint8_t *color = restore ? art+TOP_BYTES+byte : patch+2;
        memcpy(meter->band+byte, color, 2);
    }
}

void vu_meter_init(vu_meter_t *meter, const uint8_t *art, uint8_t *band)
{
    *meter = (vu_meter_t){.art = art, .band = band};
    memcpy(band, art+TOP_BYTES, VU_METER_BAND_BYTES);
    apply_pose(meter, 0, false);
}

void vu_meter_enter(const vu_meter_t *meter, uint32_t *swipe, bool on_panel)
{
    panel_blit(meter->art, 0, 0, DISPLAY_SIZE, VU_METER_PATCH_Y, swipe, on_panel);
    panel_blit(meter->band, 0, VU_METER_PATCH_Y, DISPLAY_SIZE, VU_METER_PATCH_HEIGHT, swipe, on_panel);
    panel_blit(meter->art+TOP_BYTES+VU_METER_BAND_BYTES, 0, BAND_BOTTOM,
               DISPLAY_SIZE, DISPLAY_SIZE-BAND_BOTTOM, swipe, on_panel);
}

void vu_meter_advance(vu_meter_t *meter, uint32_t delta_ms, uint32_t *swipe, bool on_panel)
{
    meter->elapsed_ms = (meter->elapsed_ms + delta_ms%PERIOD_MS)%PERIOD_MS;
    unsigned pose = meter->elapsed_ms*VU_METER_FPS/1000;
    if (pose == meter->pose) return;
    const uint8_t *art = meter->art;
    bool was_at_rest = pose_offset(art, meter->pose) == pose_offset(art, meter->pose+1);
    bool is_at_rest = pose_offset(art, pose) == pose_offset(art, pose+1);
    if (!was_at_rest) apply_pose(meter, meter->pose, true);
    if (!is_at_rest) apply_pose(meter, pose, false);
    meter->pose = pose;
    if (!was_at_rest || !is_at_rest)
        panel_blit(meter->band, 0, VU_METER_PATCH_Y, DISPLAY_SIZE, VU_METER_PATCH_HEIGHT, swipe, on_panel);
}
