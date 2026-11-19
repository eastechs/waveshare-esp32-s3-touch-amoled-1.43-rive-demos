#include "display.h"
#include "pixel.h"

#include <math.h>
#include <string.h>
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "read_lcd_id_bsp.h"
#include "panel_init.h"

enum { TILE_ROWS = 16 };
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t transfer_done;
static uint16_t *tiles[2];

static bool on_transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event,
                             void *context)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &woken);
    return woken == pdTRUE;
}

void display_init(void)
{
    const uint8_t panel_id = read_lcd_id();
    const bool is_sh8601 = panel_id == 0x86;
    ESP_LOGI("display", "Panel probe 0x%02x, using %s init", panel_id,
             is_sh8601 ? "SH8601" : "CO5300");
    transfer_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(transfer_done ? ESP_OK : ESP_ERR_NO_MEM);
    const size_t tile_bytes = DISPLAY_SIZE * TILE_ROWS * sizeof(uint16_t);
    // Small regions can use both tile buffers as a single DMA transfer.
    tiles[0] = heap_caps_malloc(2*tile_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    ESP_ERROR_CHECK(tiles[0] ? ESP_OK : ESP_ERR_NO_MEM);
    tiles[1] = tiles[0] + DISPLAY_SIZE*TILE_ROWS;
    const spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(10, 11, 12, 13, 14, 2*tile_bytes);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    const esp_lcd_panel_io_spi_config_t spi = SH8601_PANEL_IO_QSPI_CONFIG(9, on_transfer_done, transfer_done);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &spi, &io));
    const sh8601_vendor_config_t vendor = {
        .flags.use_qspi_interface = 1,
        .init_cmds = is_sh8601 ? sh8601_lcd_init_cmds : co5300_lcd_init_cmds,
        .init_cmds_size = is_sh8601 ? sizeof(sh8601_lcd_init_cmds) / sizeof(*sh8601_lcd_init_cmds)
                                   : sizeof(co5300_lcd_init_cmds) / sizeof(*co5300_lcd_init_cmds),
    };
    const esp_lcd_panel_dev_config_t config = {
        .reset_gpio_num = 21, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16, .vendor_config = (void *)&vendor,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, is_sh8601 ? 0 : 6, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
}

void display_set_brightness(uint8_t level)
{
    // Both panel revisions use the QSPI register-write opcode and one brightness byte.
    const int command = (0x02 << 24) | (LCD_CMD_WRDISBV << 8);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, command, &level, sizeof(level)));
}

static void wait_for_transfer(void)
{
    ESP_ERROR_CHECK(xSemaphoreTake(transfer_done, pdMS_TO_TICKS(1000)) == pdTRUE
                    ? ESP_OK : ESP_ERR_TIMEOUT);
}

static void clip_panel_row(uint16_t *line, int x, int y, int width)
{
    const float radius = DISPLAY_SIZE / 2.0f;
    const float dy = y + 0.5f - radius;
    const int inset = (int)ceilf(radius - sqrtf(radius * radius - dy * dy) - 0.5f);
    const int left = inset - x, right = x + width - (DISPLAY_SIZE-inset);
    if (left > 0) memset(line, 0, (left < width ? left : width)*sizeof(*line));
    if (right > 0) {
        const int count = right < width ? right : width;
        memset(line+width-count, 0, count*sizeof(*line));
    }
}

static bool valid_region(int side, int x, int y, int width, int height)
{
    return width > 0 && height > 0 && x >= 0 && y >= 0 &&
           x <= side-width && y <= side-height;
}

void display_frame_region(const uint32_t *pixels, int x, int y, int width, int height)
{
    if (!pixels || !valid_region(RENDER_SIZE, x, y, width, height)) return;
    const int panel_x = DISPLAY_SIZE - 2*(y+height);
    const int line_width = 2*height;
    const int tile_pixels = DISPLAY_SIZE*TILE_ROWS;
    const bool one_transfer = 4*width*height <= 2*tile_pixels;
    const int columns_per_tile = one_transfer ? width : tile_pixels/(2*line_width);
    int tile_index = 0;
    for (int col = 0; col < width; col += columns_per_tile) {
        const int cols = width-col < columns_per_tile ? width-col : columns_per_tile;
        uint16_t *tile = tiles[tile_index];
        const int panel_y = 2*(x+col);
        for (int i = 0; i < cols; ++i) {
            uint16_t *line = tile + 2*i*line_width;
            for (int j = 0; j < height; ++j) {
                const uint32_t argb = pixels[(y+height-1-j)*RENDER_SIZE+x+col+i];
                line[2*j] = line[2*j+1] = __builtin_bswap16(pixel_rgb565(argb));
            }
            memcpy(line+line_width, line, line_width*sizeof(*line));
            clip_panel_row(line, panel_x, panel_y+2*i, line_width);
            clip_panel_row(line+line_width, panel_x, panel_y+2*i+1, line_width);
        }
        if (col) wait_for_transfer();
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, panel_x, panel_y, panel_x+line_width,
                                                  panel_y+2*cols, tile));
        tile_index ^= 1;
    }
    wait_for_transfer();
}

void display_frame(const uint32_t *pixels)
{
    display_frame_region(pixels, 0, 0, RENDER_SIZE, RENDER_SIZE);
}

void display_rgb565_region(int x, int y, int width, int height, const uint8_t *pixels)
{
    if (!pixels || !valid_region(DISPLAY_SIZE, x, y, width, height)) return;
    // Same clockwise rotation as display_frame, one source pixel per panel pixel.
    const int panel_x = DISPLAY_SIZE-y-height;
    int tile_index = 0;
    for (int col = 0; col < width; col += TILE_ROWS) {
        int rows = width-col < TILE_ROWS ? width-col : TILE_ROWS;
        uint16_t *out = tiles[tile_index];
        for (int i = 0; i < rows; ++i) {
            for (int j = height-1; j >= 0; --j) {
                const uint8_t *p = pixels + 2*(j*width+col+i);
                *out++ = ((uint16_t)p[0] << 8) | p[1];
            }
        }
        if (col) wait_for_transfer();
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, panel_x, x+col, panel_x+height,
                                                  x+col+rows, tiles[tile_index]));
        tile_index ^= 1;
    }
    wait_for_transfer();
}

typedef struct {
    display_rows_fn paint;
    const void *context;
    int y, count;
    uint16_t *out;
} row_job_t;

static TaskHandle_t row_worker;
static SemaphoreHandle_t rows_done;
static row_job_t row_job;

static void paint_on_second_core(void *unused)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        row_job.paint(row_job.context, row_job.y, row_job.count, row_job.out);
        xSemaphoreGive(rows_done);
    }
}

static void paint_parallel(display_rows_fn paint, const void *context, int y, int count, uint16_t *out)
{
    int split = (count/2) & ~1;
    if (!split) {
        paint(context, y, count, out);
        return;
    }
    row_job = (row_job_t){paint, context, y+split, count-split, out+split*DISPLAY_SIZE};
    xTaskNotifyGive(row_worker);
    paint(context, y, split, out);
    xSemaphoreTake(rows_done, portMAX_DELAY);
}

static void stream_tiles(int x, int y, int width, int height, display_scanline_fn line, void *context,
                         display_rows_fn parallel, const void *parallel_context)
{
    int tile = 0;
    for (int row = 0; row < height; row += TILE_ROWS) {
        int rows = height-row < TILE_ROWS ? height-row : TILE_ROWS;
        if (parallel) {
            paint_parallel(parallel, parallel_context, y+row, rows, tiles[tile]);
        } else {
            for (int i = 0; i < rows; ++i)
                line(context, x, y+row+i, width, tiles[tile]+i*width);
        }
        if (row) wait_for_transfer();
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, x, y+row, x+width, y+row+rows, tiles[tile]));
        tile ^= 1;
    }
    wait_for_transfer();
}

void display_scanlines(int x, int y, int width, int height, display_scanline_fn paint, void *context)
{
    if (!paint || !valid_region(DISPLAY_SIZE, x, y, width, height)) return;
    stream_tiles(x, y, width, height, paint, context, NULL, NULL);
}

void display_parallel_rows(display_rows_fn paint, const void *context)
{
    if (!paint) return;
    if (!row_worker) {
        rows_done = xSemaphoreCreateBinary();
        ESP_ERROR_CHECK(rows_done ? ESP_OK : ESP_ERR_NO_MEM);
        ESP_ERROR_CHECK(xTaskCreatePinnedToCore(paint_on_second_core, "panel rows", 3072, NULL,
                        uxTaskPriorityGet(NULL), &row_worker, 1-xPortGetCoreID()) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    }
    stream_tiles(0, 0, DISPLAY_SIZE, DISPLAY_SIZE, NULL, NULL, paint, context);
}
