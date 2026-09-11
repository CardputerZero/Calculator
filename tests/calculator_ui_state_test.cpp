#include "lvgl/lvgl.h"
#include <cstdio>
#include <string>
#include <unordered_map>

lv_obj_t g_ui_label_a{1};
lv_obj_t g_ui_label_b{2};
std::unordered_map<int, std::string> g_ui_text{{1, ""}, {2, ""}};
lv_event_cb_t g_ui_event_cb = nullptr;
void *g_ui_event_user_data = nullptr;
static int g_next_id = 100;
static lv_font_t font20{20};
static lv_font_t font24{24};
extern const lv_font_t jetbrains_mono_14;
extern const lv_font_t jetbrains_mono_20;
extern const lv_font_t jetbrains_mono_24;
const lv_font_t jetbrains_mono_14{14};
const lv_font_t jetbrains_mono_20{20};
const lv_font_t jetbrains_mono_24{24};
extern const lv_font_t lv_font_montserrat_14;
extern const lv_font_t montserrat_bold_14;
const lv_font_t lv_font_montserrat_14{14};
const lv_font_t montserrat_bold_14{14};

static int g_label_count = 0;
lv_color_t lv_color_hex(uint32_t value) { return {value}; }
lv_obj_t *lv_obj_create(lv_obj_t *) { return new lv_obj_t{++g_next_id}; }
void reset_test_label_count() { g_label_count = 0; }
lv_obj_t *lv_label_create(lv_obj_t *) {
    ++g_label_count;
    if (g_label_count == 1) return &g_ui_label_a;
    if (g_label_count == 2) return &g_ui_label_b;
    return new lv_obj_t{++g_next_id};
}
void lv_obj_remove_style_all(lv_obj_t *) {}
void lv_obj_set_size(lv_obj_t *, lv_coord_t, lv_coord_t) {}
void lv_obj_set_pos(lv_obj_t *, lv_coord_t, lv_coord_t) {}
void lv_obj_set_style_bg_color(lv_obj_t *, lv_color_t, lv_style_selector_t) {}
void lv_obj_set_style_bg_opa(lv_obj_t *, int, lv_style_selector_t) {}
void lv_obj_set_style_radius(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_set_style_border_color(lv_obj_t *, lv_color_t, lv_style_selector_t) {}
void lv_obj_set_style_border_width(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_set_style_pad_all(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_set_style_pad_left(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_set_style_pad_right(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_set_style_pad_top(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_set_style_pad_bottom(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_set_style_text_line_space(lv_obj_t *, lv_coord_t, lv_style_selector_t) {}
void lv_obj_center(lv_obj_t *) {}
void lv_obj_clear_flag(lv_obj_t *, uint32_t) {}
void lv_obj_add_flag(lv_obj_t *, uint32_t) {}
void lv_obj_set_width(lv_obj_t *, lv_coord_t) {}
void lv_obj_set_height(lv_obj_t *, lv_coord_t) {}
void lv_obj_set_scroll_dir(lv_obj_t *, int) {}
void lv_obj_set_scrollbar_mode(lv_obj_t *, int) {}
void lv_obj_scroll_by(lv_obj_t *, lv_coord_t, lv_coord_t, int) {}
void lv_obj_scroll_by_bounded(lv_obj_t *, lv_coord_t, lv_coord_t, int) {}
int32_t lv_obj_get_scroll_bottom(const lv_obj_t *) { return 0; }
int32_t lv_obj_get_scroll_top(const lv_obj_t *) { return 0; }
lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), uint32_t, void *user_data) { (void)callback; return new lv_timer_t{user_data}; }
void *lv_timer_get_user_data(lv_timer_t *timer) { return timer->user_data; }
void lv_timer_delete(lv_timer_t *timer) { delete timer; }
void lv_obj_align(lv_obj_t *, lv_align_t, lv_coord_t, lv_coord_t) {}
void lv_obj_delete(lv_obj_t *) {}
void lv_obj_set_style_text_font(lv_obj_t *, const lv_font_t *, lv_style_selector_t) {}
void lv_obj_set_style_text_color(lv_obj_t *, lv_color_t, lv_style_selector_t) {}
void lv_obj_set_style_text_letter_space(lv_obj_t *, int, lv_style_selector_t) {}
void lv_obj_set_style_text_align(lv_obj_t *, int, lv_style_selector_t) {}
void lv_obj_set_style_anim_duration(lv_obj_t *, uint32_t, lv_style_selector_t) {}
void lv_label_set_long_mode(lv_obj_t *, int) {}
void lv_label_set_text(lv_obj_t *obj, const char *text) { g_ui_text[obj->id] = text; }
void lv_obj_add_event_cb(lv_obj_t *, lv_event_cb_t cb, int filter, void *data) { if (filter == LV_EVENT_KEY) { g_ui_event_cb=cb; g_ui_event_user_data=data; } }
lv_group_t *lv_group_create() { return new lv_group_t{1}; }
void lv_group_add_obj(lv_group_t *, lv_obj_t *) {}
void lv_group_focus_obj(lv_obj_t *) {}
void lv_indev_set_group(lv_indev_t *, lv_group_t *) {}
void lv_group_delete(lv_group_t *group) { delete group; }
const lv_font_t *lv_obj_get_style_text_font(const lv_obj_t *, lv_style_selector_t) { return &font24; }
int32_t lv_obj_get_content_width(const lv_obj_t *) { return 296; }
void lv_text_get_size(lv_point_t *size, const char *text, const lv_font_t *font, int32_t, int32_t, lv_coord_t, uint32_t) { size->x = int(font->size * 0.6 * std::string(text).size()); size->y = font->size; }
int lv_event_get_code(const lv_event_t *event) { return event->code; }
uint32_t lv_event_get_key(const lv_event_t *event) { return event->key; }
void *lv_event_get_user_data(const lv_event_t *event) { return event->user_data; }

#include "../main/src/calculator_app.cpp"

extern "C" lv_indev_t *app_get_keyboard_indev(void) { return nullptr; }
extern "C" void app_request_quit(void) {}

static int failures = 0;
static void send(uint32_t key) { lv_event_t event{LV_EVENT_KEY, key, g_ui_event_user_data}; g_ui_event_cb(&event); }
static void expect(const std::string &what, const std::string &actual, const std::string &expected) {
    if (actual != expected) { std::printf("FAIL %s: got <%s>, want <%s>\n", what.c_str(), actual.c_str(), expected.c_str()); ++failures; }
}
int main() {
    lv_obj_t screen{99};
    reset_test_label_count();
    calculator_ui_build(&screen);
    expect("initial source", g_ui_text[1], "");
    expect("initial main", g_ui_text[2], "0");
    send('a'); expect("unsupported", g_ui_text[2], "0");
    send('h');
    send('5');
    expect("help modal swallows first key", g_ui_text[2], "0");
    send('5');
    expect("input after modal closes", g_ui_text[2], "5");
    send(kClearAllKey);
    expect("long backspace clears", g_ui_text[2], "0");
    for (char c : std::string("1+2")) send(uint32_t(c));
    expect("entry source", g_ui_text[1], "");
    expect("entry main", g_ui_text[2], "1+2");
    send(LV_KEY_ENTER);
    expect("eval source", g_ui_text[1], "1+2");
    expect("eval main", g_ui_text[2], "3");
    send('*'); send('2');
    expect("continuation source", g_ui_text[1], "1+2");
    expect("continuation main", g_ui_text[2], "3*2");
    send(LV_KEY_ENTER);
    expect("continued source", g_ui_text[1], "3*2");
    expect("continued main", g_ui_text[2], "6");
    send('4');
    expect("new digit source", g_ui_text[1], "");
    expect("new digit main", g_ui_text[2], "4");

    reset_test_label_count();
    calculator_ui_build(&screen);
    for (char c : std::string("1/0")) send(uint32_t(c));
    send(LV_KEY_ENTER);
    expect("error source", g_ui_text[1], "1/0");
    expect("error main", g_ui_text[2], "error");
    send(LV_KEY_BACKSPACE);
    expect("error backspace", g_ui_text[2], "0");
    if (failures == 0) std::printf("calculator UI state tests passed\n");
    return failures == 0 ? 0 : 1;
}
