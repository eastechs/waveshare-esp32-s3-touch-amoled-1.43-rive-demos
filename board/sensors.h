#pragma once
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Call once before initializing either sensor on the shared I2C bus.
void board_i2c_init(void);
esp_err_t board_touch_init(void);
// Coordinates use the upright 466-pixel artwork space, before render downscaling. Read x and y only on a successful contact.
esp_err_t board_touch_read(bool *down, int *x, int *y);
esp_err_t board_accel_init(void);
// Apparent gravity in g, with axes right, down and toward the viewer.
esp_err_t board_accel_read(float force[3]);

#ifdef __cplusplus
}
#endif
