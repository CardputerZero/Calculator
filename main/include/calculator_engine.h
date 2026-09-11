#ifndef CALCULATOR_ENGINE_H
#define CALCULATOR_ENGINE_H

#include <string>

namespace calculator {

struct EvalResult {
    bool ok = false;
    double value = 0.0;
    std::string error_message;
};

std::string format_value(double value);
EvalResult evaluate_expression(const std::string &expression);

}  // namespace calculator

#endif
