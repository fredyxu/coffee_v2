#include "modules/ui/page/components/component_keyboard/component_keyboard.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "modules/ui/theme/color.h"
#include "modules/ui/theme/font.h"

#define UI_KEYBOARD_BTN(width) (LV_BUTTONMATRIX_CTRL_POPOVER | (width))

typedef struct {
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *textarea;
    lv_obj_t *keyboard;
    char *buffer;
    size_t buffer_size;
    ui_keyboard_event_cb_t on_event;
    void *user_data;
    uint32_t selected_button;
} ui_keyboard_ctx_t;

static const char * const s_kb_map_lower[] = {
    "1#", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "Del", "\n",
    "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l", "Enter", "\n",
    "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":", "\n",
    "关闭", "<-", "Space", "->", "OK", ""
};

static const lv_buttonmatrix_ctrl_t s_kb_ctrl_lower[] = {
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 5,
    UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4),
    UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4),
    LV_BUTTONMATRIX_CTRL_CHECKED | 7,
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6,
    UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3),
    UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3),
    LV_BUTTONMATRIX_CTRL_CHECKED | 7,
    LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1),
    LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1),
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
    LV_BUTTONMATRIX_CTRL_CHECKED | 2, 6, LV_BUTTONMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2
};

static const char * const s_kb_map_upper[] = {
    "1#", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "Del", "\n",
    "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L", "Enter", "\n",
    "_", "-", "Z", "X", "C", "V", "B", "N", "M", ".", ",", ":", "\n",
    "关闭", "<-", "Space", "->", "OK", ""
};

static const lv_buttonmatrix_ctrl_t s_kb_ctrl_upper[] = {
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 5,
    UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4),
    UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4), UI_KEYBOARD_BTN(4),
    LV_BUTTONMATRIX_CTRL_CHECKED | 7,
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6,
    UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3),
    UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3), UI_KEYBOARD_BTN(3),
    LV_BUTTONMATRIX_CTRL_CHECKED | 7,
    LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1), LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1),
    LV_BUTTONMATRIX_CTRL_CHECKED | UI_KEYBOARD_BTN(1),
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
    LV_BUTTONMATRIX_CTRL_CHECKED | 2, 6, LV_BUTTONMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2
};

static const char * const s_kb_map_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "Del", "\n",
    "abc", "+", "&", "/", "*", "=", "%", "!", "?", "#", "<", ">", "\n",
    "\\", "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'", "\n",
    "关闭", "<-", "Space", "->", "OK", ""
};

static const lv_buttonmatrix_ctrl_t s_kb_ctrl_special[] = {
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    LV_BUTTONMATRIX_CTRL_CHECKED | 2,
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1), UI_KEYBOARD_BTN(1),
    UI_KEYBOARD_BTN(1),
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2,
    LV_BUTTONMATRIX_CTRL_CHECKED | 2, 6, LV_BUTTONMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2
};

static ui_keyboard_ctx_t *keyboard_ctx_get(lv_obj_t *modal)
{
    if(modal == NULL) {
        return NULL;
    }

    return (ui_keyboard_ctx_t *)lv_obj_get_user_data(modal);
}

static void keyboard_sync_buffer(ui_keyboard_ctx_t *ctx)
{
    if(ctx == NULL || ctx->buffer == NULL || ctx->buffer_size == 0 || ctx->textarea == NULL) {
        return;
    }

    const char *text = lv_textarea_get_text(ctx->textarea);
    if(text == NULL) {
        ctx->buffer[0] = '\0';
        return;
    }

    (void)snprintf(ctx->buffer, ctx->buffer_size, "%s", text);
}

static const char *keyboard_current_text(ui_keyboard_ctx_t *ctx)
{
    if(ctx == NULL || ctx->textarea == NULL) {
        return "";
    }

    const char *text = lv_textarea_get_text(ctx->textarea);
    return text ? text : "";
}

static void keyboard_emit(ui_keyboard_ctx_t *ctx, ui_keyboard_event_t event)
{
    if(ctx == NULL) {
        return;
    }

    keyboard_sync_buffer(ctx);

    if(ctx->on_event != NULL) {
        ctx->on_event(event, keyboard_current_text(ctx), ctx->user_data);
    }
}

static uint32_t keyboard_button_count(lv_obj_t *keyboard)
{
    if(keyboard == NULL) {
        return 0;
    }

    const char * const *map = lv_keyboard_get_map_array(keyboard);
    if(map == NULL) {
        return 0;
    }

    uint32_t count = 0;
    for(size_t i = 0; map[i] != NULL && map[i][0] != '\0'; i++) {
        if(strcmp(map[i], "\n") != 0) {
            count++;
        }
    }

    return count;
}

static void keyboard_apply_selection_style(lv_obj_t *keyboard)
{
    if(keyboard == NULL) {
        return;
    }

    lv_obj_set_style_text_font(keyboard, UI_FONT_14, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, UI_COLOR_BUTTON_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, UI_COLOR_BUTTON, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 4, LV_PART_ITEMS);
    lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(keyboard, UI_COLOR_BORDER, LV_PART_ITEMS);

    lv_obj_set_style_bg_color(keyboard, UI_COLOR_ACCENT, LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(keyboard, UI_COLOR_TEXT_DARK, LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(keyboard, 2, LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(keyboard, UI_COLOR_TEXT, LV_PART_ITEMS | LV_STATE_FOCUSED);

    lv_obj_set_style_bg_color(keyboard, UI_COLOR_BUTTON_PRESSED, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(keyboard, UI_COLOR_BUTTON_TEXT, LV_PART_ITEMS | LV_STATE_PRESSED);
}

static void keyboard_apply_ascii_maps(lv_obj_t *keyboard)
{
    if(keyboard == NULL) {
        return;
    }

    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, s_kb_map_lower, s_kb_ctrl_lower);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, s_kb_map_upper, s_kb_ctrl_upper);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_SPECIAL, s_kb_map_special, s_kb_ctrl_special);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
}

static void keyboard_select_button(ui_keyboard_ctx_t *ctx, uint32_t button)
{
    if(ctx == NULL || ctx->keyboard == NULL) {
        return;
    }

    uint32_t count = keyboard_button_count(ctx->keyboard);
    if(count == 0) {
        ctx->selected_button = LV_BUTTONMATRIX_BUTTON_NONE;
        lv_buttonmatrix_set_selected_button(ctx->keyboard, LV_BUTTONMATRIX_BUTTON_NONE);
        return;
    }

    if(button >= count) {
        button = count - 1;
    }

    ctx->selected_button = button;
    lv_buttonmatrix_set_selected_button(ctx->keyboard, button);
    lv_obj_add_state(ctx->keyboard, LV_STATE_FOCUSED);
    lv_obj_invalidate(ctx->keyboard);
}

static void keyboard_move_selection(ui_keyboard_ctx_t *ctx, int step)
{
    if(ctx == NULL || ctx->keyboard == NULL) {
        return;
    }

    uint32_t count = keyboard_button_count(ctx->keyboard);
    if(count == 0) {
        return;
    }

    int next = (ctx->selected_button == LV_BUTTONMATRIX_BUTTON_NONE) ? 0 : (int)ctx->selected_button + step;
    if(next < 0) {
        next = (int)count - 1;
    } else if(next >= (int)count) {
        next = 0;
    }

    keyboard_select_button(ctx, (uint32_t)next);
}

static void keyboard_press_selected(ui_keyboard_ctx_t *ctx)
{
    if(ctx == NULL || ctx->keyboard == NULL || ctx->selected_button == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }

    lv_buttonmatrix_set_selected_button(ctx->keyboard, ctx->selected_button);
    (void)lv_obj_send_event(ctx->keyboard, LV_EVENT_VALUE_CHANGED, NULL);
}

static void keyboard_set_mode(ui_keyboard_ctx_t *ctx, lv_keyboard_mode_t mode)
{
    if(ctx == NULL || ctx->keyboard == NULL) {
        return;
    }

    lv_keyboard_set_mode(ctx->keyboard, mode);
    keyboard_select_button(ctx, 0);
}

static void keyboard_submit(ui_keyboard_ctx_t *ctx)
{
    if(ctx == NULL) {
        return;
    }

    keyboard_emit(ctx, UI_KEYBOARD_EVT_SUBMIT);
    ui_keyboard_modal_close(ctx->root);
}

static void keyboard_cancel(ui_keyboard_ctx_t *ctx)
{
    if(ctx == NULL) {
        return;
    }

    keyboard_emit(ctx, UI_KEYBOARD_EVT_CANCEL);
    ui_keyboard_modal_close(ctx->root);
}

static void keyboard_handle_button(ui_keyboard_ctx_t *ctx)
{
    if(ctx == NULL || ctx->keyboard == NULL || ctx->textarea == NULL) {
        return;
    }

    uint32_t button = lv_keyboard_get_selected_button(ctx->keyboard);
    if(button == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }

    const char *text = lv_keyboard_get_button_text(ctx->keyboard, button);
    if(text == NULL) {
        return;
    }

    if(strcmp(text, "abc") == 0) {
        keyboard_set_mode(ctx, LV_KEYBOARD_MODE_TEXT_LOWER);
        return;
    }

    if(strcmp(text, "ABC") == 0) {
        keyboard_set_mode(ctx, LV_KEYBOARD_MODE_TEXT_UPPER);
        return;
    }

    if(strcmp(text, "1#") == 0) {
        keyboard_set_mode(ctx, LV_KEYBOARD_MODE_SPECIAL);
        return;
    }

    if(strcmp(text, "关闭") == 0) {
        keyboard_cancel(ctx);
        return;
    }

    if(strcmp(text, "OK") == 0) {
        keyboard_submit(ctx);
        return;
    }

    if(strcmp(text, "Del") == 0) {
        lv_textarea_delete_char(ctx->textarea);
        return;
    }

    if(strcmp(text, "<-") == 0) {
        lv_textarea_cursor_left(ctx->textarea);
        return;
    }

    if(strcmp(text, "->") == 0) {
        lv_textarea_cursor_right(ctx->textarea);
        return;
    }

    if(strcmp(text, "Space") == 0) {
        lv_textarea_add_char(ctx->textarea, ' ');
        return;
    }

    if(strcmp(text, "Enter") == 0) {
        if(lv_textarea_get_one_line(ctx->textarea)) {
            keyboard_submit(ctx);
            return;
        }

        lv_textarea_add_char(ctx->textarea, '\n');
        return;
    }

    lv_textarea_add_text(ctx->textarea, text);
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_current_target(e);
    ui_keyboard_ctx_t *ctx = (ui_keyboard_ctx_t *)lv_event_get_user_data(e);
    if(ctx == NULL) {
        return;
    }

    if(code == LV_EVENT_VALUE_CHANGED) {
        if(target == ctx->keyboard) {
            keyboard_handle_button(ctx);
            return;
        }

        keyboard_emit(ctx, UI_KEYBOARD_EVT_CHANGED);
        return;
    }

    if(code == LV_EVENT_READY) {
        keyboard_submit(ctx);
        return;
    }

    if(code == LV_EVENT_CANCEL) {
        keyboard_cancel(ctx);
        return;
    }

    if(code == LV_EVENT_DELETE && target == ctx->root) {
        lv_free(ctx);
    }
}

static lv_obj_t *keyboard_create_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, UI_FONT_14, 0);
    return label;
}

lv_obj_t *ui_keyboard_modal_create(const ui_keyboard_config_t *cfg)
{
    if(cfg == NULL || cfg->parent == NULL) {
        return NULL;
    }

    ui_keyboard_ctx_t *ctx = lv_malloc(sizeof(ui_keyboard_ctx_t));
    if(ctx == NULL) {
        return NULL;
    }
    lv_memzero(ctx, sizeof(*ctx));

    ctx->buffer = cfg->buffer;
    ctx->buffer_size = cfg->buffer_size;
    ctx->on_event = cfg->on_event;
    ctx->user_data = cfg->user_data;
    ctx->selected_button = 0;

    lv_obj_t *parent = lv_layer_top();
    if(parent == NULL) {
        parent = cfg->parent;
    }

    lv_obj_t *root = lv_obj_create(parent);
    ctx->root = root;
    lv_obj_set_user_data(root, ctx);
    lv_obj_add_event_cb(root, keyboard_event_cb, LV_EVENT_DELETE, ctx);
    lv_obj_add_flag(root, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_60, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *panel = lv_obj_create(root);
    ctx->panel = panel;
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, LV_PCT(82));
    lv_obj_set_style_bg_color(panel, UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    (void)keyboard_create_label(panel, cfg->title ? cfg->title : "Input");

    lv_obj_t *ta = lv_textarea_create(panel);
    ctx->textarea = ta;
    lv_obj_set_width(ta, LV_PCT(95));
    lv_obj_set_height(ta, 42);
    lv_obj_set_style_text_font(ta, UI_FONT_14, 0);
    lv_obj_set_style_text_color(ta, UI_COLOR_TEXT_DARK, 0);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, cfg->password_mode);
    lv_textarea_set_placeholder_text(ta, cfg->placeholder ? cfg->placeholder : "");
    if(cfg->buffer != NULL && cfg->buffer[0] != '\0') {
        lv_textarea_set_text(ta, cfg->buffer);
    }
    if(cfg->buffer_size > 0) {
        lv_textarea_set_max_length(ta, (uint32_t)cfg->buffer_size - 1u);
    }
    lv_obj_add_event_cb(ta, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, ctx);

    lv_obj_t *kb = lv_keyboard_create(panel);
    ctx->keyboard = kb;
    lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
    lv_keyboard_set_textarea(kb, ta);
    keyboard_apply_ascii_maps(kb);
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_flex_grow(kb, 1);
    keyboard_apply_selection_style(kb);
    lv_obj_add_state(kb, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(kb, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(kb, keyboard_event_cb, LV_EVENT_READY, ctx);
    lv_obj_add_event_cb(kb, keyboard_event_cb, LV_EVENT_CANCEL, ctx);
    keyboard_select_button(ctx, 0);

    return root;
}

void ui_keyboard_modal_close(lv_obj_t *modal)
{
    if(modal == NULL) {
        return;
    }

    lv_obj_delete(modal);
}

const char *ui_keyboard_get_text(lv_obj_t *modal)
{
    ui_keyboard_ctx_t *ctx = keyboard_ctx_get(modal);
    return keyboard_current_text(ctx);
}

void ui_keyboard_set_text(lv_obj_t *modal, const char *text)
{
    ui_keyboard_ctx_t *ctx = keyboard_ctx_get(modal);
    if(ctx == NULL || ctx->textarea == NULL) {
        return;
    }

    lv_textarea_set_text(ctx->textarea, text ? text : "");
    keyboard_sync_buffer(ctx);
}

void ui_keyboard_handle_input(lv_obj_t *modal, const msg_t *msg)
{
    ui_keyboard_ctx_t *ctx = keyboard_ctx_get(modal);
    if(ctx == NULL || msg == NULL || msg->type != MSG_TYPE_INPUT) {
        return;
    }

    switch(msg->event) {
        case MSG_EVT_INPUT_ENCODER_CW:
            keyboard_move_selection(ctx, +1);
            break;

        case MSG_EVT_INPUT_ENCODER_CCW:
            keyboard_move_selection(ctx, -1);
            break;

        case MSG_EVT_INPUT_ENCODER_PRESS:
            keyboard_press_selected(ctx);
            break;

        case MSG_EVT_INPUT_ENCODER_LONG_PRESS:
            keyboard_emit(ctx, UI_KEYBOARD_EVT_CANCEL);
            ui_keyboard_modal_close(ctx->root);
            break;

        default:
            break;
    }
}
