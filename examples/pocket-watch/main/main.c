#include "changed_frame.h"
#include "display.h"
#include "sensors.h"
#include "watch.h"
#include "watch_clock.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "miniz.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t dial_start[] asm("_binary_watch_dial_argb_z_start");
extern const uint8_t dial_end[] asm("_binary_watch_dial_argb_z_end");
extern const uint8_t lid_start[] asm("_binary_watch_lid_argb_z_start");
extern const uint8_t lid_end[] asm("_binary_watch_lid_argb_z_end");
extern const uint8_t tray_start[] asm("_binary_watch_tray_argb_z_start");
extern const uint8_t tray_end[] asm("_binary_watch_tray_argb_z_end");

static uint32_t *frame_buffer(size_t side)
{
    uint32_t *buffer = heap_caps_malloc(side * side * sizeof(*buffer), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(buffer ? ESP_OK : ESP_ERR_NO_MEM);
    return buffer;
}

static uint32_t *unpack(const uint8_t *start, const uint8_t *end, size_t side)
{
    uint32_t *pixels = frame_buffer(side);
    size_t capacity = side * side * sizeof(*pixels);
    size_t input = end - start, output = capacity;
    tinfl_decompressor *decoder = malloc(sizeof(*decoder));
    ESP_ERROR_CHECK(decoder ? ESP_OK : ESP_ERR_NO_MEM);
    tinfl_init(decoder);
    tinfl_status status = tinfl_decompress(decoder, start, &input, (uint8_t *)pixels, (uint8_t *)pixels,
        &output, TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(decoder);
    ESP_ERROR_CHECK(status == TINFL_STATUS_DONE && output == capacity ? ESP_OK : ESP_ERR_INVALID_SIZE);
    return pixels;
}

// Recognize a tap on release, with the same time and travel limits as the original watch.
static bool tapped(uint64_t now_ms)
{
    static bool held, dragged;
    static uint64_t began_ms;
    static int start_x, start_y;
    bool down;
    int x, y;
    if (board_touch_read(&down, &x, &y) != ESP_OK) {
        dragged = true;
        return false;
    }
    if (down && !held) {
        start_x = x;
        start_y = y;
        began_ms = now_ms;
        dragged = false;
    } else if (down && (abs(x-start_x) > 16 || abs(y-start_y) > 16)) {
        dragged = true;
    }
    bool tap = !down && held && !dragged && now_ms-began_ms <= 400;
    held = down;
    return tap;
}

static bool update_light(watch_t *watch)
{
    static float gravity[3];
    float g[3];
    if (board_accel_read(g) != ESP_OK) return false;
    for (int i = 0; i < 3; ++i) gravity[i] += .17f*(g[i]-gravity[i]);
    float magnitude = sqrtf(gravity[0]*gravity[0]+gravity[1]*gravity[1]+gravity[2]*gravity[2]);
    return magnitude > .01f && watch_set_tilt(watch, gravity[0]/magnitude, gravity[1]/magnitude);
}

void app_main(void)
{
    display_init();
    board_i2c_init();
    ESP_ERROR_CHECK(board_touch_init());
    ESP_ERROR_CHECK(board_accel_init());
    watch_clock_start();

    // Rotating parts and the panel's column reads both revisit this buffer.
    // Keep it in internal SRAM and use PSRAM for the layers and prior frame.
    static uint32_t frame[RENDER_SIZE * RENDER_SIZE];
    uint32_t *previous = frame_buffer(RENDER_SIZE);
    uint32_t *dial = unpack(dial_start, dial_end, RENDER_SIZE);
    uint32_t *lid = unpack(lid_start, lid_end, RENDER_SIZE);
    uint32_t *tray = unpack(tray_start, tray_end, DISPLAY_SIZE);
    watch_t watch;
    watch_init(&watch, dial, lid, tray);
    uint64_t opened_ms = esp_timer_get_time()/1000;
    watch_toggle(&watch, opened_ms);

    // Run the standalone clock at the original 40 ms cadence. A missed deadline
    // skips to the actual hand position rather than slowing down the clock.
    const TickType_t period = pdMS_TO_TICKS(40);
    TickType_t deadline = xTaskGetTickCount();
    uint64_t sample_start = 0;
    unsigned frames = 0;
    bool first = true, was_closed = false, measured = false;
    for (;;) {
        uint64_t now_ms = esp_timer_get_time()/1000;
        if (tapped(now_ms)) watch_toggle(&watch, now_ms);
        bool closed = watch_is_closed(&watch, now_ms);
        bool moved = update_light(&watch);
        if (!closed || !was_closed || moved) {
            watch_paint(&watch, frame, now_ms, watch_local_ms(now_ms));
            bool sent = true;
            if (first) {
                display_frame(frame);
                memcpy(previous, frame, sizeof(frame));
                first = false;
            } else {
                sent = display_changes(previous, frame);
            }
            // Count complete panel transfers with the lid open.
            uint64_t done = esp_timer_get_time();
            if (!measured && watch_is_open(&watch, now_ms)) {
                if (!sample_start) sample_start = done;
                else if (sent) ++frames;
                if (done-sample_start >= 5000000) {
                    double seconds = (done-sample_start) / 1e6;
                    ESP_LOGI("watch", "%u displayed frames in %.2f s (%.1f FPS)", frames, seconds, frames/seconds);
                    measured = true;
                }
            } else {
                frames = 0;
                sample_start = 0;
            }
        }
        was_closed = closed;
        if (!xTaskDelayUntil(&deadline, period)) {
            deadline = xTaskGetTickCount();
            vTaskDelay(1);
        }
    }
}
