#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const uint32_t *dial, *lid, *tray;
    bool opening;
    float start_position;
    uint64_t changed_ms;
    int light_x, light_y; // Quantized screen tilt for the brass reflection.
} watch_t;

void watch_init(watch_t *watch, const uint32_t *dial, const uint32_t *lid, const uint32_t *tray);
bool watch_set_tilt(watch_t *watch, float x, float y);
void watch_toggle(watch_t *watch, uint64_t now_ms);
bool watch_is_open(const watch_t *watch, uint64_t now_ms);
bool watch_is_closed(const watch_t *watch, uint64_t now_ms);
void watch_paint(const watch_t *watch, uint32_t *frame, uint64_t uptime_ms, uint32_t local_ms);
