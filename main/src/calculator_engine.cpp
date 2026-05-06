#include "calculator_engine.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace calculator {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

std::string trim_trailing_zeros(std::string text)
{
    if (text.find_first_of("eE") != std::string::npos) {
        return text;
    }

    if (text.find('.') == std::string::npos) {
        return text;
    }

    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? "0" : text;
}

class ExpressionParser {
public:
    ExpressionParser(std::string expression, AngleMode angle_mode)
        : expression_(std::move(expression)), angle_mode_(angle_mode)
    {
    }

    EvalResult evaluate()
    {
        pos_ = 0;
        error_message_.clear();
        const double value = parse_expression();
        skip_spaces();
        if (error_message_.empty() && pos_ != expression_.size()) {
            set_error("Unexpected trailing input");
        }
        return {error_message_.empty(), value, expression_, error_message_};
    }

private:
    void skip_spaces()
    {
        while (pos_ < expression_.size() &&
               std::isspace(static_cast<unsigned char>(expression_[pos_]))) {
            ++pos_;
        }
    }

    void set_error(const char *message)
    {
        if (error_message_.empty()) {
            error_message_ = message;
        }
    }

    bool match(char expected)
    {
        skip_spaces();
        if (pos_ < expression_.size() && expression_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    char peek()
    {
        skip_spaces();
        return pos_ < expression_.size() ? expression_[pos_] : '\0';
    }

    double parse_expression()
    {
        double value = parse_term();
        while (error_message_.empty()) {
            if (match('+')) {
                value += parse_term();
            } else if (match('-')) {
                value -= parse_term();
            } else {
                break;
            }
        }
        return value;
    }

    double parse_term()
    {
        double value = parse_power();
        while (error_message_.empty()) {
            if (match('*')) {
                value *= parse_power();
            } else if (match('/')) {
                const double rhs = parse_power();
                if (std::fabs(rhs) < 1e-12) {
                    set_error("Division by zero");
                    return 0.0;
                }
                value /= rhs;
            } else {
                break;
            }
        }
        return value;
    }

    double parse_power()
    {
        double value = parse_unary();
        if (match('^')) {
            const double rhs = parse_power();
            value = std::pow(value, rhs);
            if (!std::isfinite(value)) {
                set_error("Power out of range");
                return 0.0;
            }
        }
        return value;
    }

    double parse_unary()
    {
        if (match('+')) {
            return parse_unary();
        }
        if (match('-')) {
            return -parse_unary();
        }
        return parse_postfix();
    }

    double parse_postfix()
    {
        double value = parse_primary();
        while (error_message_.empty()) {
            if (match('!')) {
                value = factorial(value);
            } else if (match('%')) {
                value *= 0.01;
            } else {
                break;
            }
        }
        return value;
    }

    double parse_primary()
    {
        skip_spaces();
        if (match('(')) {
            const double value = parse_expression();
            if (!match(')')) {
                set_error("Missing ')'");
                return 0.0;
            }
            return value;
        }

        if (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.') {
            return parse_number();
        }

        if (std::isalpha(static_cast<unsigned char>(peek()))) {
            const std::string identifier = parse_identifier();

            if (identifier == "pi") return kPi;
            if (identifier == "e") return kE;

            if (!match('(')) {
                set_error("Function call missing '('");
                return 0.0;
            }

            const double arg1 = parse_expression();
            if (!error_message_.empty()) return 0.0;

            if (match(',')) {
                const double arg2 = parse_expression();
                if (!match(')')) {
                    set_error("Missing ')'");
                    return 0.0;
                }
                return apply_binary_function(identifier, arg1, arg2);
            }

            if (!match(')')) {
                set_error("Missing ')'");
                return 0.0;
            }
            return apply_unary_function(identifier, arg1);
        }

        set_error("Expected a number or function");
        return 0.0;
    }

    double parse_number()
    {
        skip_spaces();
        const char *start = expression_.c_str() + pos_;
        char *end = nullptr;
        const double value = std::strtod(start, &end);
        if (end == start) {
            set_error("Invalid number");
            return 0.0;
        }
        pos_ += static_cast<size_t>(end - start);
        return value;
    }

    std::string parse_identifier()
    {
        skip_spaces();
        const size_t start = pos_;
        while (pos_ < expression_.size() &&
               std::isalpha(static_cast<unsigned char>(expression_[pos_]))) {
            ++pos_;
        }
        return expression_.substr(start, pos_ - start);
    }

    double factorial(double value)
    {
        if (value < 0.0) {
            set_error("Factorial requires a non-negative integer");
            return 0.0;
        }

        const double rounded = std::round(value);
        if (std::fabs(rounded - value) > 1e-9 || rounded > 170.0) {
            set_error("Factorial out of range");
            return 0.0;
        }

        double result = 1.0;
        for (int i = 2; i <= static_cast<int>(rounded); ++i) {
            result *= i;
        }
        return result;
    }

    double to_radians(double value) const
    {
        if (angle_mode_ == AngleMode::Radian) {
            return value;
        }
        return value * kPi / 180.0;
    }

    double from_radians(double value) const
    {
        if (angle_mode_ == AngleMode::Radian) {
            return value;
        }
        return value * 180.0 / kPi;
    }

    double safe_root(double radicand, double degree)
    {
        if (std::fabs(degree) < 1e-12) {
            set_error("Root degree cannot be zero");
            return 0.0;
        }

        if (radicand < 0.0) {
            const double rounded = std::round(degree);
            if (std::fabs(rounded - degree) > 1e-9 ||
                std::fmod(std::fabs(rounded), 2.0) < 1e-9) {
                set_error("Invalid root domain");
                return 0.0;
            }
            return -std::pow(-radicand, 1.0 / degree);
        }

        return std::pow(radicand, 1.0 / degree);
    }

    double finalize_function_result(double value)
    {
        if (!std::isfinite(value)) {
            set_error("Invalid function domain");
            return 0.0;
        }
        return value;
    }

    double apply_unary_function(const std::string &name, double argument)
    {
        if (name == "sin") return finalize_function_result(std::sin(to_radians(argument)));
        if (name == "cos") return finalize_function_result(std::cos(to_radians(argument)));
        if (name == "tan") return finalize_function_result(std::tan(to_radians(argument)));
        if (name == "asin") return finalize_function_result(from_radians(std::asin(argument)));
        if (name == "acos") return finalize_function_result(from_radians(std::acos(argument)));
        if (name == "atan") return finalize_function_result(from_radians(std::atan(argument)));
        if (name == "sinh") return finalize_function_result(std::sinh(argument));
        if (name == "cosh") return finalize_function_result(std::cosh(argument));
        if (name == "tanh") return finalize_function_result(std::tanh(argument));
        if (name == "asinh") return finalize_function_result(std::asinh(argument));
        if (name == "acosh") return finalize_function_result(std::acosh(argument));
        if (name == "atanh") return finalize_function_result(std::atanh(argument));
        if (name == "ln") return finalize_function_result(std::log(argument));
        if (name == "log") return finalize_function_result(std::log10(argument));
        if (name == "logtwo") return finalize_function_result(std::log2(argument));
        if (name == "sqrt") return finalize_function_result(std::sqrt(argument));
        if (name == "cbrt") return finalize_function_result(std::cbrt(argument));
        if (name == "abs") return finalize_function_result(std::fabs(argument));
        if (name == "exp") return finalize_function_result(std::exp(argument));

        set_error("Unsupported function");
        return 0.0;
    }

    double apply_binary_function(const std::string &name, double arg1, double arg2)
    {
        if (name == "root") {
            return finalize_function_result(safe_root(arg1, arg2));
        }

        set_error("Unsupported binary function");
        return 0.0;
    }

    std::string expression_;
    size_t pos_ = 0;
    AngleMode angle_mode_ = AngleMode::Radian;
    std::string error_message_;
};

}  // namespace

std::string format_value(double value)
{
    if (std::fabs(value) < 1e-12) {
        value = 0.0;
    }

    if (!std::isfinite(value)) {
        return "ERR";
    }

    std::ostringstream oss;
    oss << std::setprecision(12) << value;
    return trim_trailing_zeros(oss.str());
}

std::string close_open_parentheses(const std::string &expression)
{
    int balance = 0;
    std::string normalized;
    normalized.reserve(expression.size() + 4);

    for (char ch : expression) {
        if (ch == '(') {
            ++balance;
            normalized.push_back(ch);
        } else if (ch == ')') {
            if (balance > 0) {
                --balance;
                normalized.push_back(ch);
            }
        } else {
            normalized.push_back(ch);
        }
    }

    while (balance-- > 0) {
        normalized.push_back(')');
    }

    return normalized;
}

EvalResult evaluate_expression(const std::string &expression, AngleMode angle_mode)
{
    const std::string normalized = close_open_parentheses(expression);
    ExpressionParser parser(normalized, angle_mode);
    EvalResult result = parser.evaluate();
    result.normalized_expression = normalized;
    if (result.ok && !std::isfinite(result.value)) {
        result.ok = false;
        result.error_message = "Result is not finite";
    }
    return result;
}

}  // namespace calculator
