#include "calculator_input.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct AppendCase {
    std::vector<std::string> tokens;
    std::vector<bool> allow_number_continuation;
    std::string expected;
};

}  // namespace

int main()
{
    const std::vector<AppendCase> cases = {
        {{"(", "1", "+", "9", ")", "*", "2", "0"},
         {false, true, false, true, false, false, true, true},
         "(1+9)*20"},
        {{"1", "e", "3"}, {true, true, true}, "1e3"},
        {{"1", "e", "-", "3"}, {true, true, true, true}, "1e-3"},
        {{"2", "(", "3", "+", "4", ")"}, {true, false, true, false, true, false}, "2*(3+4)"},
        {{"2", "pi"}, {true, false}, "2*pi"},
        {{"3", ".", "1", "4"}, {true, true, true, true}, "3.14"},
        {{")", "1", "+", "2"}, {false, true, false, true}, "1+2"},
        {{"(", "1", ")", ")"}, {false, true, false, false}, "(1)"},
        {{"1", ".", ".", "2"}, {true, true, true, true}, "1.2"},
        {{".", "5", "+", ".", "2"}, {true, true, false, true, true}, "0.5+0.2"},
        {{"1", "+", "*", "2"}, {true, false, false, true}, "1*2"},
        {{"1", "e", "e", "3"}, {true, true, true, true}, "1e3"},
    };

    int failure_count = 0;

    for (const AppendCase &test_case : cases) {
        std::string expression;
        for (size_t i = 0; i < test_case.tokens.size(); ++i) {
            expression = calculator::append_token_with_implicit_multiply(
                expression, test_case.tokens[i], test_case.allow_number_continuation[i]);
        }

        if (expression != test_case.expected) {
            std::cerr << "Unexpected assembled expression. expected=" << test_case.expected
                      << " actual=" << expression << "\n";
            ++failure_count;
        }
    }

    if (failure_count != 0) {
        return EXIT_FAILURE;
    }

    std::cout << "Passed " << cases.size() << " calculator input tests\n";
    return EXIT_SUCCESS;
}
