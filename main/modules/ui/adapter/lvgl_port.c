#include "lvgl_port.h"

#include "lvgl.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "config/config_sys.h"
#include "core/utils/log.h"
#include "modules/display/lcd_i80_8.h"
#include "modules/input/touch/touch.h"
#include "core/msg/msg.h"

#define LCD_CMD_CASET 0x2A
#define LCD_CMD_PASET 0x2B
#define LCD_CMD_RAMWR 0x2C
#define LVGL_MIN_TASK_DELAY_MS 5

typedef struct {
    uint8_t *buf1;
    uint8_t *buf2;
    size_t buf_size;
    uint8_t bytes_per_pixel;
    size_t max_transfer_bytes;
} lvgl_dma_ctx_t;

static lv_display_t *s_disp = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;
static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t s_lvgl_task = NULL;
static lvgl_dma_ctx_t s_dma = {0};

static bool IRAM_ATTR lvgl_flush_done_cb(esp_lcd_panel_io_handle_t io,
                                         esp_lcd_panel_io_event_data_t *edata,
                                         void *user_ctx)
{
    LV_UNUSED(io);
    LV_UNUSED(edata);

    lv_display_t *disp = (lv_display_t *)user_ctx;
    if(disp) {
        lv_display_flush_ready(disp);
    }
    return false;
}

void lvgl_port_lock(void)
{
    if(s_lock) {
        (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

void lvgl_port_unlock(void)
{
    if(s_lock) {
        (void)xSemaphoreGive(s_lock);
    }
}

void lvgl_port_run(void (*cb)(void *), void *arg)
{
    if(cb == NULL) {
        return;
    }

    lvgl_port_lock();
    cb(arg);
    lvgl_port_unlock();
}

static void lv_tick_cb(void *arg)
{
    LV_UNUSED(arg);
    lv_tick_inc(1);
}

static void lvgl_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    if(!s_io || !disp || !area || !px_map) {
        if(disp) {
            lv_display_flush_ready(disp);
        }
        return;
    }

    size_t pixels = (size_t)lv_area_get_width(area) * (size_t)lv_area_get_height(area);
    size_t len = pixels * s_dma.bytes_per_pixel;

    if(s_dma.max_transfer_bytes > 0 && len > s_dma.max_transfer_bytes) {
        LOG("LVGL flush len too large: %u > %u", (unsigned)len, (unsigned)s_dma.max_transfer_bytes);
        lv_display_flush_ready(disp);
        return;
    }

    uint8_t col_data[4] = {
        (uint8_t)((area->x1 >> 8) & 0xFF),
        (uint8_t)(area->x1 & 0xFF),
        (uint8_t)((area->x2 >> 8) & 0xFF),
        (uint8_t)(area->x2 & 0xFF),
    };
    uint8_t row_data[4] = {
        (uint8_t)((area->y1 >> 8) & 0xFF),
        (uint8_t)(area->y1 & 0xFF),
        (uint8_t)((area->y2 >> 8) & 0xFF),
        (uint8_t)(area->y2 & 0xFF),
    };

    esp_err_t err = esp_lcd_panel_io_tx_param(s_io, LCD_CMD_CASET, col_data, sizeof(col_data));
    if(err != ESP_OK) {
        LOG("CASET failed: %s", esp_err_to_name(err));
        lv_display_flush_ready(disp);
        return;
    }

    err = esp_lcd_panel_io_tx_param(s_io, LCD_CMD_PASET, row_data, sizeof(row_data));
    if(err != ESP_OK) {
        LOG("PASET failed: %s", esp_err_to_name(err));
        lv_display_flush_ready(disp);
        return;
    }

    err = esp_lcd_panel_io_tx_color(s_io, LCD_CMD_RAMWR, px_map, len);
    if(err != ESP_OK) {
        LOG("RAMWR failed: %s", esp_err_to_name(err));
        lv_display_flush_ready(disp);
        return;
    }
}

static void lvgl_task(void *arg)
{
    LV_UNUSED(arg);

#if CONFIG_ESP_TASK_WDT_EN
    (void)esp_task_wdt_add(NULL);
#endif

    while(1) {
        lvgl_port_lock();
        uint32_t delay_ms = lv_timer_handler();
        lvgl_port_unlock();

#if CONFIG_ESP_TASK_WDT_EN
        (void)esp_task_wdt_reset();
#endif

        if(delay_ms < LVGL_MIN_TASK_DELAY_MS) {
            delay_ms = LVGL_MIN_TASK_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t lvgl_port_init(esp_lcd_panel_io_handle_t io)
{
    if(io == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(s_disp != NULL) {
        return ESP_OK;
    }

    // LOG("LVGL PORT INIT START");
    s_io = io;

    s_lock = xSemaphoreCreateMutex();
    if(!s_lock) {
        return ESP_FAIL;
    }

    lv_init();

    s_disp = lv_display_create(DISPLAY_H_RES, DISPLAY_V_RES);
    if(s_disp == NULL) {
        LOG("lv_display_create failed");
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

    s_dma.bytes_per_pixel = lv_color_format_get_size(lv_display_get_color_format(s_disp));
    s_dma.buf_size = DISPLAY_H_RES * LVGL_DRAW_BUF_LINES * s_dma.bytes_per_pixel;
    s_dma.max_transfer_bytes = lcd_get_max_transfer_bytes();

    if(s_dma.max_transfer_bytes > 0 && s_dma.max_transfer_bytes < s_dma.buf_size) {
        LOG("invalid i80 max_transfer_bytes=%u < lvgl_buf=%u",
                 (unsigned)s_dma.max_transfer_bytes, (unsigned)s_dma.buf_size);
        return ESP_ERR_INVALID_SIZE;
    }

    s_dma.buf1 = (uint8_t *)heap_caps_malloc(s_dma.buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_dma.buf2 = (uint8_t *)heap_caps_malloc(s_dma.buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if(!s_dma.buf1 || !s_dma.buf2) {
        LOG("LVGL buffer alloc failed (%u bytes each)", (unsigned)s_dma.buf_size);
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(s_disp,
                           s_dma.buf1,
                           s_dma.buf2,
                           s_dma.buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);

    esp_lcd_panel_io_callbacks_t io_cbs = {
        .on_color_trans_done = lvgl_flush_done_cb,
    };
    esp_err_t err = esp_lcd_panel_io_register_event_callbacks(s_io, &io_cbs, s_disp);
    if(err != ESP_OK) {
        LOG("register io callbacks failed: %s", esp_err_to_name(err));
        return err;
    }

    lv_indev_t *touch_indev = NULL;
    err = touch_lvgl_bind(s_disp, &touch_indev);
    if(err == ESP_OK) {
        (void)touch_set_orientation(0, s_disp);
    } else {
        LOG("touch bind failed, continue without touch: %s", esp_err_to_name(err));
    }

    const esp_timer_create_args_t tick_args = {
        .callback = lv_tick_cb,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick = NULL;

    err = esp_timer_create(&tick_args, &tick);
    if(err != ESP_OK) {
        LOG("esp_timer_create failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_timer_start_periodic(tick, 1000);
    if(err != ESP_OK) {
        LOG("esp_timer_start_periodic failed: %s", esp_err_to_name(err));
        return err;
    }

    if(xTaskCreatePinnedToCore(lvgl_task,
                               "lvgl",
                               8192,
                               NULL,
                               TASK_PRIO_LVGL,
                               &s_lvgl_task,
                               TASK_CORE_LVGL) != pdPASS) {
        LOG("create lvgl task failed");
        return ESP_FAIL;
    }

    // LOG("LVGL PORT INIT DONE (res=%dx%d, buf_lines=%d, buf=%u, max_tx=%u)",
    //          DISPLAY_H_RES,
    //          DISPLAY_V_RES,
    //          LVGL_DRAW_BUF_LINES,
    //          (unsigned)s_dma.buf_size,
    //          (unsigned)s_dma.max_transfer_bytes);
	

	
    return ESP_OK;
}

lv_display_t *lvgl_port_get_display(void)
{
    return s_disp;
}
