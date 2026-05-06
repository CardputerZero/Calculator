#ifndef CALCULATOR_ENGINE_H
#define CALCULATOR_ENGINE_H

#include <string>

namespace calculator {

enum class AngleMode {
    Radian,
    Degree,
};

struct EvalResult {
    bool ok = false;
    double value = 0.0;
    std::string normalized_expression;
    std::string error_message;
};

std::string format_value(double value);
std::string close_open_parentheses(const std::string &expression);
EvalResult evaluate_expression(const std::string &expression, AngleMode angle_mode);

}  // namespace calculator

#endif
