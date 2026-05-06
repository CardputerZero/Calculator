#include "calculator_app.h"

#include "calculator_audio.h"
#include "calculator_engine.h"
#include "calculator_input.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

using calculator::AngleMode;
using calculator::EvalResult;

constexpr lv_coord_t kScreenWidth = 320;
constexpr lv_coord_t kScreenHeight = 170;
constexpr lv_coord_t kDisplayX = 6;
constexpr lv_coord_t kDisplayY = 4;
constexpr lv_coord_t kDisplayWidth = 308;
constexpr lv_coord_t kDisplayHeight = 34;
constexpr lv_coord_t kDisplayInnerLeftPadding = 10;
constexpr lv_coord_t kDisplayInnerRightPadding = 8;
constexpr lv_coord_t kDisplayMiddleGap = 8;
constexpr lv_coord_t kResultAreaPadding = 4;
constexpr lv_coord_t kResultAreaMinWidth = 72;
constexpr lv_coord_t kPadX = 6;
constexpr lv_coord_t kPadY = 42;
constexpr lv_coord_t kPadWidth = 308;
constexpr lv_coord_t kPadHeight = 122;
constexpr lv_coord_t kButtonGap = 2;
constexpr lv_coord_t kFocusLiftTopRow = 2;
constexpr lv_coord_t kFocusLiftOtherRows = 1;
constexpr int kRows = 4;
constexpr int kCols = 11;
constexpr size_t kMaxHistory = 32;

enum class ActionType {
    Insert,
    ClearAll,
    Evaluate,
    MemoryClear,
    MemoryAdd,
    MemorySubtract,
    MemoryRecall,
    ToggleSecond,
    ToggleAngle,
    ToggleSign,
    InsertAns,
    InsertPi,
    InsertE,
    InsertScientificE,
    InsertRandom,
    InsertSquare,
    InsertCube,
    InsertPower,
    InsertExp,
    InsertTenPow,
    InsertReciprocal,
    InsertSqrt,
    InsertCbrt,
    InsertYRoot,
    InsertPercent,
    InsertFactorial,
    InsertFunction,
    Backspace,
};

enum class ButtonStyleKind {
    Number,
    Function,
    Control,
    Accent,
    Enter,
};

struct ButtonSpec {
    const char *label;
    ActionType action;
    const char *token;
    ButtonStyleKind style;
};

struct HistoryEntry {
    std::string formula;
    std::string result;
};

struct ButtonContext {
    class CalculatorApp *app = nullptr;
    int row = 0;
    int col = 0;
};

struct ButtonVisualStyle {
    lv_color_t bg;
    lv_color_t text;
};

constexpr std::array<std::array<ButtonSpec, kCols>, kRows> kButtonSpecs = {{
    {{{"1", ActionType::Insert, "1", ButtonStyleKind::Number},
      {"2", ActionType::Insert, "2", ButtonStyleKind::Number},
      {"3", ActionType::Insert, "3", ButtonStyleKind::Number},
      {"4", ActionType::Insert, "4", ButtonStyleKind::Number},
      {"5", ActionType::Insert, "5", ButtonStyleKind::Number},
      {"6", ActionType::Insert, "6", ButtonStyleKind::Number},
      {"7", ActionType::Insert, "7", ButtonStyleKind::Number},
      {"8", ActionType::Insert, "8", ButtonStyleKind::Number},
      {"9", ActionType::Insert, "9", ButtonStyleKind::Number},
      {"0", ActionType::Insert, "0", ButtonStyleKind::Number},
      {"Del", ActionType::Backspace, nullptr, ButtonStyleKind::Control}}},
    {{{"C", ActionType::ClearAll, nullptr, ButtonStyleKind::Control},
      {"sin", ActionType::InsertFunction, "sin", ButtonStyleKind::Function},
      {"cos", ActionType::InsertFunction, "cos", ButtonStyleKind::Function},
      {"+", ActionType::Insert, "+", ButtonStyleKind::Accent},
      {"-", ActionType::Insert, "-", ButtonStyleKind::Accent},
      {"/", ActionType::Insert, "/", ButtonStyleKind::Accent},
      {"*", ActionType::Insert, "*", ButtonStyleKind::Accent},
      {"^", ActionType::InsertPower, nullptr, ButtonStyleKind::Accent},
      {"(", ActionType::Insert, "(", ButtonStyleKind::Function},
      {")", ActionType::Insert, ")", ButtonStyleKind::Function},
      {"Rad", ActionType::ToggleAngle, nullptr, ButtonStyleKind::Control}}},
    {{{".", ActionType::Insert, ".", ButtonStyleKind::Number},
      {"tan", ActionType::InsertFunction, "tan", ButtonStyleKind::Function},
      {"ln", ActionType::InsertFunction, "ln", ButtonStyleKind::Function},
      {"log", ActionType::InsertFunction, "log", ButtonStyleKind::Function},
      {"sqrt", ActionType::InsertSqrt, nullptr, ButtonStyleKind::Function},
      {"=", ActionType::Evaluate, nullptr, ButtonStyleKind::Enter},
      {"abs", ActionType::InsertFunction, "abs", ButtonStyleKind::Function},
      {"pi", ActionType::InsertPi, nullptr, ButtonStyleKind::Function},
      {"e", ActionType::InsertE, nullptr, ButtonStyleKind::Function},
      {"!", ActionType::InsertFactorial, nullptr, ButtonStyleKind::Function},
      {"OK", ActionType::Evaluate, nullptr, ButtonStyleKind::Enter}}},
    {{{"2nd", ActionType::ToggleSecond, nullptr, ButtonStyleKind::Control},
      {"Ans", ActionType::InsertAns, nullptr, ButtonStyleKind::Control},
      {"Rand", ActionType::InsertRandom, nullptr, ButtonStyleKind::Control},
      {"1/x", ActionType::InsertReciprocal, nullptr, ButtonStyleKind::Function},
      {"x^2", ActionType::InsertSquare, nullptr, ButtonStyleKind::Function},
      {"x^3", ActionType::InsertCube, nullptr, ButtonStyleKind::Function},
      {"x^y", ActionType::InsertPower, nullptr, ButtonStyleKind::Function},
      {"e^x", ActionType::InsertExp, nullptr, ButtonStyleKind::Function},
      {"10^x", ActionType::InsertTenPow, nullptr, ButtonStyleKind::Function},
      {"%", ActionType::InsertPercent, nullptr, ButtonStyleKind::Control},
      {"+/-", ActionType::ToggleSign, nullptr, ButtonStyleKind::Control}}}
}};

lv_color_t color_hex(uint32_t hex)
{
    return lv_color_hex(hex);
}

class CalculatorApp {
public:
    explicit CalculatorApp(lv_obj_t *screen)
        : screen_(screen), rng_(std::random_device{}())
    {
        build();
        refresh_dynamic_labels();
        refresh_display();
        refresh_focus();
    }

private:
    void build()
    {
        lv_obj_set_style_bg_color(screen_, color_hex(0x050505), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_size(screen_, kScreenWidth, kScreenHeight);
        lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

        root_panel_ = lv_obj_create(screen_);
        lv_obj_remove_style_all(root_panel_);
        lv_obj_set_size(root_panel_, kScreenWidth, kScreenHeight);
        lv_obj_clear_flag(root_panel_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(root_panel_, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(root_panel_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_set_style_bg_color(root_panel_, color_hex(0x050505), 0);
        lv_obj_set_style_bg_opa(root_panel_, LV_OPA_COVER, 0);
        lv_obj_add_event_cb(root_panel_, handle_root_event, LV_EVENT_KEY, this);

        key_group_ = lv_group_create();
        lv_group_add_obj(key_group_, root_panel_);
        lv_group_focus_obj(root_panel_);

        if (lv_indev_t *keyboard = app_get_keyboard_indev()) {
            lv_indev_set_group(keyboard, key_group_);
        }

        build_display();
        build_buttons();
        build_focus_overlay();
        lv_obj_move_background(display_panel_);
    }

    void build_display()
    {
        configure_display_layout();
        display_panel_ = lv_obj_create(root_panel_);
        lv_obj_remove_style_all(display_panel_);
        lv_obj_set_pos(display_panel_, kDisplayX, kDisplayY);
        lv_obj_set_size(display_panel_, kDisplayWidth, kDisplayHeight);
        lv_obj_set_style_bg_color(display_panel_, color_hex(0x111111), 0);
        lv_obj_set_style_bg_opa(display_panel_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(display_panel_, color_hex(0x262626), 0);
        lv_obj_set_style_border_width(display_panel_, 1, 0);
        lv_obj_set_style_radius(display_panel_, 10, 0);
        lv_obj_clear_flag(display_panel_, LV_OBJ_FLAG_SCROLLABLE);

        expression_label_ = lv_label_create(display_panel_);
        lv_obj_set_pos(expression_label_, kDisplayInnerLeftPadding, 8);
        lv_obj_set_size(expression_label_, expression_area_width_, 14);
        lv_obj_set_style_text_font(expression_label_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(expression_label_, color_hex(0xD8D8D8), 0);
        lv_label_set_long_mode(expression_label_, LV_LABEL_LONG_CLIP);
        lv_label_set_text(expression_label_, "");

        equals_label_ = lv_label_create(display_panel_);
        lv_obj_set_pos(equals_label_, equals_area_x_, 10);
        lv_obj_set_style_text_font(equals_label_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(equals_label_, color_hex(0x9E9E9E), 0);
        lv_label_set_text(equals_label_, "=");

        result_label_ = lv_label_create(display_panel_);
        lv_obj_set_pos(result_label_, result_area_x_, 8);
        lv_obj_set_size(result_label_, result_area_width_, 18);
        lv_obj_set_style_text_align(result_label_, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(result_label_, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(result_label_, color_hex(0xF4F4F4), 0);
        lv_label_set_text(result_label_, "0");
    }

    void build_buttons()
    {
        keypad_panel_ = lv_obj_create(root_panel_);
        lv_obj_remove_style_all(keypad_panel_);
        lv_obj_set_pos(keypad_panel_, kPadX, kPadY);
        lv_obj_set_size(keypad_panel_, kPadWidth, kPadHeight);
        lv_obj_set_style_bg_opa(keypad_panel_, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(keypad_panel_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(keypad_panel_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        button_width_ = (kPadWidth - (kCols - 1) * kButtonGap) / kCols;
        button_height_ = (kPadHeight - (kRows - 1) * kButtonGap) / kRows;

        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kCols; ++col) {
                ButtonContext &context = button_contexts_[row][col];
                context.app = this;
                context.row = row;
                context.col = col;

                lv_obj_t *button = lv_btn_create(keypad_panel_);
                button_matrix_[row][col] = button;
                lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_pos(button, col * (button_width_ + kButtonGap),
                               row * (button_height_ + kButtonGap));
                lv_obj_set_size(button, button_width_, button_height_);
                lv_obj_set_style_radius(button, 6, 0);
                lv_obj_set_style_shadow_width(button, 0, 0);
                lv_obj_set_style_outline_pad(button, 1, 0);
                lv_obj_add_event_cb(button, handle_button_click, LV_EVENT_CLICKED, &context);

                lv_obj_t *label = lv_label_create(button);
                label_matrix_[row][col] = label;
                lv_obj_set_width(label, button_width_ - 4);
                lv_obj_set_align(label, LV_ALIGN_CENTER);
                lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
                lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
                lv_label_set_text(label, kButtonSpecs[row][col].label);
            }
        }
    }

    void build_focus_overlay()
    {
        focus_overlay_ = lv_obj_create(root_panel_);
        lv_obj_remove_style_all(focus_overlay_);
        lv_obj_set_size(focus_overlay_, button_width_, button_height_);
        lv_obj_set_style_radius(focus_overlay_, 6, 0);
        lv_obj_set_style_shadow_width(focus_overlay_, 0, 0);
        lv_obj_set_style_outline_pad(focus_overlay_, 1, 0);
        lv_obj_clear_flag(focus_overlay_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(focus_overlay_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_add_flag(focus_overlay_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(focus_overlay_, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_event_cb(focus_overlay_, handle_focus_overlay_click, LV_EVENT_CLICKED, this);

        focus_overlay_label_ = lv_label_create(focus_overlay_);
        lv_obj_set_width(focus_overlay_label_, button_width_ - 4);
        lv_obj_set_align(focus_overlay_label_, LV_ALIGN_CENTER);
        lv_obj_set_style_text_align(focus_overlay_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(focus_overlay_label_, LV_LABEL_LONG_CLIP);
    }

    void move_selection(int delta_row, int delta_col)
    {
        int next_row = selected_row_ + delta_row;
        int next_col = selected_col_ + delta_col;

        if (next_row < 0) next_row = 0;
        if (next_row >= kRows) next_row = kRows - 1;
        if (next_col < 0) next_col = 0;
        if (next_col >= kCols) next_col = kCols - 1;

        selected_row_ = next_row;
        selected_col_ = next_col;
        refresh_focus();
    }

    void browse_history(int direction)
    {
        if (history_.empty()) {
            return;
        }

        if (history_index_ < 0) {
            history_index_ = static_cast<int>(history_.size()) - 1;
        } else {
            history_index_ += direction;
            if (history_index_ < 0) history_index_ = 0;
            if (history_index_ >= static_cast<int>(history_.size())) history_index_ = -1;
        }
    }

    std::string current_status_text() const
    {
        std::string text;
        if (history_index_ >= 0 && history_index_ < static_cast<int>(history_.size())) {
            text = "HIST ";
            text += std::to_string(history_index_ + 1);
            text += "/";
            text += std::to_string(history_.size());
        } else {
            text = "LIVE";
        }

        text += angle_mode_ == AngleMode::Degree ? " DEG" : " RAD";
        if (second_mode_) text += " 2nd";
        if (std::fabs(memory_value_) > 1e-12) text += " M";
        return text;
    }

    std::string current_expression_text() const
    {
        if (history_index_ >= 0 && history_index_ < static_cast<int>(history_.size())) {
            return history_[history_index_].formula;
        }
        if (error_state_) {
            return expression_.empty() ? "Invalid expression" : expression_;
        }
        if (just_evaluated_ && !last_formula_.empty()) {
            return last_formula_;
        }
        return expression_.empty() ? "" : expression_;
    }

    std::string current_result_text() const
    {
        if (history_index_ >= 0 && history_index_ < static_cast<int>(history_.size())) {
            return history_[history_index_].result;
        }
        if (error_state_) {
            return "ERR";
        }
        if (just_evaluated_ && !last_result_text_.empty()) {
            return last_result_text_;
        }
        if (expression_.empty()) {
            return "0";
        }
        const EvalResult preview = calculator::evaluate_expression(expression_, angle_mode_);
        return preview.ok ? calculator::format_value(preview.value) : "...";
    }

    void refresh_display()
    {
        const std::string expression = current_expression_text();
        const bool show_expression = !expression.empty();
        const std::string result = current_result_text();
        update_display_layout(result);
        lv_point_t expression_size{};
        lv_text_get_size(&expression_size, expression.c_str(), &lv_font_montserrat_12, 0, 0,
                         LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_label_set_long_mode(expression_label_,
                               show_expression &&
                                       expression_size.x > lv_obj_get_content_width(expression_label_)
                                   ? LV_LABEL_LONG_SCROLL_CIRCULAR
                                   : LV_LABEL_LONG_CLIP);
        lv_label_set_text(expression_label_, expression.c_str());
        if (show_expression) {
            lv_obj_clear_flag(equals_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(result_label_, result_area_x_, 8);
        } else {
            lv_obj_add_flag(equals_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(result_label_, result_area_x_, 7);
        }

        lv_label_set_text(result_label_, result.c_str());
        dump_debug_state(expression, result, show_expression);
    }

    void dump_debug_state(const std::string &expression, const std::string &result,
                          bool show_expression) const
    {
        const char *path = std::getenv("CALCULATOR_STATE_PATH");
        if (path == nullptr || *path == '\0') {
            return;
        }

        FILE *fp = std::fopen(path, "w");
        if (fp == nullptr) {
            return;
        }

        std::fprintf(fp, "expression=%s\n", expression.c_str());
        std::fprintf(fp, "result=%s\n", result.c_str());
        std::fprintf(fp, "show_expression=%d\n", show_expression ? 1 : 0);
        std::fprintf(fp, "angle_mode=%s\n",
                     angle_mode_ == AngleMode::Degree ? "degree" : "radian");
        std::fclose(fp);
    }

    std::string mapped_function_name(const char *token) const
    {
        if (!second_mode_) {
            return token ? token : "";
        }

        if (std::strcmp(token, "sin") == 0) return "asin";
        if (std::strcmp(token, "cos") == 0) return "acos";
        if (std::strcmp(token, "tan") == 0) return "atan";
        if (std::strcmp(token, "sinh") == 0) return "asinh";
        if (std::strcmp(token, "cosh") == 0) return "acosh";
        if (std::strcmp(token, "tanh") == 0) return "atanh";
        return token ? token : "";
    }

    std::string label_for_button(const ButtonSpec &spec) const
    {
        if (spec.action == ActionType::ToggleAngle) {
            return angle_mode_ == AngleMode::Degree ? "Deg" : "Rad";
        }
        if (second_mode_ && spec.action == ActionType::InsertRandom) {
            return calculator::audio_enabled() ? "Snd*" : "Snd";
        }
        if (second_mode_ && spec.action == ActionType::InsertSqrt) {
            return "cbrt";
        }
        if (spec.action == ActionType::InsertFunction && spec.token != nullptr) {
            return mapped_function_name(spec.token);
        }
        if (spec.action == ActionType::ToggleSecond) {
            return second_mode_ ? "2nd*" : "2nd";
        }
        return spec.label;
    }

    bool button_is_toggled(const ButtonSpec &spec) const
    {
        if (spec.action == ActionType::ToggleSecond) {
            return second_mode_;
        }
        if (second_mode_ && spec.action == ActionType::InsertRandom) {
            return calculator::audio_enabled();
        }
        return false;
    }

    const lv_font_t *font_for_button_label(const std::string &text) const
    {
        return text.size() > 3 ? &lv_font_montserrat_8
                               : (text.size() > 2 ? &lv_font_montserrat_10
                                                  : &lv_font_montserrat_12);
    }

    lv_coord_t focus_lift_for_row(int row) const
    {
        return row == 0 ? kFocusLiftTopRow : kFocusLiftOtherRows;
    }

    lv_coord_t measure_text_width(const std::string &text, const lv_font_t *font) const
    {
        lv_point_t size{};
        lv_text_get_size(&size, text.empty() ? "0" : text.c_str(), font, 0, 0,
                         LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        return size.x;
    }

    lv_coord_t max_result_area_width() const
    {
        const lv_coord_t equals_width = measure_text_width("=", &lv_font_montserrat_12);
        const lv_coord_t width = kDisplayWidth - kDisplayInnerLeftPadding -
                                 kDisplayInnerRightPadding - (kDisplayMiddleGap * 2) -
                                 equals_width;
        return width > 0 ? width : 0;
    }

    lv_coord_t compute_baseline_result_area_width() const
    {
        lv_coord_t widest = 0;
        const auto record_sample = [&](double value) {
            const lv_coord_t width =
                measure_text_width(calculator::format_value(value), &lv_font_montserrat_8);
            if (width > widest) {
                widest = width;
            }
        };

        const std::array<double, 12> explicit_samples = {
            0.0,
            1.0 / 4356.0,
            -1.0 / 4356.0,
            0.000123456789012,
            -0.000123456789012,
            123456789012.0,
            -123456789012.0,
            std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::max(),
            std::numeric_limits<double>::min(),
            -std::numeric_limits<double>::min(),
            9.99999999999e11,
        };

        for (double value : explicit_samples) {
            record_sample(value);
        }

        for (int exponent = -12; exponent <= 12; ++exponent) {
            const double scale = std::pow(10.0, static_cast<double>(exponent));
            record_sample(1.23456789012 * scale);
            record_sample(-1.23456789012 * scale);
            record_sample(9.99999999999 * scale);
            record_sample(-9.99999999999 * scale);
        }

        widest += kResultAreaPadding;
        if (widest < kResultAreaMinWidth) {
            widest = kResultAreaMinWidth;
        }
        const lv_coord_t max_width = max_result_area_width();
        return widest > max_width ? max_width : widest;
    }

    void configure_display_layout()
    {
        baseline_result_area_width_ = compute_baseline_result_area_width();
        update_display_layout("0");
    }

    const lv_font_t *font_for_result_text(const std::string &text,
                                          lv_coord_t available_width) const
    {
        static const std::array<const lv_font_t *, 6> kFonts = {
            &lv_font_montserrat_18,
            &lv_font_montserrat_16,
            &lv_font_montserrat_14,
            &lv_font_montserrat_12,
            &lv_font_montserrat_10,
            &lv_font_montserrat_8,
        };

        const char *sample = text.empty() ? "0" : text.c_str();
        for (const lv_font_t *font : kFonts) {
            lv_point_t size{};
            lv_text_get_size(&size, sample, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            if (size.x <= available_width) {
                return font;
            }
        }
        return kFonts.back();
    }

    void update_display_layout(const std::string &result_text)
    {
        const lv_coord_t equals_width = measure_text_width("=", &lv_font_montserrat_12);
        const lv_coord_t max_width = max_result_area_width();
        const lv_font_t *result_font = font_for_result_text(result_text, max_width);
        lv_coord_t desired_width =
            measure_text_width(result_text.empty() ? "0" : result_text, result_font) +
            kResultAreaPadding;

        if (desired_width < baseline_result_area_width_) {
            desired_width = baseline_result_area_width_;
        }
        if (desired_width > max_width) {
            desired_width = max_width;
        }

        result_area_width_ = desired_width;
        result_area_x_ = kDisplayWidth - kDisplayInnerRightPadding - result_area_width_;
        equals_area_x_ = result_area_x_ - kDisplayMiddleGap - equals_width;
        expression_area_width_ = equals_area_x_ - kDisplayInnerLeftPadding - kDisplayMiddleGap;
        if (expression_area_width_ < 0) {
            expression_area_width_ = 0;
        }

        if (expression_label_ != nullptr) {
            lv_obj_set_size(expression_label_, expression_area_width_, 14);
        }
        if (equals_label_ != nullptr) {
            lv_obj_set_pos(equals_label_, equals_area_x_, 10);
        }
        if (result_label_ != nullptr) {
            lv_obj_set_pos(result_label_, result_area_x_, 8);
            lv_obj_set_size(result_label_, result_area_width_, 18);
            lv_obj_set_style_text_font(result_label_, result_font, 0);
        }
    }

    ButtonVisualStyle visual_style_for_button(const ButtonSpec &spec, bool toggled) const
    {
        lv_color_t bg = color_hex(0x232323);
        lv_color_t text = color_hex(0xF4F4F4);

        if (spec.style == ButtonStyleKind::Number) {
            bg = color_hex(0x2C2C2E);
        } else if (spec.style == ButtonStyleKind::Control) {
            bg = color_hex(0x5B5B5D);
        } else if (spec.style == ButtonStyleKind::Accent) {
            bg = color_hex(0xFF9F0A);
        } else if (spec.style == ButtonStyleKind::Enter) {
            bg = color_hex(0xFFB340);
        }

        if (spec.action == ActionType::ClearAll) {
            bg = color_hex(0xA5A5A5);
            text = color_hex(0x111111);
        } else if (spec.style == ButtonStyleKind::Accent || spec.style == ButtonStyleKind::Enter) {
            text = color_hex(0xFFFFFF);
        }

        if (toggled) {
            bg = color_hex(0x7A5B1C);
        }

        return {bg, text};
    }

    void apply_button_visual(lv_obj_t *button, lv_obj_t *label, const ButtonVisualStyle &visual,
                             bool selected, lv_coord_t translate_y)
    {
        lv_obj_set_style_bg_color(button, visual.bg, 0);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(button,
                                      selected ? color_hex(0xF6F6F6) : color_hex(0x090909),
                                      0);
        lv_obj_set_style_border_width(button, selected ? 2 : 1, 0);
        lv_obj_set_style_outline_color(button,
                                       selected ? color_hex(0xFFFFFF) : color_hex(0x000000),
                                       0);
        lv_obj_set_style_outline_opa(button, selected ? LV_OPA_50 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_outline_width(button, selected ? 1 : 0, 0);
        lv_obj_set_style_translate_y(button, translate_y, 0);
        lv_obj_set_style_text_color(label, visual.text, 0);
    }

    void refresh_dynamic_labels()
    {
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kCols; ++col) {
                const ButtonSpec &spec = kButtonSpecs[row][col];
                lv_obj_t *label = label_matrix_[row][col];
                if (label == nullptr) continue;

                const std::string text = label_for_button(spec);
                lv_label_set_text(label, text.c_str());
                lv_obj_set_style_text_font(label, font_for_button_label(text), 0);
            }
        }
    }

    void refresh_focus_overlay()
    {
        if (focus_overlay_ == nullptr || focus_overlay_label_ == nullptr) {
            return;
        }

        const ButtonSpec &spec = kButtonSpecs[selected_row_][selected_col_];
        const bool toggled = button_is_toggled(spec);
        const std::string text = label_for_button(spec);
        const ButtonVisualStyle visual = visual_style_for_button(spec, toggled);
        const lv_coord_t focus_lift = focus_lift_for_row(selected_row_);

        lv_obj_set_pos(focus_overlay_,
                       kPadX + selected_col_ * (button_width_ + kButtonGap),
                       kPadY + selected_row_ * (button_height_ + kButtonGap) - focus_lift);
        lv_obj_set_size(focus_overlay_, button_width_, button_height_);
        lv_obj_set_width(focus_overlay_label_, button_width_ - 4);
        lv_label_set_text(focus_overlay_label_, text.c_str());
        lv_obj_set_style_text_font(focus_overlay_label_, font_for_button_label(text), 0);
        apply_button_visual(focus_overlay_, focus_overlay_label_, visual, true, 0);
        lv_obj_move_foreground(focus_overlay_);
    }

    void refresh_focus()
    {
        refresh_dynamic_labels();
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kCols; ++col) {
                const ButtonSpec &spec = kButtonSpecs[row][col];
                lv_obj_t *button = button_matrix_[row][col];
                lv_obj_t *label = label_matrix_[row][col];
                if (button == nullptr || label == nullptr) continue;

                const bool selected = row == selected_row_ && col == selected_col_;
                const bool toggled = button_is_toggled(spec);
                const ButtonVisualStyle visual = visual_style_for_button(spec, toggled);
                apply_button_visual(button, label, visual, false, 0);
                lv_obj_set_style_bg_opa(button, selected ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
                lv_obj_set_style_border_opa(button, selected ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
                lv_obj_set_style_text_opa(label, selected ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
            }
        }
        refresh_focus_overlay();
    }

    void reset_view_state()
    {
        history_index_ = -1;
        error_state_ = false;
    }

    void prepare_for_insert(bool starts_new_expression)
    {
        reset_view_state();
        if (just_evaluated_ && starts_new_expression) {
            expression_.clear();
        }
        if (starts_new_expression || just_evaluated_) {
            just_evaluated_ = false;
        }
    }

    char last_non_space() const
    {
        for (auto it = expression_.rbegin(); it != expression_.rend(); ++it) {
            if (!std::isspace(static_cast<unsigned char>(*it))) {
                return *it;
            }
        }
        return '\0';
    }

    bool continues_from_result(const std::string &token) const
    {
        if (!just_evaluated_) return false;
        return token == "+" || token == "-" || token == "*" || token == "/" ||
               token == "^" || token == "!" || token == "%" || token == ")";
    }

    static bool is_numeric_fragment_token(const std::string &token)
    {
        if (token.size() != 1) {
            return false;
        }

        const char ch = token.front();
        return std::isdigit(static_cast<unsigned char>(ch)) || ch == '.' ||
               ch == '+' || ch == '-';
    }

    void append_token(const std::string &token, bool allow_number_continuation = false)
    {
        if (token.empty()) return;
        expression_ =
            calculator::append_token_with_implicit_multiply(expression_, token,
                                                            allow_number_continuation);
    }

    void ensure_expression_seeded_from_result()
    {
        if (expression_.empty() && !last_result_text_.empty()) {
            expression_ = last_result_text_;
            just_evaluated_ = false;
        }
    }

    EvalResult current_numeric_value() const
    {
        if (!expression_.empty()) {
            return calculator::evaluate_expression(expression_, angle_mode_);
        }
        return {true, last_result_, last_result_text_, ""};
    }

    void insert_function_token(const char *base_name)
    {
        const std::string function_name = mapped_function_name(base_name);
        prepare_for_insert(!continues_from_result(function_name));
        append_token(function_name + "(");
    }

    void insert_random_value()
    {
        prepare_for_insert(true);
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        append_token(calculator::format_value(distribution(rng_)));
    }

    void toggle_sound_enabled()
    {
        const bool was_enabled = calculator::audio_enabled();
        if (was_enabled) {
            calculator::play_cue(calculator::AudioCue::SoftClick);
        }

        calculator::set_audio_enabled(!was_enabled);

        if (!was_enabled) {
            calculator::play_cue(calculator::AudioCue::Confirm);
        }
    }

    void play_feedback_for_button(const ButtonSpec &spec) const
    {
        if (!calculator::audio_enabled()) {
            return;
        }

        if (spec.action == ActionType::Insert && spec.token != nullptr &&
            std::strlen(spec.token) == 1 &&
            std::isdigit(static_cast<unsigned char>(spec.token[0]))) {
            calculator::play_digit_cue(spec.token[0] - '0');
            return;
        }

        if (spec.action == ActionType::Evaluate) {
            calculator::play_cue(calculator::AudioCue::Confirm);
            return;
        }

        if (spec.style == ButtonStyleKind::Accent || spec.action == ActionType::ToggleAngle ||
            spec.action == ActionType::ToggleSign) {
            calculator::play_cue(calculator::AudioCue::Accent);
            return;
        }

        if (spec.style == ButtonStyleKind::Function) {
            calculator::play_cue(calculator::AudioCue::Function);
            return;
        }

        calculator::play_cue(calculator::AudioCue::SoftClick);
    }

    void play_feedback_for_printable_key(char key) const
    {
        if (!calculator::audio_enabled()) {
            return;
        }

        if (std::isdigit(static_cast<unsigned char>(key))) {
            calculator::play_digit_cue(key - '0');
            return;
        }

        if (key == '=') {
            calculator::play_cue(calculator::AudioCue::Confirm);
            return;
        }

        if (key == '+' || key == '-' || key == '*' || key == '/' || key == '^') {
            calculator::play_cue(calculator::AudioCue::Accent);
            return;
        }

        calculator::play_cue(calculator::AudioCue::SoftClick);
    }

    void toggle_sign()
    {
        reset_view_state();
        ensure_expression_seeded_from_result();
        if (expression_.empty()) {
            expression_ = "-";
            just_evaluated_ = false;
            return;
        }
        if (expression_.size() > 3 &&
            expression_.rfind("-(", 0) == 0 &&
            expression_.back() == ')') {
            expression_ = expression_.substr(2, expression_.size() - 3);
        } else {
            expression_ = "-(" + expression_ + ")";
        }
        just_evaluated_ = false;
    }

    void apply_memory(char operation)
    {
        const EvalResult value = current_numeric_value();
        if (!value.ok) {
            error_state_ = true;
            return;
        }

        if (operation == '+') memory_value_ += value.value;
        if (operation == '-') memory_value_ -= value.value;
    }

    void commit_current_expression()
    {
        reset_view_state();
        if (expression_.empty()) {
            return;
        }

        const EvalResult result = calculator::evaluate_expression(expression_, angle_mode_);
        if (!result.ok || !std::isfinite(result.value)) {
            error_state_ = true;
            just_evaluated_ = false;
            return;
        }

        last_formula_ = result.normalized_expression;
        last_result_ = result.value;
        last_result_text_ = calculator::format_value(last_result_);
        history_.push_back({last_formula_, last_result_text_});
        if (history_.size() > kMaxHistory) {
            history_.erase(history_.begin());
        }
        expression_ = last_result_text_;
        just_evaluated_ = true;
    }

    void erase_last_token()
    {
        static const std::array<const char *, 21> compound_tokens = {
            "atanh(", "acosh(", "asinh(", "atan(", "acos(", "asin(",
            "sinh(", "cosh(", "tanh(", "sqrt(", "cbrt(", "root(",
            "abs(", "sin(", "cos(", "tan(", "exp(", "log(", "ln(",
            "10^", "1e"
        };

        reset_view_state();
        just_evaluated_ = false;
        if (expression_.empty()) return;

        for (const char *token : compound_tokens) {
            const std::string suffix(token);
            if (expression_.size() >= suffix.size() &&
                expression_.compare(expression_.size() - suffix.size(), suffix.size(), suffix) == 0) {
                expression_.erase(expression_.size() - suffix.size());
                return;
            }
        }

        if (expression_.size() >= 2 &&
            (expression_.compare(expression_.size() - 2, 2, "^2") == 0 ||
             expression_.compare(expression_.size() - 2, 2, "^3") == 0 ||
             expression_.compare(expression_.size() - 2, 2, "pi") == 0)) {
            expression_.erase(expression_.size() - 2);
            return;
        }

        expression_.pop_back();
    }

    void apply_button(int row, int col)
    {
        selected_row_ = row;
        selected_col_ = col;
        const ButtonSpec &spec = kButtonSpecs[row][col];

        if (second_mode_ && spec.action == ActionType::InsertRandom) {
            toggle_sound_enabled();
            if (lv_indev_t *keyboard = app_get_keyboard_indev()) {
                lv_indev_set_group(keyboard, key_group_);
            }
            lv_group_focus_obj(root_panel_);
            refresh_display();
            refresh_focus();
            return;
        }

        switch (spec.action) {
        case ActionType::Insert: {
            const std::string token = spec.token ? spec.token : "";
            prepare_for_insert(!continues_from_result(token));
            append_token(token, is_numeric_fragment_token(token));
            break;
        }
        case ActionType::ClearAll:
            expression_.clear();
            last_formula_.clear();
            last_result_text_ = calculator::format_value(last_result_);
            error_state_ = false;
            just_evaluated_ = false;
            history_index_ = -1;
            break;
        case ActionType::Evaluate:
            commit_current_expression();
            break;
        case ActionType::MemoryClear:
            memory_value_ = 0.0;
            break;
        case ActionType::MemoryAdd:
            apply_memory('+');
            break;
        case ActionType::MemorySubtract:
            apply_memory('-');
            break;
        case ActionType::MemoryRecall:
            prepare_for_insert(true);
            append_token(calculator::format_value(memory_value_));
            break;
        case ActionType::ToggleSecond:
            second_mode_ = !second_mode_;
            break;
        case ActionType::ToggleAngle:
            angle_mode_ = angle_mode_ == AngleMode::Radian ? AngleMode::Degree : AngleMode::Radian;
            break;
        case ActionType::ToggleSign:
            toggle_sign();
            break;
        case ActionType::InsertAns:
            prepare_for_insert(true);
            append_token(last_result_text_);
            break;
        case ActionType::InsertPi:
            prepare_for_insert(true);
            append_token("pi");
            break;
        case ActionType::InsertE:
            prepare_for_insert(true);
            append_token("e");
            break;
        case ActionType::InsertScientificE:
            reset_view_state();
            if (expression_.empty() || (!std::isdigit(static_cast<unsigned char>(last_non_space())) &&
                                        last_non_space() != '.')) {
                prepare_for_insert(true);
                append_token("1e");
            } else {
                prepare_for_insert(false);
                append_token("e", true);
            }
            break;
        case ActionType::InsertRandom:
            insert_random_value();
            break;
        case ActionType::InsertSquare:
            reset_view_state();
            ensure_expression_seeded_from_result();
            if (!expression_.empty()) expression_ = "(" + expression_ + ")^2";
            just_evaluated_ = false;
            break;
        case ActionType::InsertCube:
            reset_view_state();
            ensure_expression_seeded_from_result();
            if (!expression_.empty()) expression_ = "(" + expression_ + ")^3";
            just_evaluated_ = false;
            break;
        case ActionType::InsertPower:
            prepare_for_insert(false);
            append_token("^");
            break;
        case ActionType::InsertExp:
            prepare_for_insert(true);
            append_token("exp(");
            break;
        case ActionType::InsertTenPow:
            prepare_for_insert(true);
            append_token("10^");
            break;
        case ActionType::InsertReciprocal:
            reset_view_state();
            ensure_expression_seeded_from_result();
            if (!expression_.empty()) expression_ = "1/(" + expression_ + ")";
            just_evaluated_ = false;
            break;
        case ActionType::InsertSqrt:
            prepare_for_insert(true);
            append_token(second_mode_ ? "cbrt(" : "sqrt(");
            break;
        case ActionType::InsertCbrt:
            prepare_for_insert(true);
            append_token("cbrt(");
            break;
        case ActionType::InsertYRoot:
            reset_view_state();
            ensure_expression_seeded_from_result();
            if (expression_.empty()) {
                expression_ = "root(";
            } else {
                expression_ = "root(" + calculator::close_open_parentheses(expression_) + ",";
            }
            just_evaluated_ = false;
            break;
        case ActionType::InsertPercent:
            prepare_for_insert(false);
            append_token("%");
            break;
        case ActionType::InsertFactorial:
            prepare_for_insert(false);
            append_token("!");
            break;
        case ActionType::InsertFunction:
            insert_function_token(spec.token);
            break;
        case ActionType::Backspace:
            erase_last_token();
            break;
        }

        play_feedback_for_button(spec);

        if (lv_indev_t *keyboard = app_get_keyboard_indev()) {
            lv_indev_set_group(keyboard, key_group_);
        }
        lv_group_focus_obj(root_panel_);
        refresh_display();
        refresh_focus();
    }

    void apply_action_shortcut(ActionType action)
    {
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kCols; ++col) {
                if (kButtonSpecs[row][col].action == action) {
                    apply_button(row, col);
                    return;
                }
            }
        }
    }

    void handle_printable_key(char key)
    {
        bool handled = true;
        if (std::isdigit(static_cast<unsigned char>(key)) || key == '.' || key == '(' || key == ')' || key == ',') {
            char token[2] = {key, '\0'};
            prepare_for_insert(true);
            append_token(token, true);
        } else if (key == '+' || key == '-' || key == '*' || key == '/' || key == '^' || key == '!' || key == '%') {
            char token[2] = {key, '\0'};
            prepare_for_insert(!continues_from_result(token));
            append_token(token, key == '+' || key == '-');
        } else if (key == '=') {
            commit_current_expression();
        } else if (key == ' ') {
            apply_button(selected_row_, selected_col_);
            return;
        } else if (key == 'c' || key == 'C') {
            apply_action_shortcut(ActionType::ClearAll);
            return;
        } else if (key == 's' || key == 'S') {
            toggle_sound_enabled();
            handled = false;
        } else if (key == 'p' || key == 'P') {
            prepare_for_insert(true);
            append_token("pi");
        } else if (key == 'e') {
            const char previous = last_non_space();
            if (std::isdigit(static_cast<unsigned char>(previous)) || previous == '.') {
                prepare_for_insert(false);
                append_token("e", true);
            } else {
                prepare_for_insert(true);
                append_token("e");
            }
        } else {
            handled = false;
        }

        if (handled) {
            play_feedback_for_printable_key(key);
        }
        refresh_display();
    }

    void handle_key(uint32_t key)
    {
        switch (key) {
        case LV_KEY_LEFT:
            move_selection(0, -1);
            return;
        case LV_KEY_RIGHT:
            move_selection(0, 1);
            return;
        case LV_KEY_UP:
            move_selection(-1, 0);
            return;
        case LV_KEY_DOWN:
            move_selection(1, 0);
            return;
        case LV_KEY_ENTER:
            commit_current_expression();
            play_feedback_for_printable_key('=');
            return;
        case LV_KEY_BACKSPACE:
        case LV_KEY_DEL:
            apply_action_shortcut(ActionType::Backspace);
            return;
        case LV_KEY_ESC:
            app_request_quit();
            return;
        case LV_KEY_PREV:
            browse_history(-1);
            refresh_display();
            return;
        case LV_KEY_NEXT:
            browse_history(1);
            refresh_display();
            return;
        default:
            break;
        }

        if (key >= 32 && key < 127) {
            handle_printable_key(static_cast<char>(key));
        }
    }

    static void handle_button_click(lv_event_t *event)
    {
        auto *context = static_cast<ButtonContext *>(lv_event_get_user_data(event));
        if (context != nullptr && context->app != nullptr) {
            context->app->apply_button(context->row, context->col);
        }
    }

    static void handle_focus_overlay_click(lv_event_t *event)
    {
        auto *self = static_cast<CalculatorApp *>(lv_event_get_user_data(event));
        if (self == nullptr) {
            return;
        }
        self->apply_button(self->selected_row_, self->selected_col_);
    }

    static void handle_root_event(lv_event_t *event)
    {
        if (lv_event_get_code(event) != LV_EVENT_KEY) {
            return;
        }
        auto *self = static_cast<CalculatorApp *>(lv_event_get_user_data(event));
        if (self == nullptr) {
            return;
        }
        self->handle_key(lv_event_get_key(event));
        self->refresh_display();
        self->refresh_focus();
    }

    lv_obj_t *screen_ = nullptr;
    lv_obj_t *root_panel_ = nullptr;
    lv_obj_t *display_panel_ = nullptr;
    lv_obj_t *keypad_panel_ = nullptr;
    lv_obj_t *focus_overlay_ = nullptr;
    lv_obj_t *focus_overlay_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *hint_label_ = nullptr;
    lv_obj_t *expression_label_ = nullptr;
    lv_obj_t *equals_label_ = nullptr;
    lv_obj_t *result_label_ = nullptr;
    lv_group_t *key_group_ = nullptr;
    std::array<std::array<lv_obj_t *, kCols>, kRows> button_matrix_{};
    std::array<std::array<lv_obj_t *, kCols>, kRows> label_matrix_{};
    std::array<std::array<ButtonContext, kCols>, kRows> button_contexts_{};
    std::vector<HistoryEntry> history_;
    std::string expression_;
    std::string last_formula_;
    std::string last_result_text_ = "0";
    int selected_row_ = 0;
    int selected_col_ = 0;
    lv_coord_t button_width_ = 0;
    lv_coord_t button_height_ = 0;
    lv_coord_t expression_area_width_ = 0;
    lv_coord_t equals_area_x_ = 0;
    lv_coord_t result_area_x_ = 0;
    lv_coord_t baseline_result_area_width_ = kResultAreaMinWidth;
    lv_coord_t result_area_width_ = kResultAreaMinWidth;
    int history_index_ = -1;
    AngleMode angle_mode_ = AngleMode::Radian;
    bool second_mode_ = false;
    bool just_evaluated_ = false;
    bool error_state_ = false;
    double last_result_ = 0.0;
    double memory_value_ = 0.0;
    std::mt19937 rng_;
};

CalculatorApp *g_calculator_app = nullptr;

}  // namespace

extern "C" void calculator_ui_build(lv_obj_t *screen)
{
    delete g_calculator_app;
    g_calculator_app = new CalculatorApp(screen);
}
