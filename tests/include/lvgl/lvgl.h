#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

struct lv_obj_t { int id = 0; };
struct lv_color_t { uint32_t v; };
struct lv_font_t { int size; };
struct lv_group_t { int id; };
struct lv_indev_t { int id; };
struct lv_point_t { int32_t x; int32_t y; };
struct lv_event_t { int code = 0; uint32_t key = 0; void *user_data = nullptr; };
struct lv_timer_t { void *user_data = nullptr; };

enum {
    LV_PART_MAIN = 1, LV_STATE_DEFAULT = 2, LV_OPA_COVER = 255, LV_OPA_TRANSP = 0,
    LV_OBJ_FLAG_SCROLLABLE = 1, LV_OBJ_FLAG_CLICKABLE = 2,
    LV_OBJ_FLAG_CLICK_FOCUSABLE = 4, LV_LABEL_LONG_SCROLL_CIRCULAR = 8,
    LV_LABEL_LONG_WRAP = 9, LV_OPA_80 = 204, LV_OPA_90 = 229, LV_ALIGN_CENTER = 5, LV_ALIGN_BOTTOM_RIGHT = 9,
    LV_EVENT_KEY = 16, LV_EVENT_SCROLL = 17, LV_KEY_ENTER = 10, LV_KEY_BACKSPACE = 127,
    LV_KEY_ESC = 27,
    LV_KEY_DEL = 255, LV_KEY_HOME = 254, LV_DIR_VER = 2, LV_SCROLLBAR_MODE_AUTO = 1, LV_SIZE_CONTENT = -1, LV_ANIM_ON = 1, LV_ANIM_OFF = 0, LV_KEY_LEFT = 253, LV_KEY_RIGHT = 252, LV_KEY_UP = 251, LV_KEY_DOWN = 250, LV_TEXT_FLAG_NONE = 0, LV_TEXT_ALIGN_RIGHT = 4, LV_COORD_MAX = 0x7fffffff
};
using lv_event_cb_t = void(*)(lv_event_t*);
using lv_style_selector_t = int;
using lv_coord_t = int32_t;
using lv_align_t = int;

extern lv_obj_t g_ui_label_a;
extern lv_obj_t g_ui_label_b;
extern std::unordered_map<int, std::string> g_ui_text;
extern lv_event_cb_t g_ui_event_cb;
extern void *g_ui_event_user_data;

lv_color_t lv_color_hex(uint32_t);
lv_obj_t *lv_obj_create(lv_obj_t *);
lv_obj_t *lv_label_create(lv_obj_t *);
void reset_test_label_count();
void lv_obj_remove_style_all(lv_obj_t *);
void lv_obj_set_size(lv_obj_t *, lv_coord_t, lv_coord_t);
void lv_obj_set_pos(lv_obj_t *, lv_coord_t, lv_coord_t);
void lv_obj_set_style_bg_color(lv_obj_t *, lv_color_t, lv_style_selector_t);
void lv_obj_set_style_bg_opa(lv_obj_t *, int, lv_style_selector_t);
void lv_obj_set_style_radius(lv_obj_t *, lv_coord_t, lv_style_selector_t);
void lv_obj_set_style_border_color(lv_obj_t *, lv_color_t, lv_style_selector_t);
void lv_obj_set_style_border_width(lv_obj_t *, lv_coord_t, lv_style_selector_t);
void lv_obj_center(lv_obj_t *);
void lv_obj_clear_flag(lv_obj_t *, uint32_t);
void lv_obj_add_flag(lv_obj_t *, uint32_t);
void lv_obj_delete(lv_obj_t *);
void lv_obj_set_width(lv_obj_t *, lv_coord_t);
void lv_obj_set_height(lv_obj_t *, lv_coord_t);
void lv_obj_set_scroll_dir(lv_obj_t *, int);
void lv_obj_set_scrollbar_mode(lv_obj_t *, int);
void lv_obj_scroll_by(lv_obj_t *, lv_coord_t, lv_coord_t, int);
void lv_obj_scroll_by_bounded(lv_obj_t *, lv_coord_t, lv_coord_t, int);
int32_t lv_obj_get_scroll_bottom(const lv_obj_t *);
int32_t lv_obj_get_scroll_top(const lv_obj_t *);
lv_timer_t *lv_timer_create(void (*)(lv_timer_t *), uint32_t, void *);
void *lv_timer_get_user_data(lv_timer_t *);
void lv_timer_delete(lv_timer_t *);
void lv_obj_align(lv_obj_t *, lv_align_t, lv_coord_t, lv_coord_t);
void lv_obj_set_style_pad_all(lv_obj_t *, lv_coord_t, lv_style_selector_t);
void lv_obj_set_style_text_line_space(lv_obj_t *, lv_coord_t, lv_style_selector_t);
void lv_obj_set_style_text_font(lv_obj_t *, const lv_font_t *, lv_style_selector_t);
void lv_obj_set_style_text_color(lv_obj_t *, lv_color_t, lv_style_selector_t);
void lv_obj_set_style_text_letter_space(lv_obj_t *, int, lv_style_selector_t);
void lv_obj_set_style_text_align(lv_obj_t *, int, lv_style_selector_t);
void lv_obj_set_style_anim_duration(lv_obj_t *, uint32_t, lv_style_selector_t);
void lv_label_set_long_mode(lv_obj_t *, int);
void lv_label_set_text(lv_obj_t *, const char *);
void lv_obj_add_event_cb(lv_obj_t *, lv_event_cb_t, int, void *);
lv_group_t *lv_group_create();
void lv_group_add_obj(lv_group_t *, lv_obj_t *);
void lv_group_focus_obj(lv_obj_t *);
void lv_indev_set_group(lv_indev_t *, lv_group_t *);
void lv_group_delete(lv_group_t *);
const lv_font_t *lv_obj_get_style_text_font(const lv_obj_t *, lv_style_selector_t);
int32_t lv_obj_get_content_width(const lv_obj_t *);
void lv_text_get_size(lv_point_t *, const char *, const lv_font_t *, int32_t, int32_t, lv_coord_t, uint32_t);
int lv_event_get_code(const lv_event_t *);
uint32_t lv_event_get_key(const lv_event_t *);
void *lv_event_get_user_data(const lv_event_t *);
