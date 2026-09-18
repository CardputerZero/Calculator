#include "calculator_engine.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

static int failures = 0;

void expect_value(const std::string &expression, double expected, double tolerance = 1e-10)
{
    const auto result = calculator::evaluate_expression(expression);
    if (!result.ok || std::fabs(result.value - expected) > tolerance) {
        std::cerr << "FAIL " << expression << ": ok=" << result.ok
                  << " value=" << result.value << " expected=" << expected << '\n';
        ++failures;
    }
}

void expect_error(const std::string &expression)
{
    const auto result = calculator::evaluate_expression(expression);
    if (result.ok) {
        std::cerr << "FAIL " << expression << ": expected error, got " << result.value << '\n';
        ++failures;
    }
}

void expect_format(double value, const std::string &expected)
{
    const std::string actual = calculator::format_value(value);
    if (actual != expected) {
        std::cerr << "FAIL format " << value << ": got " << actual
                  << " expected " << expected << '\n';
        ++failures;
    }
}

void expect_formatted_expression(const std::string &expression, const std::string &expected)
{
    const auto result = calculator::evaluate_expression(expression);
    if (!result.ok) {
        std::cerr << "FAIL format " << expression << ": evaluation failed\n";
        ++failures;
        return;
    }
    expect_format(result.value, expected);
}

int main()
{
    expect_value("1+2", 3);
    expect_value("3*2", 6);
    expect_value("1/8", 0.125);
    expect_value("2^10", 1024);
    expect_value("2^3^2", 512);
    expect_value("(1+2)*3", 9);
    expect_value("-2^2", -4);
    expect_value("2^-3", 0.125);
    expect_value(".5*4", 2);
    expect_value("5.*2", 10);
    expect_error("1/0");
    expect_error("1+");
    expect_error("1++2");
    expect_error("(1+2");
    expect_error("1.2.3");
    expect_formatted_expression("77777.777*8", "622222.216");
    expect_formatted_expression("0.1+0.2", "0.3");
    expect_format(2.0 / 3.0, "0.666666666666667");
    expect_format(123.456789012345, "123.456789012345");
    expect_format(1.0e-13, "1e-13");
    expect_format(1.23456789012345e20, "1.23456789012345e+20");
    expect_format(-0.0, "0");
    expect_format(std::numeric_limits<double>::infinity(), "error");
    if (failures == 0) std::cout << "calculator engine tests passed\n";
    return failures == 0 ? 0 : 1;
}
