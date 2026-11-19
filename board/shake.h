#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float gravity[3];
    float first_peak[3];
    uint32_t peak_ms;
    uint32_t last_motion_ms;
    bool initialized;
    bool tracking_peak;
    bool settling;
} shake_detector_t;

// Feed acceleration in g at 100 Hz. Fires once per back-and-forth shake.
bool shake_update(shake_detector_t *state, uint32_t now_ms, const float acceleration[3]);

#ifdef __cplusplus
}
#endif
