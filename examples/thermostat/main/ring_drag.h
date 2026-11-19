#pragma once
#include "display.h"
#include <math.h>
#include <stdbool.h>

typedef struct {
    bool on_ring;
    float angle;
} ring_drag_t;

static inline int ring_radius_squared(int x, int y)
{
    int dx = x-DISPLAY_SIZE/2, dy = y-DISPLAY_SIZE/2;
    return dx*dx + dy*dy;
}

static inline float ring_angle(int x, int y)
{
    return atan2f(x-DISPLAY_SIZE*.5f, DISPLAY_SIZE*.5f-y) * 57.2957795f;
}

static inline bool ring_begin(ring_drag_t *ring, int x, int y)
{
    int r2 = ring_radius_squared(x, y);
    if (r2 < 177*177 || r2 > (DISPLAY_SIZE/2+2)*(DISPLAY_SIZE/2+2)) return false;
    ring->on_ring = true;
    ring->angle = ring_angle(x,y);
    return true;
}

static inline float ring_turn_above(ring_drag_t *ring, int x, int y, int min_radius)
{
    if (ring_radius_squared(x,y) < min_radius*min_radius) {
        ring->on_ring = false;
        return 0;
    }
    float angle = ring_angle(x,y);
    float delta = ring->on_ring ? remainderf(angle-ring->angle, 360) : 0;
    ring->angle = angle;
    ring->on_ring = true;
    return delta;
}

static inline float ring_turn(ring_drag_t *ring, int x, int y)
{
    return ring_turn_above(ring, x, y, 146);
}
