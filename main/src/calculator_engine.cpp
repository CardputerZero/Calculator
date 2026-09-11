#include "calculator_engine.h"

#include <cmath>
#include <limits>
#include <sstream>

namespace calculator {
namespace {

class Parser {
public:
    explicit Parser(const std::string &text) : text_(text), position_(0) {}

    bool parse(double &value)
    {
        if (!parse_expression(value)) {
            return false;
        }
        return position_ == text_.size();
    }

private:
    bool parse_expression(double &value)
    {
        if (!parse_term(value)) {
            return false;
        }

        while (position_ < text_.size()) {
            const char op = text_[position_];
            if (op != '+' && op != '-') {
                break;
            }
            ++position_;

            double right = 0.0;
            if (!parse_term(right)) {
                return false;
            }
            if (op == '+') {
                value += right;
            } else {
                value -= right;
            }
        }
        return true;
    }

    bool parse_term(double &value)
    {
        if (!parse_factor(value)) {
            return false;
        }

        while (position_ < text_.size()) {
            const char op = text_[position_];
            if (op != '*' && op != '/') {
                break;
            }
            ++position_;

            double right = 0.0;
            if (!parse_factor(right)) {
                return false;
            }
            if (op == '*') {
                value *= right;
            } else {
                if (right == 0.0) {
                    return false;
                }
                value /= right;
            }
        }
        return true;
    }

    bool parse_factor(double &value)
    {
        if (position_ < text_.size() && text_[position_] == '-') {
            ++position_;
            if (!parse_factor(value)) {
                return false;
            }
            value = -value;
            return true;
        }

        if (!parse_primary(value)) {
            return false;
        }

        if (position_ < text_.size() && text_[position_] == '^') {
            ++position_;
            double exponent = 0.0;
            if (!parse_factor(exponent)) {
                return false;
            }
            value = std::pow(value, exponent);
        }
        return true;
    }

    bool parse_primary(double &value)
    {
        if (position_ < text_.size() && text_[position_] == '(') {
            ++position_;
            if (!parse_expression(value)) {
                return false;
            }
            if (position_ >= text_.size() || text_[position_] != ')') {
                return false;
            }
            ++position_;
            return true;
        }

        const size_t start = position_;
        bool has_digit = false;
        bool has_dot = false;
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (ch >= '0' && ch <= '9') {
                has_digit = true;
            } else if (ch == '.') {
                if (has_dot) {
                    return false;
                }
                has_dot = true;
            } else {
                break;
            }
            ++position_;
        }

        if (!has_digit || position_ == start) {
            position_ = start;
            return false;
        }

        const std::string number = text_.substr(start, position_ - start);
        std::istringstream stream(number);
        stream.imbue(std::locale::classic());
        stream >> value;
        return !stream.fail() && stream.eof();
    }

    const std::string &text_;
    size_t position_;
};

std::string trim_zeros(std::string text)
{
    const size_t dot = text.find('.');
    if (dot != std::string::npos) {
        size_t last = text.size();
        while (last > dot + 1 && text[last - 1] == '0') {
            --last;
        }
        if (last > dot && text[last - 1] == '.') {
            --last;
        }
        text.resize(last);
    }
    return text;
}

}  // namespace

std::string format_value(double value)
{
    if (!std::isfinite(value)) {
        return "error";
    }
    if (value == 0.0) {
        return "0";
    }

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.12f", value);
    std::string result = trim_zeros(buffer);
    if (result == "-0") {
        result = "0";
    }
    return result;
}

EvalResult evaluate_expression(const std::string &expression)
{
    EvalResult result;
    if (expression.empty()) {
        result.error_message = "empty";
        return result;
    }

    Parser parser(expression);
    double value = 0.0;
    if (!parser.parse(value) || !std::isfinite(value)) {
        result.error_message = "syntax";
        return result;
    }

    result.ok = true;
    result.value = value;
    return result;
}

}  // namespace calculator
