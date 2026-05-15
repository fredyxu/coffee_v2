#include "page_settings_item.h"

#include <stdbool.h>

#include "modules/ui/page/components/components_top_status/component_top_status.h"
#include "modules/ui/page/page_settings/page_settings_data.h"
#include "modules/ui/page/page_settings/page_settings_item_style.h"
#include "modules/ui/style/ui_style.h"

#define PAGE_SETTINGS_ITEM_FOCUS_MAX 32

static page_settings_item_focus_item_t s_focus[PAGE_SETTINGS_ITEM_FOCUS_MAX];
static size_t s_focus_count = 0;
static int s_focus_index = -1;



static op_status_type_t s_op_status = OP_MENU;

static void focus_add(lv_obj_t *obj, void *data, void (*on_enter)(void *data)) {
	if (s_focus_count >= PAGE_SETTINGS_ITEM_FOCUS_MAX) {
		return;
	}
	if(obj == NULL || s_focus_count >= PAGE_SETTINGS_ITEM_FOCUS_MAX) {
        return;
    }
	s_focus[s_focus_count++] = (page_settings_item_focus_item_t) {
        .obj = obj,
        .data = data,
        .on_enter = on_enter,
    };

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

// 列表
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

// 字符类型项目
static void insert_settings_item_text(const settings_sub_item_t *item) {
    lv_obj_t *obj_body = lv_obj_create(page_item_in_body);

    page_settings_item_apply_style_page_item_text(obj_body);
}

// 布尔开关类型
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

// 插入选项
static void insert_settings_items() {
    sub_items = page_settings_get_sub_items(SETTINGS_ITEM_ID_WIFI, &sub_items_count);
    for (size_t i = 0; i < sub_items_count; i++) {
        if (sub_items[i].value_type == SETTINGS_VALUE_TYPE_TEXT) {
            insert_settings_item_text(&sub_items[i]);
        } else if (sub_items[i].value_type == SETTINGS_VALUE_TYPE_BOOL) {
            insert_settings_item_bool(&sub_items[i]);
        } else if(sub_items[i].value_type == SETTINGS_VALUE_TYPE_LIST) {
            insert_settings_item_list(&sub_items[i]);
        } 
    }
}

static void create_page() {
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
    ui_add_top_status(page_body);
    create_page();
}
