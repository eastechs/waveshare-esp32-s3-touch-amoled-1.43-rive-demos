#include "water.h"
#include "sensors.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t glass_start[] asm("_binary_water_glass_start");
extern const uint8_t glass_end[] asm("_binary_water_glass_end");

void app_main(void)
{
    static water_t water;
    display_init();
    board_i2c_init();
    ESP_ERROR_CHECK(board_accel_init());
    ESP_ERROR_CHECK(water_init(&water, glass_start, glass_end-glass_start) ? ESP_OK : ESP_ERR_INVALID_SIZE);
    float force[3];
    ESP_ERROR_CHECK(board_accel_read(force));
    water_enter(&water, force);
    uint32_t last_ms = esp_timer_get_time()/1000;
    for (;;) {
        uint32_t now_ms = esp_timer_get_time()/1000;
        float sample[3];
        if (board_accel_read(sample) == ESP_OK) {
            for (int i = 0; i < 3; ++i) force[i] += 0.25f * (sample[i] - force[i]);
        }
        water_advance(&water, force, now_ms-last_ms);
        water_present(&water);
        last_ms = now_ms;
        vTaskDelay(1);
    }
}
