#include "calculator_app.h"
#include "calculator_engine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

extern const lv_font_t montserrat_bold_14;
extern const lv_font_t jetbrains_mono_14;
extern const lv_font_t jetbrains_mono_20;
extern const lv_font_t jetbrains_mono_24;
extern const lv_font_t lv_font_montserrat_14;

namespace {

constexpr int32_t kScreenWidth = 320;
constexpr int32_t kScreenHeight = 170;
constexpr int32_t kTextX = 12;
constexpr int32_t kTextWidth = 296;
constexpr int32_t kSourceY = 52;
constexpr int32_t kSourceHeight = 28;
constexpr int32_t kMainY = 88;
constexpr int32_t kMainHeight = 36;
constexpr uint32_t kLongEscMs = 1000;
constexpr uint32_t kScrollSpeedPerSecond = 25;
constexpr uint32_t kMinScrollDuration = 6000;
constexpr char kClearAllKey = 'C';
constexpr uint32_t kHelpKey = 0x10001;

bool is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

bool is_binary_operator(char ch)
{
    return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^';
}

int open_parentheses(const std::string &text)
{
    int balance = 0;
    for (char ch : text) {
        if (ch == '(') {
            ++balance;
        } else if (ch == ')' && balance > 0) {
            --balance;
        }
    }
    return balance;
}

class CalculatorView {
public:
    explicit CalculatorView(lv_obj_t *screen)
    {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_size(screen, kScreenWidth, kScreenHeight);
        lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

        root_ = lv_obj_create(screen);
        lv_obj_remove_style_all(root_);
        lv_obj_set_size(root_, kScreenWidth, kScreenHeight);
        lv_obj_set_pos(root_, 0, 0);
        lv_obj_set_style_bg_color(root_, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICK_FOCUSABLE);

        source_label_ = lv_label_create(root_);
        configure_label(source_label_, kSourceY, kSourceHeight, &jetbrains_mono_20, 0x9a9aa0);
        main_label_ = lv_label_create(root_);
        configure_label(main_label_, kMainY, kMainHeight, &jetbrains_mono_24, 0xffffff);

        lv_label_set_text(source_label_, "");
        lv_label_set_text(main_label_, "0");
        lv_obj_add_event_cb(root_, CalculatorView::handle_key_event, LV_EVENT_KEY, this);

        key_group_ = lv_group_create();
        lv_group_add_obj(key_group_, root_);
        lv_group_focus_obj(root_);
        if (lv_indev_t *keyboard = app_get_keyboard_indev()) {
            lv_indev_set_group(keyboard, key_group_);
        }
    }

    ~CalculatorView()
    {
        stop_help_auto_scroll();
        if (key_group_ != nullptr) {
            lv_group_delete(key_group_);
        }
    }

    void show_help_for_debug()
    {
        show_help();
    }

private:
    void configure_label(lv_obj_t *label, int32_t y, int32_t height,
                         const lv_font_t *font, uint32_t color)
    {
        lv_obj_set_pos(label, kTextX, y);
        lv_obj_set_size(label, kTextWidth, height);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        lv_obj_set_style_text_letter_space(label, 0, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_anim_duration(label, 0, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }

    void set_label_text(lv_obj_t *label, const std::string &text)
    {
        lv_label_set_text(label, text.c_str());

        lv_point_t text_size;
        lv_text_get_size(&text_size, text.c_str(), lv_obj_get_style_text_font(label, LV_PART_MAIN),
                         0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        const int32_t visible_width = lv_obj_get_content_width(label);
        if (text_size.x > visible_width) {
            const uint32_t overflow = static_cast<uint32_t>(text_size.x - visible_width + 24);
            uint32_t duration = overflow * 1000U / kScrollSpeedPerSecond;
            if (duration < kMinScrollDuration) {
                duration = kMinScrollDuration;
            }
            lv_obj_set_style_anim_duration(label, duration, 0);
        } else {
            lv_obj_set_style_anim_duration(label, 0, 0);
        }
    }

    void clear_all()
    {
        clear_error();
        expression_.clear();
        source_text_.clear();
        main_text_.clear();
        last_result_.clear();
        just_evaluated_ = false;
        refresh();
    }

    static bool can_start_new_from_result(char input)
    {
        return is_binary_operator(input) || input == '.';
    }

    void insert(char input)
    {
        if (error_) {
            if (is_binary_operator(input)) {
                return;
            }
            clear_error();
            expression_.clear();
        }

        if (just_evaluated_ && can_start_new_from_result(input)) {
            expression_ = last_result_;
            just_evaluated_ = false;
        } else if (just_evaluated_) {
            expression_.clear();
            source_text_.clear();
            just_evaluated_ = false;
        }

        const char previous = expression_.empty() ? '\0' : expression_.back();

        if (is_digit(input)) {
            if (previous == ')' || previous == '.') {
                if (previous == ')') {
                    return;
                }
            }
            expression_.push_back(input);
            refresh();
            return;
        }

        switch (input) {
        case '.':
            if (previous == ')' || previous == '(') {
                if (previous == '(') {
                    expression_ += "0.";
                    refresh();
                }
                return;
            }
            if (is_digit(previous) && current_number_has_dot()) {
                return;
            }
            if (is_digit(previous)) {
                expression_.push_back('.');
            } else if (expression_.empty() || is_binary_operator(previous) || previous == '(') {
                expression_.push_back('0');
                expression_.push_back('.');
            }
            refresh();
            return;
        case '(':
            if (previous == '\0' || is_binary_operator(previous) || previous == '(') {
                expression_.push_back('(');
            } else if (just_evaluated_ && expression_.empty()) {
                expression_ = "(";
            }
            refresh();
            return;
        case ')':
            if (open_parentheses(expression_) > 0 &&
                (is_digit(previous) || previous == '.' || previous == ')')) {
                expression_.push_back(')');
                refresh();
            }
            return;
        case '+':
        case '*':
        case '/':
        case '^':
            if (expression_.empty() || previous == '(') {
                return;
            }
            if (is_binary_operator(previous)) {
                expression_.pop_back();
            }
            expression_.push_back(input);
            refresh();
            return;
        case '-':
            if (previous == '\0' || previous == '(' || is_binary_operator(previous)) {
                if (is_binary_operator(previous) && previous != '-') {
                    expression_.pop_back();
                } else if (previous == '-') {
                    expression_.pop_back();
                    expression_.push_back('+');
                    return;
                }
                expression_.push_back('-');
            } else {
                expression_.push_back('-');
            }
            refresh();
            return;
        default:
            return;
        }
    }

    bool current_number_has_dot() const
    {
        size_t index = expression_.size();
        while (index > 0) {
            const char ch = expression_[index - 1];
            if (ch == '.') {
                return true;
            }
            if (!is_digit(ch)) {
                break;
            }
            --index;
        }
        return false;
    }

    void evaluate()
    {
        if (just_evaluated_ || expression_.empty()) {
            return;
        }

        const calculator::EvalResult result = calculator::evaluate_expression(expression_);
        if (!result.ok) {
            error_ = true;
            source_text_ = expression_;
            main_text_ = "error";
            refresh();
            return;
        }

        last_result_ = calculator::format_value(result.value);
        source_text_ = expression_;
        main_text_ = last_result_;
        expression_ = last_result_;
        just_evaluated_ = true;
        refresh();
    }

    void backspace()
    {
        if (error_) {
            clear_error();
            expression_.clear();
            refresh();
            return;
        }
        if (just_evaluated_) {
            just_evaluated_ = false;
        }
        if (!expression_.empty()) {
            expression_.pop_back();
        }
        refresh();
    }

    void clear_error()
    {
        error_ = false;
        source_text_.clear();
        main_text_.clear();
    }

    void refresh()
    {
        if (error_) {
            set_label_text(source_label_, source_text_);
            set_label_text(main_label_, main_text_);
            return;
        }

        if (just_evaluated_) {
            set_label_text(source_label_, source_text_);
            set_label_text(main_label_, main_text_);
            return;
        }

        set_label_text(source_label_, source_text_);
        set_label_text(main_label_, expression_.empty() ? "0" : expression_);
    }

    lv_obj_t *create_modal_overlay()
    {
        lv_obj_t *overlay = lv_obj_create(root_);
        lv_obj_remove_style_all(overlay);
        lv_obj_set_size(overlay, kScreenWidth, kScreenHeight);
        lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
        lv_obj_set_style_border_width(overlay, 0, 0);
        lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
        return overlay;
    }

    lv_obj_t *create_modal_card(lv_obj_t *overlay, int32_t width, int32_t height,
                                int32_t pad_top, int32_t pad_bottom)
    {
        lv_obj_t *card = lv_obj_create(overlay);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, width, height);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x18181c), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x3c3c40), 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_pad_left(card, 14, 0);
        lv_obj_set_style_pad_right(card, 14, 0);
        lv_obj_set_style_pad_top(card, pad_top, 0);
        lv_obj_set_style_pad_bottom(card, pad_bottom, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        return card;
    }

    void show_help()
    {
        if (modal_ != nullptr) {
            return;
        }

        lv_obj_t *overlay = create_modal_overlay();
        lv_obj_t *card = create_modal_card(overlay, 304, 154, 10, 8);

        lv_obj_t *title = lv_label_create(card);
        lv_obj_set_style_text_font(title, &montserrat_bold_14, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
        lv_label_set_text(title, "HELP");
        lv_obj_set_pos(title, 0, 0);

        help_scroll_ = lv_obj_create(card);
        lv_obj_remove_style_all(help_scroll_);
        lv_obj_set_size(help_scroll_, 276, 92);
        lv_obj_set_pos(help_scroll_, 0, 20);
        lv_obj_set_style_bg_opa(help_scroll_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(help_scroll_, 0, 0);
        lv_obj_set_style_pad_all(help_scroll_, 0, 0);
        lv_obj_set_scroll_dir(help_scroll_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(help_scroll_, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_add_event_cb(help_scroll_, CalculatorView::handle_scroll_event, LV_EVENT_SCROLL, this);

        lv_obj_t *text = lv_label_create(help_scroll_);
        lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(text, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_line_space(text, 2, 0);
        lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(text, 260);
        lv_obj_set_height(text, LV_SIZE_CONTENT);
        lv_label_set_text(text,
            "A simple calculator for everyday use.\n"
            "Supports + - * / ^ ().\n"
            "\n"
            "Number keys: numbers.\n"
            "Sym + symbol keys: . + - * / ^ ().\n"
            "OK or =: calculate.\n"
            "Backspace: delete one character.\n"
            "Long Backspace: AC.\n"
            "Short ESC: hint.\n"
            "Long ESC: exit.");

        lv_obj_t *footer = lv_label_create(card);
        lv_obj_set_style_text_font(footer, &jetbrains_mono_14, 0);
        lv_obj_set_style_text_color(footer, lv_color_hex(0x8a8a90), 0);
        lv_label_set_text(footer, "F/X or Fn+F/X: scroll / ESC: close");
        lv_obj_align(footer, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

        modal_ = overlay;
        modal_active_ = true;
        help_auto_scrolling_ = false;
        help_auto_timer_ = lv_timer_create(CalculatorView::help_auto_timer_cb, 40, this);
    }

    void show_exit_hint()
    {
        if (modal_ != nullptr) {
            return;
        }

        lv_obj_t *overlay = create_modal_overlay();
        lv_obj_t *card = create_modal_card(overlay, 252, 54, 8, 8);

        lv_obj_t *text = lv_label_create(card);
        lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(text, lv_color_hex(0xffffff), 0);
        lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(text, 224);
        lv_label_set_text(text, "Long-press ESC to exit.");
        lv_obj_center(text);

        modal_ = overlay;
        modal_active_ = true;
    }

    void scroll_help(int32_t distance)
    {
        stop_help_auto_scroll();
        if (help_scroll_ == nullptr || distance == 0) {
            return;
        }

        const int32_t available = distance > 0 ? lv_obj_get_scroll_top(help_scroll_)
                                               : lv_obj_get_scroll_bottom(help_scroll_);
        const int32_t amount = distance > 0 ? std::min(distance, available)
                                            : -std::min(-distance, available);
        if (amount != 0) {
            lv_obj_scroll_by_bounded(help_scroll_, 0, amount, LV_ANIM_ON);
        }
    }

    void stop_help_auto_scroll()
    {
        if (help_auto_timer_ != nullptr) {
            lv_timer_t *timer = help_auto_timer_;
            help_auto_timer_ = nullptr;
            lv_timer_delete(timer);
        }
        help_auto_scrolling_ = false;
    }

    void help_auto_tick()
    {
        if (help_scroll_ == nullptr) {
            stop_help_auto_scroll();
            return;
        }
        if (lv_obj_get_scroll_bottom(help_scroll_) <= 0) {
            stop_help_auto_scroll();
            return;
        }

        help_auto_scrolling_ = true;
        lv_obj_scroll_by_bounded(help_scroll_, 0, -1, LV_ANIM_OFF);
        help_auto_scrolling_ = false;
    }

    void notify_help_scrolled()
    {
        if (!help_auto_scrolling_) {
            stop_help_auto_scroll();
        }
    }

    void close_modal()
    {
        stop_help_auto_scroll();
        if (modal_ != nullptr) {
            lv_obj_delete(modal_);
            modal_ = nullptr;
        }
        help_scroll_ = nullptr;
        modal_active_ = false;
    }

    void handle_key(uint32_t key)
    {
        if (modal_active_) {
            if (key == LV_KEY_UP || key == 'f' || key == 'F') {
                scroll_help(28);
            } else if (key == LV_KEY_DOWN || key == 'x' || key == 'X') {
                scroll_help(-28);
            } else {
                close_modal();
            }
            return;
        }

        if (key == kHelpKey || key == 'h' || key == 'H') {
            show_help();
            return;
        }

        if (key == LV_KEY_HOME) {
            app_request_quit();
            return;
        }
        if (key == LV_KEY_ESC) {
            show_exit_hint();
            return;
        }
        if (key == '=') {
            evaluate();
            return;
        }
        if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            backspace();
            return;
        }
        if (key == LV_KEY_ENTER || key == ' ') {
            evaluate();
            return;
        }
        if (key == kClearAllKey) {
            clear_all();
            return;
        }
        if (key < 32 || key > 126) {
            return;
        }

        switch (static_cast<char>(key)) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '.': case '+': case '-': case '*': case '/':
        case '^': case '(': case ')':
            insert(static_cast<char>(key));
            break;
        default:
            break;
        }
    }

    static void handle_scroll_event(lv_event_t *event)
    {
        if (lv_event_get_code(event) != LV_EVENT_SCROLL) {
            return;
        }
        auto *view = static_cast<CalculatorView *>(lv_event_get_user_data(event));
        if (view != nullptr) {
            view->notify_help_scrolled();
        }
    }

    static void help_auto_timer_cb(lv_timer_t *timer)
    {
        auto *view = static_cast<CalculatorView *>(lv_timer_get_user_data(timer));
        if (view != nullptr) {
            view->help_auto_tick();
        }
    }

    static void handle_key_event(lv_event_t *event)
    {
        if (lv_event_get_code(event) != LV_EVENT_KEY) {
            return;
        }
        auto *view = static_cast<CalculatorView *>(lv_event_get_user_data(event));
        if (view != nullptr) {
            view->handle_key(lv_event_get_key(event));
        }
    }

    lv_obj_t *root_ = nullptr;
    lv_obj_t *source_label_ = nullptr;
    lv_obj_t *main_label_ = nullptr;
    lv_obj_t *modal_ = nullptr;
    lv_obj_t *help_scroll_ = nullptr;
    lv_timer_t *help_auto_timer_ = nullptr;
    bool help_auto_scrolling_ = false;
    bool modal_active_ = false;
    lv_group_t *key_group_ = nullptr;
    std::string expression_;
    std::string source_text_;
    std::string main_text_;
    std::string last_result_;
    bool just_evaluated_ = false;
    bool error_ = false;
};

CalculatorView *g_calculator_view = nullptr;

}  // namespace

extern "C" void calculator_ui_build(lv_obj_t *screen)
{
    delete g_calculator_view;
    g_calculator_view = new CalculatorView(screen);
}

extern "C" void calculator_ui_show_help_for_debug(void)
{
    if (g_calculator_view != nullptr) {
        g_calculator_view->show_help_for_debug();
    }
}
