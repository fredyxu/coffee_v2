#include "lcd_i80_8.h"
#include "lcd_i80_8_panel.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_i80.h"
#include "config/config_pin.h"
#include "config/config_sys.h"
#include "core/utils/log.h"

// 性能配置 (同display_i80_8.c)
#define I80_PERF_PROFILE           2
#define I80_BUS_WIDTH              8

#if (CONFIG_LCD_PANEL_CTRL == 0 && CONFIG_LCD_ILI9341_TIMING_PROFILE == 1)
#define I80_PCLK_HZ                (10 * 1000 * 1000)
#define I80_TRANS_QUEUE_DEPTH      12
#define I80_DRAW_BUF_LINES         50
#define I80_TRANSFER_BYTES_PER_PIXEL DISPLAY_COLOR_BYTES_PER_PIXEL
#elif (I80_PERF_PROFILE == 0)
#define I80_PCLK_HZ                (8 * 1000 * 1000)
#define I80_TRANS_QUEUE_DEPTH      12
#define I80_DRAW_BUF_LINES         50
#define I80_TRANSFER_BYTES_PER_PIXEL DISPLAY_COLOR_BYTES_PER_PIXEL
#elif (I80_PERF_PROFILE == 1)
#define I80_PCLK_HZ                (16 * 1000 * 1000)
#define I80_TRANS_QUEUE_DEPTH      18
#define I80_DRAW_BUF_LINES         80
#define I80_TRANSFER_BYTES_PER_PIXEL DISPLAY_COLOR_BYTES_PER_PIXEL
#elif (I80_PERF_PROFILE == 2)
#define I80_PCLK_HZ                (20 * 1000 * 1000)
#define I80_TRANS_QUEUE_DEPTH      24
#define I80_DRAW_BUF_LINES         100
#define I80_TRANSFER_BYTES_PER_PIXEL DISPLAY_COLOR_BYTES_PER_PIXEL
#endif


// 方向配置
#define I80_LCD_ORI0_SWAP_XY       1
#define I80_LCD_ORI0_MIRROR_X      1
#define I80_LCD_ORI0_MIRROR_Y      0

// 颜色配置
#define I80_LCD_BGR_ORDER          0
#if (CONFIG_LCD_PANEL_CTRL == 0)
#define I80_IO_SWAP_COLOR_BYTES    1
#define I80_IO_PCLK_ACTIVE_NEG     0
#else
#define I80_IO_SWAP_COLOR_BYTES    1
#define I80_IO_PCLK_ACTIVE_NEG     0
#endif

// 背光配置
#define BL_LEDC_MODE                LEDC_LOW_SPEED_MODE
#define BL_LEDC_TIMER               LEDC_TIMER_1
#define BL_LEDC_CHANNEL             LEDC_CHANNEL_1
#define BL_LEDC_FREQ_HZ             5000
#define BL_LEDC_RESOLUTION          LEDC_TIMER_10_BIT
#define BL_LEDC_MAX_DUTY            ((1 << 10) - 1)

#if (CONFIG_LCD_PANEL_CTRL == 0)
#define LCD_RESET_LOW_MS            120
#define LCD_RESET_POST_MS           250
#else
#define LCD_RESET_LOW_MS            20
#define LCD_RESET_POST_MS           120
#endif

static esp_lcd_panel_io_handle_t s_io = NULL;
static bool s_backlight_ready = false;
static size_t s_max_transfer_bytes = 0;

// MADCTL位
#define LCD_MADCTL_MY              0x80
#define LCD_MADCTL_MX              0x40
#define LCD_MADCTL_MV              0x20
#define LCD_MADCTL_BGR             0x08

static bool log_init_error(esp_err_t err, const char *step)
{
    if(err == ESP_OK) return false;
    LOG("%s failed: %s", step, esp_err_to_name(err));
    return true;
}

static void fill_i80_data_gpio_nums(int *data_gpio_nums)
{
    data_gpio_nums[0] = PIN_LCD_D0;
    data_gpio_nums[1] = PIN_LCD_D1;
    data_gpio_nums[2] = PIN_LCD_D2;
    data_gpio_nums[3] = PIN_LCD_D3;
    data_gpio_nums[4] = PIN_LCD_D4;
    data_gpio_nums[5] = PIN_LCD_D5;
    data_gpio_nums[6] = PIN_LCD_D6;
    data_gpio_nums[7] = PIN_LCD_D7;
}

static void lcd_i80_hardware_reset(void)
{
    gpio_set_level(PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(LCD_RESET_LOW_MS));
    gpio_set_level(PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(LCD_RESET_POST_MS));
}

void lcd_i80_panel_write_cmd(uint8_t cmd)
{
    if(s_io == NULL) return;
    esp_lcd_panel_io_tx_param(s_io, cmd, NULL, 0);
}

void lcd_i80_panel_write_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    if(s_io == NULL) return;
    esp_lcd_panel_io_tx_param(s_io, cmd, data, len);
}

static void set_window(int x1, int y1, int x2, int y2)
{
    uint8_t buf[4];
    buf[0] = (x1 >> 8) & 0xFF;
    buf[1] = x1 & 0xFF;
    buf[2] = (x2 >> 8) & 0xFF;
    buf[3] = x2 & 0xFF;
    lcd_i80_panel_write_data(LCD_CMD_CASET, buf, 4);

    buf[0] = (y1 >> 8) & 0xFF;
    buf[1] = y1 & 0xFF;
    buf[2] = (y2 >> 8) & 0xFF;
    buf[3] = y2 & 0xFF;
    lcd_i80_panel_write_data(LCD_CMD_PASET, buf, 4);
}

static bool panel_flags_to_rot_idx(bool swap_xy, bool mirror_x, bool mirror_y, uint8_t *out_idx)
{
    if(swap_xy == false && mirror_x == false && mirror_y == false) {
        *out_idx = 0;
        return true;
    }
    if(swap_xy == true && mirror_x == true && mirror_y == false) {
        *out_idx = 1;
        return true;
    }
    if(swap_xy == false && mirror_x == true && mirror_y == true) {
        *out_idx = 2;
        return true;
    }
    if(swap_xy == true && mirror_x == false && mirror_y == true) {
        *out_idx = 3;
        return true;
    }
    return false;
}

static void rot_idx_to_panel_flags(uint8_t rot_idx, bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    switch(rot_idx & 0x03U) {
        case 0:
            *swap_xy = false; *mirror_x = false; *mirror_y = false;
            break;
        case 1:
            *swap_xy = true;  *mirror_x = true;  *mirror_y = false;
            break;
        case 2:
            *swap_xy = false; *mirror_x = true;  *mirror_y = true;
            break;
        default:
            *swap_xy = true;  *mirror_x = false; *mirror_y = true;
            break;
    }
}

static void panel_apply_orientation(uint8_t orientation)
{
    uint8_t ori = orientation & 0x03U;
    uint8_t base_rot = 0;
    if(!panel_flags_to_rot_idx(I80_LCD_ORI0_SWAP_XY, I80_LCD_ORI0_MIRROR_X, I80_LCD_ORI0_MIRROR_Y, &base_rot)) {
        LOG("invalid ORI0 flags, fallback to rot=0");
        base_rot = 0;
    }

    uint8_t rot = (uint8_t)((base_rot + ori) & 0x03U);
    bool swap_xy = false;
    bool mirror_x = false;
    bool mirror_y = false;
    rot_idx_to_panel_flags(rot, &swap_xy, &mirror_x, &mirror_y);

    uint8_t madctl = 0;
    if(swap_xy) madctl |= LCD_MADCTL_MV;
    if(mirror_x) madctl |= LCD_MADCTL_MX;
    if(mirror_y) madctl |= LCD_MADCTL_MY;
    if(I80_LCD_BGR_ORDER) madctl |= LCD_MADCTL_BGR;
    lcd_i80_panel_write_data(LCD_CMD_MADCTL, &madctl, 1);

    // LOG("orientation=%u (MADCTL=0x%02X)", (unsigned)ori, madctl);
}

void lcd_i80_panel_apply_landscape_dir(void)
{
    uint8_t orientation = 0;
    if(CONFIG_LCD_LANDSCAPE_DIR == 1) {
        orientation = 2;
    } else if(CONFIG_LCD_LANDSCAPE_DIR != 0) {
        LOG("invalid CONFIG_LCD_LANDSCAPE_DIR=%d, fallback to normal", CONFIG_LCD_LANDSCAPE_DIR);
    }
    panel_apply_orientation(orientation);
}

void lcd_i80_panel_apply_madctl(uint8_t normal, uint8_t reverse)
{
    uint8_t madctl = normal;
    if(CONFIG_LCD_LANDSCAPE_DIR == 1) {
        madctl = reverse;
    } else if(CONFIG_LCD_LANDSCAPE_DIR != 0) {
        LOG("invalid CONFIG_LCD_LANDSCAPE_DIR=%d, fallback to normal", CONFIG_LCD_LANDSCAPE_DIR);
    }
    lcd_i80_panel_write_data(LCD_CMD_MADCTL, &madctl, 1);
}

static esp_err_t backlight_init(void)
{
    if(s_backlight_ready) return ESP_OK;

    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    if(log_init_error(gpio_config(&bk_gpio_config), "gpio_config(BL)")) {
        return ESP_FAIL;
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = BL_LEDC_MODE,
        .duty_resolution = BL_LEDC_RESOLUTION,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if(log_init_error(ledc_timer_config(&timer_cfg), "ledc_timer_config")) {
        return ESP_FAIL;
    }

    ledc_channel_config_t ch_cfg = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = BL_LEDC_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    if(log_init_error(ledc_channel_config(&ch_cfg), "ledc_channel_config")) {
        return ESP_FAIL;
    }

    s_backlight_ready = true;
    return ESP_OK;
}

esp_err_t lcd_set_backlight(uint8_t percent)
{
    if(percent > 100) percent = 100;
    if(!s_backlight_ready && backlight_init() != ESP_OK) {
        return ESP_FAIL;
    }

    uint32_t duty = (BL_LEDC_MAX_DUTY * (uint32_t)percent) / 100U;
    esp_err_t err = ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
    if(err != ESP_OK) return err;
    return ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}

esp_lcd_panel_io_handle_t lcd_get_io(void)
{
    return s_io;
}

size_t lcd_get_max_transfer_bytes(void)
{
    return s_max_transfer_bytes;
}

static esp_err_t panel_fill_color_blocking(uint16_t color565, int y_start, int y_end)
{
    const size_t line_px = DISPLAY_H_RES;
    const size_t line_bytes = line_px * sizeof(uint16_t);
    uint16_t *line = (uint16_t *)heap_caps_malloc(line_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if(line == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for(size_t i = 0; i < line_px; i++) {
        line[i] = color565;
    }

    for(int y = y_start; y <= y_end; y++) {
        set_window(0, y, DISPLAY_H_RES - 1, y);
        esp_err_t err = esp_lcd_panel_io_tx_color(s_io, LCD_CMD_RAMWR, line, line_bytes);
        if(err != ESP_OK) {
            heap_caps_free(line);
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    heap_caps_free(line);
    return ESP_OK;
}

void lcd_test_color(void)
{
    const int h = DISPLAY_V_RES;
    const int band = h / 6;
    (void)panel_fill_color_blocking(0xF800, 0 * band, 1 * band - 1); // Red
    (void)panel_fill_color_blocking(0x07E0, 1 * band, 2 * band - 1); // Green
    (void)panel_fill_color_blocking(0x001F, 2 * band, 3 * band - 1); // Blue
    (void)panel_fill_color_blocking(0xFFFF, 3 * band, 4 * band - 1); // White
    (void)panel_fill_color_blocking(0x0000, 4 * band, 5 * band - 1); // Black
    (void)panel_fill_color_blocking(0xFFE0, 5 * band, h - 1);        // Yellow
}

esp_err_t lcd_i80_8_init(void)
{
    // LOG("LCD INIT START");

    if(backlight_init() != ESP_OK) return ESP_FAIL;

    gpio_config_t rst_gpio_config = {
        .pin_bit_mask = (1ULL << PIN_LCD_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    if(log_init_error(gpio_config(&rst_gpio_config), "gpio_config(RST)")) {
        return ESP_FAIL;
    }

#if (CONFIG_LCD_PANEL_CTRL == 0)
    (void)lcd_set_backlight(0);
    gpio_set_level(PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
#else
    (void)lcd_set_backlight(100);
    gpio_set_level(PIN_LCD_RST, 1);
#endif

    int data_gpio_nums[I80_BUS_WIDTH] = {0};
    fill_i80_data_gpio_nums(data_gpio_nums);

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;

    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .wr_gpio_num = PIN_LCD_WR,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {0},
        .bus_width = I80_BUS_WIDTH,
        .max_transfer_bytes = DISPLAY_H_RES * LVGL_DRAW_BUF_LINES * I80_TRANSFER_BYTES_PER_PIXEL,
        .dma_burst_size = 64,
    };
    for(int i = 0; i < I80_BUS_WIDTH; i++) {
        bus_config.data_gpio_nums[i] = data_gpio_nums[i];
    }
    if(log_init_error(esp_lcd_new_i80_bus(&bus_config, &i80_bus), "esp_lcd_new_i80_bus")) {
        return ESP_FAIL;
    }

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = I80_PCLK_HZ,
        .trans_queue_depth = I80_TRANS_QUEUE_DEPTH,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = I80_IO_SWAP_COLOR_BYTES,
            .pclk_active_neg = I80_IO_PCLK_ACTIVE_NEG,
            .pclk_idle_low = 0,
        },
    };
    if(log_init_error(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle), "esp_lcd_new_panel_io_i80")) {
        return ESP_FAIL;
    }

    s_io = io_handle;
    s_max_transfer_bytes = bus_config.max_transfer_bytes;

    // Hardware reset
    lcd_i80_hardware_reset();

#if (CONFIG_LCD_PANEL_CTRL == 0)
    return lcd_panel_ili9341_init();
#elif (CONFIG_LCD_PANEL_CTRL == 1)
    return lcd_panel_st7798_init();
#else
#error "Invalid CONFIG_LCD_PANEL_CTRL: 0=ILI9341, 1=ST7798"
#endif
}
