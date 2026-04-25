#include "page_home.h"
#include "lvgl.h"

#include "ui/theme/color.h"
#include "ui/theme/font.h"
#include "ui/page/components/components_top_status/component_top_status.h"

#include "config/config_sys.h"
#include "core/utils/log.h"
#include "modules/ui/ui.h"

static lv_obj_t * home_body;

// static esp_err_t add_home_msg(const char * txt) {
//     if(s == NULL || txt == NULL) {
//         return ESP_ERR_INVALID_STATE;
//     }

//     lv_obj_t * label = lv_label_create(s);
//     lv_label_set_text(label, txt);
//     lv_obj_set_width(label, LV_PCT(100)); // 宽度铺满，支持自动换行
//     lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
//     lv_obj_set_style_text_font(label, UI_FONT_14, 0);
    
//     lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);

//     // 更新布局
//     // 在滚动之前必须强制刷新布局
//     lv_obj_update_layout(s);

//     // 滚动到底部
//     // 参数含义：对象, 滚动像素偏移(此处设为极大值), 是否使用动画
//     lv_obj_scroll_to_y(s, lv_obj_get_scroll_bottom(s), LV_ANIM_ON);
    
//     // 或者使用此方法：让最后一个子对象可见
//     // lv_obj_scroll_to_view(label, LV_ANIM_ON);
//     return ESP_OK;
// }

// static void create_auto_scroll_view(lv_obj_t *p) {
//     // 创建背景容器（类似你之前的 UI_COLOR_BG）
//     s = lv_obj_create(p);
//     lv_obj_set_size(s, DISPLAY_H_RES - 20, DISPLAY_V_RES - 20); // 设置固定大小
//     lv_obj_center(s);
    
//     // 设置背景颜色
//     lv_obj_set_style_bg_color(s, UI_COLOR_BG, 0);
//     lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
//     lv_obj_set_style_border_color(s, UI_COLOR_TEXT_1, 0);
//     lv_obj_set_style_border_width(s, 0, 0);
//     lv_obj_set_style_radius(s, 0, 0);

//     // 设置布局：从上到下排列子对象
//     lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
//     lv_obj_set_flex_align(s, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

//     // 移除不必要的内边距（可选，增加显示面积）
//     lv_obj_set_style_pad_all(s, 5, 0);
//     lv_obj_set_style_pad_row(s, 5, 0); // 每行之间的间距

//     // 隐藏水平滚动条，只保留垂直滚动
//     lv_obj_set_scrollbar_mode(s, LV_SCROLLBAR_MODE_AUTO);
// }

esp_err_t page_home_show(lv_obj_t *p) {
	// LOG("初始化页面显示");
    if(p == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
	if(ui_page_init(p, &home_body) != ESP_OK || home_body == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    lv_obj_set_style_bg_color(home_body, UI_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home_body, LV_OPA_COVER, LV_PART_MAIN);

	ui_add_top_status(home_body);


    return ESP_OK;
}
