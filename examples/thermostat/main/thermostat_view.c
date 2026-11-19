#include "thermostat_view.h"
#include "display.h"
#include "art_buffer.h"
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

enum { PIXELS = RENDER_SIZE*RENDER_SIZE, STEPS = THERMOSTAT_HIGH-THERMOSTAT_LOW };
_Static_assert(PIXELS <= UINT16_MAX, "Indicator offsets must fit the framebuffer");

void thermostat_free(thermostat_view_t *view)
{
    free(view->cold);
    free(view->warm);
    for (int i = 0; i <= STEPS; ++i) free(view->settings[i].ink);
    *view = (thermostat_view_t){0};
}

bool thermostat_bake(thermostat_view_t *view, rive_instance_t *scene, uint32_t *scratch)
{
    view->cold = art_buffer(PIXELS*sizeof(*scratch));
    view->warm = art_buffer(PIXELS*sizeof(*scratch));
    if (!view->cold || !view->warm) goto fail;
    rive_vm_set_number(scene, "controlsOpacity", 0);
    rive_vm_set_number(scene, "setpoint", THERMOSTAT_LOW);
    if (!rive_advance(scene, 0)) goto fail;
    memcpy(view->cold, scratch, PIXELS*sizeof(*scratch));
    rive_vm_set_number(scene, "setpoint", THERMOSTAT_HIGH);
    if (!rive_advance(scene, 0)) goto fail;
    memcpy(view->warm, scratch, PIXELS*sizeof(*scratch));

    // The moving index, two digits and operating mode occupy only a small part of each frame.
    rive_vm_set_number(scene, "surfaceOpacity", 0);
    rive_vm_set_number(scene, "controlsOpacity", 1);
    for (int i = 0; i <= STEPS; ++i) {
        rive_vm_set_number(scene, "setpoint", THERMOSTAT_LOW+i);
        if (!rive_advance(scene, 0)) goto fail;
        size_t count = 0;
        for (int p = 0; p < PIXELS; ++p) count += (scratch[p] >> 24) != 0;
        if (count == 0) goto fail;
        thermostat_ink_t *ink = art_buffer(count*sizeof(*ink));
        if (!ink) goto fail;
        view->settings[i].ink = ink;
        view->settings[i].count = count;
        for (int p = 0; p < PIXELS; ++p) {
            if (scratch[p] >> 24) *ink++ = (thermostat_ink_t){scratch[p], (uint16_t)p};
        }
#ifdef ESP_PLATFORM
        vTaskDelay(1);
#endif
    }
    return true;
fail:
    thermostat_free(view);
    return false;
}

static uint32_t blend_pair(uint32_t a, uint32_t b, unsigned weight)
{
    const uint32_t mask = 0x00ff00ff;
    return (((a & mask)*(256-weight) + (b & mask)*weight) >> 8) & mask;
}

void thermostat_paint(const thermostat_view_t *view, int temperature, uint32_t *frame)
{
    int i = temperature-THERMOSTAT_LOW;
    if (i < 0) i = 0;
    if (i > STEPS) i = STEPS;
    unsigned weight = (256*i + STEPS/2)/STEPS;
    for (int p = 0; p < PIXELS; ++p) {
        uint32_t a = view->cold[p], b = view->warm[p];
        frame[p] = blend_pair(a,b,weight) | (blend_pair(a>>8,b>>8,weight)<<8);
    }
    for (size_t k = 0; k < view->settings[i].count; ++k) {
        thermostat_ink_t ink = view->settings[i].ink[k];
        unsigned alpha = ink.pixel >> 24;
        if (alpha == 255) {
            frame[ink.offset] = ink.pixel;
            continue;
        }
        uint32_t dst = frame[ink.offset];
        // ThorVG's raster target already premultiplies the foreground channels.
        unsigned remaining = 256-alpha;
        uint32_t under = blend_pair(0,dst,remaining) | (blend_pair(0,dst>>8,remaining)<<8);
        frame[ink.offset] = ink.pixel + under;
    }
}
