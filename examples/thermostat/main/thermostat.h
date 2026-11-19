#pragma once
#include <stdbool.h>

// Angles run clockwise from twelve o'clock. The lower gap separates the end stops.
enum {
    THERMOSTAT_LOW = 60, THERMOSTAT_HIGH = 84, THERMOSTAT_INITIAL = 72,
    THERMOSTAT_HALF_ARC = 144, DEMO_ROOM_F = 70, DEMO_HUMIDITY = 46
};

typedef struct {
    float position;
    int setpoint;
} thermostat_drag_t;

bool thermostat_begin(thermostat_drag_t *drag, float angle);
bool thermostat_move(thermostat_drag_t *drag, float delta);
