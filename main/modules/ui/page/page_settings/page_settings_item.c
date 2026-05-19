#include "page_settings_item.h"

#include <stdbool.h>
#include "modules/ui/page/components/components_top_status/component_top_status.h"
#include "modules/ui/page/page_settings/page_settings_data.h"
#include "modules/ui/page/page_settings/page_settings_item_style.h"
#include "modules/ui/style/ui_style.h"
#include "core/utils/log.h"
#include "modules/ui/ui_actor.h"
#include "core/msg/msg.h"


#define PAGE_SETTINGS_ITEM_FOCUS_MAX 32


static page_settings_item_focus_item_t s_focus[PAGE_SETTINGS_ITEM_FOCUS_MAX];
static size_t s_focus_count = 0;
static int s_focus_index = -1;
static op_status_type_t current_status = OP_MENU;


void static input_handler(const msg_t *msg) {
	switch(msg->type) {
	case MSG_TYPE_INPUT:
		switch(msg->event) {		
			case MSG_EVT_INPUT_ENCODER_CW:
				LOG("输入事件: 编码器顺时针");
				break;
			case MSG_EVT_INPUT_ENCODER_CCW:
				LOG("输入事件: 编码器逆时针");
				break;
			case MSG_EVT_INPUT_ENCODER_PRESS:
				LOG("输入事件: 编码器按下");
				break;
			case MSG_EVT_INPUT_ENCODER_LONG_PRESS:
				LOG("输入事件: 编码器长按");
				break;
			default:
				LOG("输入事件: 未处理的事件类型 %d", msg->event);
				break;
		}

		break;
	case MSG_TYPE_CMD:
		break;
	case MSG_TYPE_SYS:
		break;
	}
}

static const ui_page_ops_t page_settings_item_ops = {
	.on_input = input_handler,
};




static op_status_type_t s_op_status = OP_MENU;

static void focus_add(lv_obj_t *obj, void *data, void (*on_enter)(void *data)) {
	if (s_focus_count >= PAGE_SETTINGS_ITEM_FOCUS_MAX) {
		return;
	}
	if(obj == NULL || s_focus_count >= PAGE_SETTINGS_ITEM_FOCUS_MAX) {
        return;
    }
	// s_focus[s_focus_count++] = (page_settings_item_focus_item_t) {
    //     .obj = obj,
    //     .data = data,
    //     .on_enter = on_enter,
    // };

    if(s_focus_index < 0) {
        s_focus_index = 0;
        lv_obj_add_state(obj, LV_STATE_FOCUSED);
    }
}

static lv_obj_t *page_body;
static lv_obj_t *page_item_body;
// 选项内框
static lv_obj_t *page_item_in_body;

static const settings_item_t *setting_item;

static size_t sub_items_count = 0;
static const settings_sub_item_t *sub_items;

// **************************************************
// 拆入列表样式
// **************************************************
static void insert_settings_item_list(const settings_sub_item_t *item) {
	for(size_t i = 0; i < *item->value_count; i++) {
		settings_value_list_t *value_info = (settings_value_list_t *)item->value_list + i;
		// printf("Value: %s, Int: %d\n", value_info->title, value_info->value_int);

		lv_obj_t *obj_body = lv_obj_create(page_item_in_body);
		lv_obj_t *obj_title_label = lv_label_create(obj_body);
		lv_label_set_text(obj_title_label, value_info->title);

		page_settings_item_apply_style_page_item_list(
			obj_body, 
			obj_title_label
		);
	}
	ui_style_insert_line_1(page_item_in_body);
}

// **************************************************
// 字符样式
// **************************************************
static void insert_settings_item_text(const settings_sub_item_t *item) {
    lv_obj_t *obj_body = lv_obj_create(page_item_in_body);

    page_settings_item_apply_style_page_item_text(obj_body);
}

// **************************************************
// 布尔（开关）类型项目
// **************************************************
static void insert_settings_item_bool(const settings_sub_item_t *item) {
    lv_obj_t *obj_body = lv_obj_create(page_item_in_body);
    lv_obj_t *obj_title_body = lv_obj_create(obj_body);
    lv_obj_t *obj_title_label = lv_label_create(obj_title_body);
    lv_label_set_text(obj_title_label, item->title);
    lv_obj_t *obj_switch = lv_switch_create(obj_body);
    if (item->value) {
        lv_obj_add_state(obj_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(obj_switch, LV_STATE_CHECKED);
    }

    page_settings_item_apply_style_page_item_bool(obj_body, obj_title_body, obj_title_label,
                                                  obj_switch);
    ui_style_insert_line_1(page_item_in_body);
}


// **************************************************
// 数字项目 滑动
// **************************************************
static void insert_settings_item_int(const settings_sub_item_t *item) {
	lv_obj_t *obj_body = lv_obj_create(page_item_in_body);
	lv_obj_t *obj_title_body = lv_obj_create(obj_body);
	lv_obj_t *obj_title_label = lv_label_create(obj_title_body);
	lv_obj_t *obj_slider_body = lv_obj_create(obj_body);
	lv_obj_t *obj_slider = lv_slider_create(obj_slider_body);
	lv_slider_set_range(obj_slider, item->min_value, item->max_value);
	lv_slider_set_value(obj_slider, *(int *)item->value, LV_ANIM_OFF);


	lv_label_set_text(obj_title_label, item->title);

	page_settings_item_apply_style_page_item_int(
		obj_body, 
		obj_title_body, 
		obj_title_label, 
		obj_slider_body,
		obj_slider
	);
}

// **************************************************
// 插入项目
// **************************************************
static void insert_settings_items() {
    sub_items = page_settings_get_sub_items(SETTINGS_ITEM_ID_WIFI, &sub_items_count);
    for (size_t i = 0; i < sub_items_count; i++) {
		switch (sub_items[i].value_type) {
			// 插入文本项
			case SETTINGS_VALUE_TYPE_TEXT:
				insert_settings_item_text(&sub_items[i]);
				break;
			// 插入布尔项
			case SETTINGS_VALUE_TYPE_BOOL:
				insert_settings_item_bool(&sub_items[i]);
				break;
			// 插入列表项
			case SETTINGS_VALUE_TYPE_LIST:
				insert_settings_item_list(&sub_items[i]);
				break;
			case SETTINGS_VALUE_TYPE_INT:
				insert_settings_item_int(&sub_items[i]);
				break;
			default:
				LOG("未知的项目类型: %d", sub_items[i].value_type);
				break;
		}
	}
		
}

static void create_page() {
	// 标题
    lv_obj_t *title_body = lv_obj_create(page_body);
    page_settings_item_apply_style_page_title_body(title_body);

    lv_obj_t *title_label = lv_label_create(title_body);
    page_settings_item_apply_style_page_title_label(title_label);
    lv_label_set_text(title_label, setting_item->title);

    // lv_obj_t *line_1 = lv_obj_create(page_body);
    // lv_obj_add_style(line_1, &style_line_1, 0);

    page_item_body = lv_obj_create(page_body);
    page_settings_item_apply_style_page_item_body(page_item_body);
    // lv_obj_add_style(page_item_body, &style_page_item_body, 0);

    // 选项内框
    page_item_in_body = lv_obj_create(page_item_body);
    page_settings_item_apply_style_page_item_in_body(page_item_in_body);
    // lv_obj_add_style(page_item_in_body, &style_page_item_in_body, 0);

    // 插入选项
    insert_settings_items();
}

void page_settings_item_show(lv_obj_t *p, settings_item_id_t id) {
    if (p == NULL) {
        return;
    }
    setting_item = page_settings_find_item(id);
    if (setting_item == NULL) {
        return;
    }
    page_settings_item_style_init();
    page_body = lv_obj_create(p);
    lv_obj_add_style(page_body, &style_page_body, 0);
	// lv_obj_set_style_bg_color(page_body, UI_COLOR_ACCENT, 0);
    ui_add_top_status(page_body);
    create_page();

	ui_actor_set_ops(&page_settings_item_ops);
}
