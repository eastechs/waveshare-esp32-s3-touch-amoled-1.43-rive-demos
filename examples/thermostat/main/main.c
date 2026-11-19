#include "display.h"
#include "sensors.h"
#include "ring_drag.h"
#include "thermostat_view.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t scene_start[] asm("_binary_thermostat_riv_start");
extern const uint8_t scene_end[] asm("_binary_thermostat_riv_end");

void app_main(void)
{
    display_init();
    board_i2c_init();
    ESP_ERROR_CHECK(board_touch_init());
    uint32_t *pixels = heap_caps_calloc(RENDER_SIZE*RENDER_SIZE, sizeof(*pixels),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(pixels ? ESP_OK : ESP_ERR_NO_MEM);
    rive_thorvg_init(0);
    rive_instance_t *scene = rive_create(pixels, RENDER_SIZE, RENDER_SIZE, RIVE_PIXEL_FORMAT_ARGB8888);
    ESP_ERROR_CHECK(scene ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(rive_load(scene, scene_start, scene_end-scene_start) ? ESP_OK : ESP_FAIL);
    thermostat_view_t view = {0};
    ESP_ERROR_CHECK(thermostat_bake(&view, scene, pixels) ? ESP_OK : ESP_FAIL);
    rive_destroy(scene);
    rive_thorvg_term();

    thermostat_drag_t dial = {.setpoint = THERMOSTAT_INITIAL};
    thermostat_paint(&view, dial.setpoint, pixels);
    display_frame(pixels);
    ring_drag_t ring = {0};
    bool held = false, captured = false;
    for (;;) {
        bool down;
        int x, y;
        if (board_touch_read(&down, &x, &y) == ESP_OK) {
            int previous = dial.setpoint;
            if (down && !held)
                captured = ring_begin(&ring, x, y) && thermostat_begin(&dial, ring.angle);
            else if (down && captured)
                thermostat_move(&dial, ring_turn(&ring, x, y));
            if (dial.setpoint != previous) {
                thermostat_paint(&view, dial.setpoint, pixels);
                display_frame(pixels);
            }
            held = down;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}
