#include "changed_frame.h"
#include "display.h"
#include "sensors.h"
#include "settings.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rive_thorvg.h"
#include <string.h>

extern const uint8_t scene_start[] asm("_binary_scene_riv_start");
extern const uint8_t scene_end[] asm("_binary_scene_riv_end");

static void forward_touch(rive_instance_t *rive)
{
    static bool held;
    static float last_x, last_y;
    bool down;
    int x, y;
    if (board_touch_read(&down, &x, &y) != ESP_OK) return;
    if (down) {
        float ax, ay;
        rive_screen_to_artboard(rive, x * 0.5f, y * 0.5f, &ax, &ay);
        if (!held) rive_pointer_down(rive, ax, ay);
        else if (ax != last_x || ay != last_y) rive_pointer_move(rive, ax, ay);
        last_x = ax;
        last_y = ay;
    } else if (held) {
        rive_pointer_up(rive, last_x, last_y);
    }
    held = down;
}

void app_main(void)
{
    display_init();
    board_i2c_init();
    ESP_ERROR_CHECK(board_touch_init());
    const size_t bytes = RENDER_SIZE * RENDER_SIZE * sizeof(uint32_t);
    uint32_t *pixels = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint32_t *previous = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(pixels && previous ? ESP_OK : ESP_ERR_NO_MEM);

    rive_thorvg_init(0);
    rive_instance_t *rive = rive_create(pixels, RENDER_SIZE, RENDER_SIZE, RIVE_PIXEL_FORMAT_ARGB8888);
    ESP_ERROR_CHECK(rive ? ESP_OK : ESP_ERR_NO_MEM);
    const char *artboard = RIV_ARTBOARD[0] ? RIV_ARTBOARD : NULL;
    ESP_ERROR_CHECK(rive_load_ex(rive, scene_start, scene_end-scene_start, artboard, NULL) ? ESP_OK : ESP_FAIL);
    ESP_LOGI("player", "Loaded %u bytes, artboard %.0f x %.0f", (unsigned)(scene_end-scene_start),
             rive_artboard_width(rive), rive_artboard_height(rive));
    rive_set_fit(rive, RIVE_FIT_CONTAIN, RIVE_ALIGN_CENTER);
    rive_advance(rive, 0);
    display_frame(pixels);
    memcpy(previous, pixels, bytes);

    int64_t last = esp_timer_get_time();
    for (;;) {
        int64_t start = esp_timer_get_time();
        forward_touch(rive);
        if (rive_advance(rive, (start-last) / 1000000.0f)) display_changes(previous, pixels);
        last = start;
        int remaining_ms = (33333 - (esp_timer_get_time()-start)) / 1000;
        vTaskDelay(pdMS_TO_TICKS(remaining_ms > 0 ? remaining_ms : 1));
    }
}
