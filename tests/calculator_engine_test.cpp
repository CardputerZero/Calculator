#include "calculator_engine.h"
#include <cmath>
#include <iostream>
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
    if (calculator::format_value(2.0 / 3.0) != "0.666666666667") ++failures;
    if (calculator::format_value(-0.0) != "0") ++failures;
    if (failures == 0) std::cout << "calculator engine tests passed\n";
    return failures == 0 ? 0 : 1;
}
