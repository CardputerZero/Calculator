#include "calculator_engine.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using calculator::AngleMode;
using calculator::EvalResult;

struct SuccessCase {
    std::string expression;
    AngleMode angle_mode;
    double expected;
    double tolerance;
};

struct FailureCase {
    std::string expression;
    AngleMode angle_mode;
};

struct FormatCase {
    double value;
    std::string expected;
};

bool nearly_equal(double lhs, double rhs, double tolerance)
{
    return std::fabs(lhs - rhs) <= tolerance;
}

bool run_success_case(const SuccessCase &test_case)
{
    const EvalResult result =
        calculator::evaluate_expression(test_case.expression, test_case.angle_mode);
    if (!result.ok) {
        std::cerr << "Expected success but failed: " << test_case.expression
                  << " error=" << result.error_message << "\n";
        return false;
    }

    if (!nearly_equal(result.value, test_case.expected, test_case.tolerance)) {
        std::cerr << "Unexpected result for " << test_case.expression
                  << " expected=" << test_case.expected
                  << " actual=" << result.value << "\n";
        return false;
    }

    return true;
}

bool run_failure_case(const FailureCase &test_case)
{
    const EvalResult result =
        calculator::evaluate_expression(test_case.expression, test_case.angle_mode);
    if (result.ok) {
        std::cerr << "Expected failure but succeeded: " << test_case.expression
                  << " value=" << result.value << "\n";
        return false;
    }
    return true;
}

bool run_format_case(const FormatCase &test_case)
{
    const std::string formatted = calculator::format_value(test_case.value);
    if (formatted != test_case.expected) {
        std::cerr << "Unexpected formatted value. expected=" << test_case.expected
                  << " actual=" << formatted << "\n";
        return false;
    }
    return true;
}

}  // namespace

int main()
{
    const std::vector<SuccessCase> success_cases = {
        {"1+2*3", AngleMode::Radian, 7.0, 1e-9},
        {"(2+3)*4", AngleMode::Radian, 20.0, 1e-9},
        {"2^3^2", AngleMode::Radian, 512.0, 1e-9},
        {"sqrt(81)", AngleMode::Radian, 9.0, 1e-9},
        {"cbrt(27)", AngleMode::Radian, 3.0, 1e-9},
        {"root(81,4)", AngleMode::Radian, 3.0, 1e-9},
        {"root(-27,3)", AngleMode::Radian, -3.0, 1e-9},
        {"5!", AngleMode::Radian, 120.0, 1e-9},
        {"abs(-(2+5))", AngleMode::Radian, 7.0, 1e-9},
        {"exp(1)", AngleMode::Radian, std::exp(1.0), 1e-9},
        {"ln(e)", AngleMode::Radian, 1.0, 1e-9},
        {"log(1000)", AngleMode::Radian, 3.0, 1e-9},
        {"logtwo(32)", AngleMode::Radian, 5.0, 1e-9},
        {"10^3", AngleMode::Radian, 1000.0, 1e-9},
        {"1e3+2", AngleMode::Radian, 1002.0, 1e-9},
        {"50%", AngleMode::Radian, 0.5, 1e-9},
        {"sin(pi/2)", AngleMode::Radian, 1.0, 1e-9},
        {"cos(pi)", AngleMode::Radian, -1.0, 1e-9},
        {"tan(pi/4)", AngleMode::Radian, 1.0, 1e-9},
        {"sin(30)", AngleMode::Degree, 0.5, 1e-9},
        {"cos(60)", AngleMode::Degree, 0.5, 1e-9},
        {"asin(0.5)", AngleMode::Degree, 30.0, 1e-9},
        {"acos(0.5)", AngleMode::Degree, 60.0, 1e-9},
        {"atan(1)", AngleMode::Degree, 45.0, 1e-9},
        {"sinh(0)", AngleMode::Radian, 0.0, 1e-9},
        {"cosh(0)", AngleMode::Radian, 1.0, 1e-9},
        {"tanh(0)", AngleMode::Radian, 0.0, 1e-9},
        {"asinh(0)", AngleMode::Radian, 0.0, 1e-9},
        {"acosh(1)", AngleMode::Radian, 0.0, 1e-9},
        {"atanh(0.5)", AngleMode::Radian, std::atanh(0.5), 1e-9},
        {"2*(3+4", AngleMode::Radian, 14.0, 1e-9},
    };

    const std::vector<FailureCase> failure_cases = {
        {"1/0", AngleMode::Radian},
        {"sqrt(-1)", AngleMode::Radian},
        {"ln(-1)", AngleMode::Radian},
        {"asin(2)", AngleMode::Radian},
        {"root(-16,2)", AngleMode::Radian},
        {"(-1)!", AngleMode::Radian},
        {"(", AngleMode::Radian},
    };

    const std::vector<FormatCase> format_cases = {
        {1000.0, "1000"},
        {1.0 / 4356.0, "0.000229568411387"},
        {-1.0 / 4356.0, "-0.000229568411387"},
    };

    int failure_count = 0;

    for (const SuccessCase &test_case : success_cases) {
        if (!run_success_case(test_case)) {
            ++failure_count;
        }
    }

    for (const FailureCase &test_case : failure_cases) {
        if (!run_failure_case(test_case)) {
            ++failure_count;
        }
    }

    for (const FormatCase &test_case : format_cases) {
        if (!run_format_case(test_case)) {
            ++failure_count;
        }
    }

    if (failure_count != 0) {
        std::cerr << failure_count << " calculator engine test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Passed "
              << success_cases.size() + failure_cases.size() + format_cases.size()
              << " calculator engine tests\n";
    return EXIT_SUCCESS;
}
