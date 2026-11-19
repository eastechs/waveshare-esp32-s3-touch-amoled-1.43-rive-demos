#pragma once
#include "rive_thorvg.h"
#include "thermostat.h"

typedef struct {
    uint32_t pixel;
    uint16_t offset;
} thermostat_ink_t;

typedef struct {
    uint32_t *cold, *warm;
    struct {
        thermostat_ink_t *ink;
        size_t count;
    } settings[THERMOSTAT_HIGH - THERMOSTAT_LOW + 1];
} thermostat_view_t;

// Bake the authored layers at startup. The scratch buffer belongs to the Rive instance.
bool thermostat_bake(thermostat_view_t *view, rive_instance_t *scene, uint32_t *scratch);
void thermostat_paint(const thermostat_view_t *view, int temperature, uint32_t *frame);
void thermostat_free(thermostat_view_t *view);
