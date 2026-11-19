#pragma once

#include "display.h"
#include <stdbool.h>
#include <stddef.h>

enum {
    WATER_TILE = 128, WATER_SAMPLE_BYTES = 2, // Depth and underwater light.
    WATER_FIELD_BYTES = WATER_SAMPLE_BYTES*DISPLAY_SIZE*DISPLAY_SIZE,
    WATER_GLASS_BYTES = WATER_FIELD_BYTES + WATER_TILE*WATER_TILE
};

typedef struct {
    const uint8_t *glass;
    float down[3], velocity[3], previous_force[3];
    float ripple, phase;
    uint16_t inset[RENDER_SIZE];
    int8_t sine[256];
    uint8_t caustics[WATER_TILE*WATER_TILE];
    uint32_t color[2][256];
} water_t;

bool water_init(water_t *water, const uint8_t *glass, size_t bytes);
// Screen coordinates are right, down and toward the viewer, measured in g.
void water_enter(water_t *water, const float force[3]);
void water_advance(water_t *water, const float force[3], uint32_t elapsed_ms);
void water_present(const water_t *water);
void water_snapshot(const water_t *water, uint32_t *frame);
