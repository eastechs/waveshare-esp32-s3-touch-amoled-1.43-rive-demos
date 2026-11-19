#include "sensors.h"
#include "display.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bsp.h"
#include "qmi8658c.h"

enum { FT3168 = 0x38 };

void board_i2c_init(void)
{
    I2C_master_Init();
}

static esp_err_t write_register(uint8_t address, uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_write_to_device(I2C_NUM_0, address, data, sizeof(data), pdMS_TO_TICKS(20));
}

static esp_err_t read_registers(uint8_t address, uint8_t reg, uint8_t *out, size_t count)
{
    return i2c_master_write_read_device(I2C_NUM_0, address, &reg, 1, out, count, pdMS_TO_TICKS(20));
}

esp_err_t board_touch_init(void)
{
    return write_register(FT3168, 0, 0);
}

esp_err_t board_touch_read(bool *down, int *x, int *y)
{
    if (!down || !x || !y) return ESP_ERR_INVALID_ARG;
    uint8_t data[5];
    esp_err_t error = read_registers(FT3168, 0x02, data, sizeof(data));
    if (error != ESP_OK) return error;
    *down = (data[0] & 15) != 0;
    if (*down) {
        int raw_x = ((data[1] & 15) << 8) | data[2];
        int raw_y = ((data[3] & 15) << 8) | data[4];
        if (raw_x >= DISPLAY_SIZE || raw_y >= DISPLAY_SIZE) return ESP_ERR_INVALID_RESPONSE;
        *x = raw_y;
        *y = DISPLAY_SIZE - 1 - raw_x;
    }
    return ESP_OK;
}

esp_err_t board_accel_init(void)
{
    const uint8_t addr = QMI8658_SLAVE_ADDR_H;
    uint8_t id;
    ESP_RETURN_ON_ERROR(read_registers(addr, Qmi8658Register_WhoAmI, &id, 1), "imu", "read ID");
    if (id != 0x05) return ESP_ERR_INVALID_RESPONSE;
    ESP_RETURN_ON_ERROR(write_register(addr, Qmi8658Register_Ctrl7, QMI8658_DISABLE_ALL), "imu", "disable");
    ESP_RETURN_ON_ERROR(write_register(addr, Qmi8658Register_Ctrl1, 0x40), "imu", "burst reads");
    ESP_RETURN_ON_ERROR(write_register(addr, Qmi8658Register_Ctrl2,
        Qmi8658AccRange_8g | Qmi8658AccOdr_125Hz), "imu", "range and sample rate");
    ESP_RETURN_ON_ERROR(write_register(addr, Qmi8658Register_Ctrl7, QMI8658_ACC_ENABLE), "imu", "enable");
    vTaskDelay(pdMS_TO_TICKS(60));
    return ESP_OK;
}

esp_err_t board_accel_read(float force[3])
{
    if (!force) return ESP_ERR_INVALID_ARG;
    uint8_t raw[6];
    esp_err_t error = read_registers(QMI8658_SLAVE_ADDR_H, Qmi8658Register_Ax_L, raw, sizeof(raw));
    if (error != ESP_OK) return error;
    float g[3];
    for (int i = 0; i < 3; ++i) g[i] = (int16_t)(raw[2*i] | raw[2*i+1] << 8) / 4096.0f;
    force[0] = -g[1];
    force[1] = g[0];
    force[2] = g[2];
    return ESP_OK;
}
