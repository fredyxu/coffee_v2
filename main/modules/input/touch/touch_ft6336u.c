#include "touch_ft6336u.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "config/config_sys.h"
#include "core/utils/log.h"
// #include "LOG.h"

// static const char *TAG = "touch_ft6336u";

extern const int g_touch_pin_sda;
extern const int g_touch_pin_scl;
extern const int g_touch_pin_int;
extern const int g_touch_pin_rst;

/* FT6336U 默认 I2C 地址 */
#define FT6336U_ADDR                0x38

/*
 * 低风险推荐接线：
 * TSDI / SDA -> GPIO18
 * TCLK / SCL -> GPIO47
 * TPEN / INT -> GPIO17
 * TCS  / RST -> GPIO16
 * TSDO / NC  -> 不接
 */
#define FT_I2C_PORT                 I2C_NUM_0
#define FT_I2C_FREQ_HZ              TOUCH_I2C_FREQ_HZ

#define FT_TOUCH_H_RES              DISPLAY_H_RES
#define FT_TOUCH_V_RES              DISPLAY_V_RES

/* FT6336U registers */
#define FT_REG_DEV_MODE             0x00
#define FT_REG_GEST_ID              0x01
#define FT_REG_TD_STATUS            0x02
#define FT_REG_P1_XH                0x03
#define FT_REG_P1_XL                0x04
#define FT_REG_P1_YH                0x05
#define FT_REG_P1_YL                0x06

#define FT_REG_ID_G_LIB_VERSION_H   0xA1
#define FT_REG_ID_G_LIB_VERSION_L   0xA2
#define FT_REG_ID_G_CHIPID          0xA3
#define FT_REG_ID_G_MODE            0xA4
#define FT_REG_ID_G_FIRMID          0xA6
#define FT_REG_ID_G_CIPHER          0xA8

#define FT_TOUCH_EVT_DOWN           0x00
#define FT_TOUCH_EVT_UP             0x01
#define FT_TOUCH_EVT_CONTACT        0x02

typedef struct {
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} touch_ft_cfg_t;

typedef struct {
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
    int16_t offset_x;
    int16_t offset_y;
} touch_ft_orientation_cfg_t;

static bool s_inited = false;
static lv_indev_t *s_indev = NULL;

static touch_ft_cfg_t s_cfg = {
    .swap_xy = true,
    .mirror_x = false,
    .mirror_y = false,
};
static int16_t s_offset_x = 0;
static int16_t s_offset_y = 0;
static int16_t s_h_res = FT_TOUCH_H_RES;
static int16_t s_v_res = FT_TOUCH_V_RES;

// Orientation mapping for user-facing ids:
// 0=0deg, 1=90deg, 2=180deg, 3=270deg.
// Keep these panel-specific mappings in touch driver to keep app/main clean.
static const touch_ft_orientation_cfg_t s_ori_cfg[4] = {
    {.swap_xy = true,  .mirror_x = false, .mirror_y = true,  .offset_x = 0, .offset_y = 0}, // 0
    {.swap_xy = false, .mirror_x = true,  .mirror_y = true,  .offset_x = 0, .offset_y = 0}, // 90
    {.swap_xy = true,  .mirror_x = true,  .mirror_y = false, .offset_x = 0, .offset_y = 0}, // 180
    {.swap_xy = false, .mirror_x = false, .mirror_y = false, .offset_x = 0, .offset_y = 0}, // 270
};

static esp_err_t ft_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_write_to_device(
        FT_I2C_PORT,
        FT6336U_ADDR,
        buf,
        sizeof(buf),
        pdMS_TO_TICKS(50)
    );
}

static esp_err_t ft_read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    if(buf == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_write_read_device(
        FT_I2C_PORT,
        FT6336U_ADDR,
        &reg,
        1,
        buf,
        len,
        pdMS_TO_TICKS(50)
    );
}

static void ft_transform_point(int16_t *x, int16_t *y)
{
    int16_t tx = *x;
    int16_t ty = *y;

    if(s_cfg.swap_xy) {
        int16_t t = tx;
        tx = ty;
        ty = t;
    }

    if(s_cfg.mirror_x) {
        tx = (s_h_res - 1) - tx;
    }

    if(s_cfg.mirror_y) {
        ty = (s_v_res - 1) - ty;
    }

    tx += s_offset_x;
    ty += s_offset_y;

    if(tx < 0) tx = 0;
    if(ty < 0) ty = 0;
    if(tx >= s_h_res) tx = s_h_res - 1;
    if(ty >= s_v_res) ty = s_v_res - 1;

    *x = tx;
    *y = ty;
}

static esp_err_t ft_hw_reset(void)
{
    if(g_touch_pin_rst < 0) {
        return ESP_OK;
    }

    gpio_set_level(g_touch_pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(g_touch_pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static esp_err_t ft_probe(void)
{
    uint8_t td_status = 0;
    esp_err_t err = ft_read_regs(FT_REG_TD_STATUS, &td_status, 1);
    if(err != ESP_OK) {
        LOG("probe read TD_STATUS failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t chip_id = 0;
    err = ft_read_regs(FT_REG_ID_G_CHIPID, &chip_id, 1);
    if(err == ESP_OK) {
        // LOG("chip id: 0x%02X", chip_id);
    } else {
        LOG("read CHIPID failed: %s", esp_err_to_name(err));
    }

    uint8_t firm_id = 0;
    err = ft_read_regs(FT_REG_ID_G_FIRMID, &firm_id, 1);
    if(err == ESP_OK) {
        // LOG("firmware id: 0x%02X", firm_id);
    } else {
        LOG("read FIRMID failed: %s", esp_err_to_name(err));
    }

    uint8_t cipher = 0;
    err = ft_read_regs(FT_REG_ID_G_CIPHER, &cipher, 1);
    if(err == ESP_OK) {
        // LOG("cipher: 0x%02X", cipher);
    } else {
        LOG("read CIPHER failed: %s", esp_err_to_name(err));
    }

    uint8_t lib_ver[2] = {0};
    err = ft_read_regs(FT_REG_ID_G_LIB_VERSION_H, lib_ver, 2);
    if(err == ESP_OK) {
        uint16_t ver = ((uint16_t)lib_ver[0] << 8) | lib_ver[1];
        // LOG("lib version: 0x%04X", ver);
    } else {
        LOG("read LIB_VERSION failed: %s", esp_err_to_name(err));
    }

    // LOG("probe ok, TD_STATUS=0x%02X", td_status);
    return ESP_OK;
}

bool touch_ft6336u_read_point(touch_ft6336u_point_t *point)
{
    if(point == NULL || !s_inited) {
        return false;
    }

    point->pressed = false;
    point->x = 0;
    point->y = 0;

    uint8_t st = 0;
    if(ft_read_regs(FT_REG_TD_STATUS, &st, 1) != ESP_OK) {
        return false;
    }

    uint8_t touch_cnt = st & 0x0F;
    if(touch_cnt == 0) {
        return true;
    }

    uint8_t p1[4] = {0};
    if(ft_read_regs(FT_REG_P1_XH, p1, sizeof(p1)) != ESP_OK) {
        return false;
    }

    uint8_t event = (p1[0] >> 6) & 0x03;
    if(event != FT_TOUCH_EVT_DOWN && event != FT_TOUCH_EVT_CONTACT) {
        return true;
    }

    int16_t x = (int16_t)(((p1[0] & 0x0F) << 8) | p1[1]);
    int16_t y = (int16_t)(((p1[2] & 0x0F) << 8) | p1[3]);

    ft_transform_point(&x, &y);

    point->x = x;
    point->y = y;
    point->pressed = true;
    return true;
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);

    touch_ft6336u_point_t p = {0};
    if(!touch_ft6336u_read_point(&p)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if(p.pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = p.x;
        data->point.y = p.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

esp_err_t touch_ft6336u_init(void)
{
    if(s_inited) {
        return ESP_OK;
    }

    if(g_touch_pin_int >= 0) {
        gpio_config_t int_cfg = {
            .pin_bit_mask = 1ULL << g_touch_pin_int,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&int_cfg);
        if(err != ESP_OK) {
            LOG("config INT failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    if(g_touch_pin_rst >= 0) {
        gpio_config_t rst_cfg = {
            .pin_bit_mask = 1ULL << g_touch_pin_rst,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&rst_cfg);
        if(err != ESP_OK) {
            LOG("config RST failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = g_touch_pin_sda,
        .scl_io_num = g_touch_pin_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = FT_I2C_FREQ_HZ,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(FT_I2C_PORT, &conf);
    if(err != ESP_OK) {
        LOG("i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(FT_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LOG("i2c_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = ft_hw_reset();
    if(err != ESP_OK) {
        LOG("hw reset failed: %s", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    err = ft_probe();
    if(err != ESP_OK) {
        LOG("FT6336U probe failed at addr 0x%02X", FT6336U_ADDR);
        return err;
    }

    /* 普通工作模式 */
    err = ft_write_reg(FT_REG_ID_G_MODE, 0x00);
    if(err != ESP_OK) {
        LOG("set G_MODE failed: %s", esp_err_to_name(err));
    }

    s_inited = true;
    // LOG(
    //          "touch inited: addr=0x%02X sda=%d scl=%d int=%d rst=%d",
    //          FT6336U_ADDR, g_touch_pin_sda, g_touch_pin_scl, g_touch_pin_int, g_touch_pin_rst);

    return ESP_OK;
}

esp_err_t touch_ft6336u_deinit(void)
{
    if(!s_inited) {
        return ESP_OK;
    }

    i2c_driver_delete(FT_I2C_PORT);
    s_inited = false;
    s_indev = NULL;
    return ESP_OK;
}

void touch_ft6336u_set_transform(bool swap_xy, bool mirror_x, bool mirror_y)
{
    s_cfg.swap_xy = swap_xy;
    s_cfg.mirror_x = mirror_x;
    s_cfg.mirror_y = mirror_y;
}

void touch_ft6336u_set_offset(int16_t offset_x, int16_t offset_y)
{
    s_offset_x = offset_x;
    s_offset_y = offset_y;
}

void touch_ft6336u_set_resolution(int16_t h_res, int16_t v_res)
{
    if(h_res > 0) s_h_res = h_res;
    if(v_res > 0) s_v_res = v_res;
}

esp_err_t touch_ft6336u_set_orientation(uint8_t orientation, lv_display_t *disp)
{
    uint8_t ori = orientation & 0x03U;
    const touch_ft_orientation_cfg_t *cfg = &s_ori_cfg[ori];

    int16_t h = FT_TOUCH_H_RES;
    int16_t v = FT_TOUCH_V_RES;
    if(disp) {
        h = (int16_t)lv_display_get_horizontal_resolution(disp);
        v = (int16_t)lv_display_get_vertical_resolution(disp);
    }

    touch_ft6336u_set_resolution(h, v);
    touch_ft6336u_set_transform(cfg->swap_xy, cfg->mirror_x, cfg->mirror_y);
    touch_ft6336u_set_offset(cfg->offset_x, cfg->offset_y);
    // LOG("orientation=%u swap=%d mx=%d my=%d ofs=%d,%d res=%d,%d",
    //          (unsigned)ori, (int)cfg->swap_xy, (int)cfg->mirror_x, (int)cfg->mirror_y,
    //          (int)cfg->offset_x, (int)cfg->offset_y, (int)h, (int)v);
    return ESP_OK;
}

esp_err_t touch_ft6336u_lvgl_bind(lv_display_t *disp, lv_indev_t **out_indev)
{
    if(disp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = touch_ft6336u_init();
    if(err != ESP_OK) {
        return err;
    }

    lv_indev_t *indev = lv_indev_create();
    if(indev == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);
    lv_indev_set_display(indev, disp);

    s_indev = indev;
    if(out_indev) {
        *out_indev = indev;
    }

    return ESP_OK;
}
