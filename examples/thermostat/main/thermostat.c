#include "thermostat.h"
#include <math.h>

static int target_at(float position)
{
    return THERMOSTAT_LOW + lroundf((position+THERMOSTAT_HALF_ARC) *
           (THERMOSTAT_HIGH-THERMOSTAT_LOW)/(2*THERMOSTAT_HALF_ARC));
}

bool thermostat_begin(thermostat_drag_t *drag, float angle)
{
    if (fabsf(angle) > THERMOSTAT_HALF_ARC+6) return false;
    drag->position = fmaxf(-THERMOSTAT_HALF_ARC, fminf(THERMOSTAT_HALF_ARC, angle));
    drag->setpoint = target_at(drag->position);
    return true;
}

bool thermostat_move(thermostat_drag_t *drag, float delta)
{
    drag->position = fmaxf(-THERMOSTAT_HALF_ARC, fminf(THERMOSTAT_HALF_ARC, drag->position+delta));
    int target = target_at(drag->position);
    if (target == drag->setpoint) return false;
    drag->setpoint = target;
    return true;
}
