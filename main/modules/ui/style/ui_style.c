#include "ui_style.h"

#include "config/config_sys.h"
#include "modules/ui/theme/color.h"

static bool style_init_done = false;

lv_style_t style_page_body;
static lv_style_t style_line_1;

void page_body_style_init(void) {
    lv_style_init(&style_page_body);
    lv_style_set_size(&style_page_body, DISPLAY_H_RES, DISPLAY_V_RES);
    lv_style_set_layout(&style_page_body, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_page_body, LV_FLEX_FLOW_COLUMN);
    // 设置对齐方式
    // 参数 1: 主轴对齐 (justify-content) -> flex-start
    // 参数 2: 交叉轴对齐 (align-items) -> center
    // 参数 3: 轨道对齐 (多行/列分布) -> center (默认即可)
    lv_style_set_pad_all(&style_page_body, 0);
    lv_style_set_margin_all(&style_page_body, 0);
    lv_style_set_flex_main_place(&style_page_body, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&style_page_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&style_page_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_bg_color(&style_page_body, UI_COLOR_BG);

    lv_style_set_border_width(&style_page_body, 0);
    lv_style_set_outline_width(&style_page_body, 0);
    lv_style_set_shadow_width(&style_page_body, 0);
    lv_style_set_radius(&style_page_body, 0);
    // lv_style_set_pad_all(&style_page_body, 0);
    lv_style_set_margin_all(&style_page_body, 0);
    // lv_style_set_bg_opa(&style_page_body, LV_OPA_COVER);
    lv_style_set_clip_corner(&style_page_body, true);


	// 分割线
    lv_style_init(&style_line_1);
    lv_style_set_size(&style_line_1, LV_PCT(100), 1);
    lv_style_set_pad_all(&style_line_1, 0);
    lv_style_set_margin_all(&style_line_1, 0);
    lv_style_set_bg_color(&style_line_1, UI_COLOR_BORDER);
	lv_style_set_border_width(&style_line_1, 0);


}

void ui_style_init(void) {
    if (style_init_done) {
        return;
    }

    (void)page_body_style_init();

    style_init_done = true;
}

static void ui_style_init_common(lv_style_t *s) {
    lv_style_init(s);
    lv_style_set_margin_all(s, 0);
    lv_style_set_pad_all(s, 0);
    lv_style_set_bg_color(s, UI_COLOR_BG);
    lv_style_set_radius(s, 0);
    lv_style_set_border_width(s, 0);

    lv_style_set_layout(s, LV_LAYOUT_FLEX);
}

void ui_style_init_row(lv_style_t *s) {
    ui_style_init_common(s);
    lv_style_set_flex_flow(s, LV_FLEX_FLOW_ROW);
}

void ui_style_init_column(lv_style_t *s) {
    ui_style_init_common(s);
    lv_style_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
}

void ui_style_insert_line_1(lv_obj_t *obj) {
    lv_obj_t *line = lv_obj_create(obj);
    lv_obj_add_style(line, &style_line_1, 0);
}
