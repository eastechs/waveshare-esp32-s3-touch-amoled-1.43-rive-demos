#include "vu_meter.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t meter_start[] asm("_binary_vu_meter_rgb565_start");
extern const uint8_t meter_end[] asm("_binary_vu_meter_rgb565_end");

void app_main(void)
{
    display_init();
    ESP_ERROR_CHECK(vu_meter_valid(meter_start, meter_end-meter_start) ? ESP_OK : ESP_ERR_INVALID_SIZE);
    uint8_t *band = heap_caps_malloc(VU_METER_BAND_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(band ? ESP_OK : ESP_ERR_NO_MEM);
    vu_meter_t meter;
    vu_meter_init(&meter, meter_start, band);
    vu_meter_enter(&meter, NULL, true);
    uint32_t last_ms = esp_timer_get_time()/1000;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000/VU_METER_FPS));
        uint32_t now_ms = esp_timer_get_time()/1000;
        vu_meter_advance(&meter, now_ms-last_ms, NULL, true);
        last_ms = now_ms;
    }
}
