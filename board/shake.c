#include "shake.h"
#include <string.h>

bool shake_update(shake_detector_t *state, uint32_t now_ms, const float acceleration[3])
{
    if (!state->initialized) {
        memcpy(state->gravity, acceleration, sizeof(state->gravity));
        state->initialized = true;
        return false;
    }

    float movement[3], energy = 0, alignment = 0;
    for (int axis = 0; axis < 3; ++axis) {
        movement[axis] = acceleration[axis] - state->gravity[axis];
        state->gravity[axis] += 0.045f * movement[axis];
        energy += movement[axis] * movement[axis];
        alignment += state->first_peak[axis] * movement[axis];
    }
    if (energy > 0.35f * 0.35f) state->last_motion_ms = now_ms;
    if (state->settling) {
        if (now_ms - state->last_motion_ms >= 500) state->settling = false;
        return false;
    }
    if (energy < 0.82f * 0.82f) return false;

    const uint32_t elapsed = now_ms - state->peak_ms;
    if (!state->tracking_peak || elapsed > 480) {
        memcpy(state->first_peak, movement, sizeof(movement));
        state->peak_ms = now_ms;
        state->tracking_peak = true;
    } else if (elapsed >= 45 && alignment < -0.40f) {
        state->tracking_peak = false;
        state->settling = true;
        return true;
    }
    return false;
}
