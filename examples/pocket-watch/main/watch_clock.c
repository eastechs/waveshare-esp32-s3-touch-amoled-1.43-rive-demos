#include "watch_clock.h"
#include "driver/i2c.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { PCF85063 = 0x51, DAY_MS = 86400000 };
static uint64_t base_uptime_ms;
static uint32_t base_local_ms;

static int from_bcd(uint8_t b)
{
    return (b >> 4)*10 + (b & 15);
}

static uint8_t bcd(int n)
{
    return (n/10)*16 + n%10;
}

void watch_clock_start(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    char month[4] = {0};
    int year = 2000;
    struct tm build = {.tm_mday = 1};
    sscanf(app->date, "%3s %d %d", month, &build.tm_mday, &year);
    sscanf(app->time, "%d:%d:%d", &build.tm_hour, &build.tm_min, &build.tm_sec);
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *match = strstr(months, month);
    build.tm_mon = match ? (match-months)/3 : 0;
    build.tm_year = year-1900;
    time_t built = mktime(&build);
    struct tm local = build;
    uint8_t reg = 0, data[11];
    const TickType_t timeout = pdMS_TO_TICKS(40);
    const char *source = "firmware build time";
    if (i2c_master_write_read_device(I2C_NUM_0, PCF85063, &reg, 1, data, sizeof(data), timeout) == ESP_OK) {
        bool twelve = (data[0] & 0x02) != 0;
        int h = from_bcd(data[6] & (twelve ? 0x1f : 0x3f));
        struct tm rtc = {.tm_year=100+from_bcd(data[10]), .tm_mon=from_bcd(data[9]&0x1f)-1,
                         .tm_mday=from_bcd(data[7]&0x3f), .tm_min=from_bcd(data[5]&0x7f),
                         .tm_sec=from_bcd(data[4]&0x7f),
                         .tm_hour=twelve ? h%12 + ((data[6]&0x20) ? 12 : 0) : h};
        bool valid = !(data[0]&0x20) && !(data[4]&0x80) && rtc.tm_mon >= 0 && rtc.tm_mon < 12 &&
                     rtc.tm_mday >= 1 && rtc.tm_mday <= 31 && rtc.tm_hour < 24 &&
                     rtc.tm_min < 60 && rtc.tm_sec < 60;
        if (valid && difftime(built, mktime(&rtc)) < 86400) {
            local = rtc;
            source = "PCF85063";
        } else {
            uint8_t control[] = {0, data[0] & ~0x22}; // Start oscillator and select 24-hour time.
            uint8_t calendar[] = {4, bcd(build.tm_sec), bcd(build.tm_min), bcd(build.tm_hour),
                                  bcd(build.tm_mday), bcd(build.tm_wday), bcd(build.tm_mon+1),
                                  bcd((year-2000)%100)};
            if (i2c_master_write_to_device(I2C_NUM_0, PCF85063, control, sizeof(control), timeout) == ESP_OK &&
                i2c_master_write_to_device(I2C_NUM_0, PCF85063, calendar, sizeof(calendar), timeout) == ESP_OK)
                source = "PCF85063 initialized from firmware build time";
        }
    }
    base_uptime_ms = esp_timer_get_time()/1000;
    base_local_ms = ((local.tm_hour*60 + local.tm_min)*60 + local.tm_sec)*1000u;
    ESP_LOGI("watch", "%02d:%02d:%02d (%s)", local.tm_hour, local.tm_min, local.tm_sec, source);
}

uint32_t watch_local_ms(uint64_t uptime_ms)
{
    return (base_local_ms + uptime_ms-base_uptime_ms) % DAY_MS;
}
