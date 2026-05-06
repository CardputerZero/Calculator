#include "calculator_input.h"

#include <cctype>
#include <string>

namespace calculator {
namespace {

int last_non_space_index(const std::string &text, int before = -1)
{
    int index = before >= 0 ? before : static_cast<int>(text.size()) - 1;
    while (index >= 0 &&
           std::isspace(static_cast<unsigned char>(text[static_cast<size_t>(index)]))) {
        --index;
    }
    return index;
}

bool is_digit_or_dot(char ch)
{
    return std::isdigit(static_cast<unsigned char>(ch)) || ch == '.';
}

bool is_binary_operator(char ch)
{
    return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^';
}

bool is_postfix_operator(char ch)
{
    return ch == '!' || ch == '%';
}

bool is_value_terminator(char ch)
{
    return std::isdigit(static_cast<unsigned char>(ch)) || ch == ')' ||
           std::isalpha(static_cast<unsigned char>(ch)) || is_postfix_operator(ch);
}

bool ends_with_scientific_marker(const std::string &expression, int marker_index)
{
    if (marker_index < 1) {
        return false;
    }

    const int value_index = last_non_space_index(expression, marker_index - 1);
    if (value_index < 0) {
        return false;
    }

    return is_digit_or_dot(expression[static_cast<size_t>(value_index)]);
}

bool ends_with_incomplete_number(const std::string &expression)
{
    const int previous_index = last_non_space_index(expression);
    if (previous_index < 0) {
        return false;
    }

    const char previous = expression[static_cast<size_t>(previous_index)];
    if (previous == '.') {
        return true;
    }

    if ((previous == 'e' || previous == 'E') &&
        ends_with_scientific_marker(expression, previous_index)) {
        return true;
    }

    if ((previous == '+' || previous == '-') && previous_index >= 1) {
        const int marker_index = last_non_space_index(expression, previous_index - 1);
        if (marker_index >= 0) {
            const char marker = expression[static_cast<size_t>(marker_index)];
            if ((marker == 'e' || marker == 'E') &&
                ends_with_scientific_marker(expression, marker_index)) {
                return true;
            }
        }
    }

    return false;
}

int current_number_start_index(const std::string &expression)
{
    int index = last_non_space_index(expression);
    if (index < 0) {
        return -1;
    }

    while (index >= 0) {
        const char ch = expression[static_cast<size_t>(index)];
        if (is_digit_or_dot(ch) || ch == 'e' || ch == 'E') {
            --index;
            continue;
        }

        if ((ch == '+' || ch == '-') && index > 0) {
            const int marker_index = last_non_space_index(expression, index - 1);
            if (marker_index >= 0) {
                const char marker = expression[static_cast<size_t>(marker_index)];
                if ((marker == 'e' || marker == 'E') &&
                    ends_with_scientific_marker(expression, marker_index)) {
                    --index;
                    continue;
                }
            }
        }
        break;
    }

    return index + 1;
}

bool current_number_has_decimal(const std::string &expression)
{
    const int start = current_number_start_index(expression);
    const int end = last_non_space_index(expression);
    if (start < 0 || end < start) {
        return false;
    }

    for (int index = start; index <= end; ++index) {
        const char ch = expression[static_cast<size_t>(index)];
        if (ch == 'e' || ch == 'E') {
            return false;
        }
        if (ch == '.') {
            return true;
        }
    }
    return false;
}

bool current_number_has_exponent(const std::string &expression)
{
    const int start = current_number_start_index(expression);
    const int end = last_non_space_index(expression);
    if (start < 0 || end < start) {
        return false;
    }

    for (int index = start; index <= end; ++index) {
        const char ch = expression[static_cast<size_t>(index)];
        if (ch == 'e' || ch == 'E') {
            return true;
        }
    }
    return false;
}

int unmatched_open_parentheses(const std::string &expression)
{
    int balance = 0;
    for (char ch : expression) {
        if (ch == '(') {
            ++balance;
        } else if (ch == ')' && balance > 0) {
            --balance;
        }
    }
    return balance;
}

bool can_start_number_literal(const std::string &expression)
{
    const int previous_index = last_non_space_index(expression);
    if (previous_index < 0) {
        return true;
    }

    const char previous = expression[static_cast<size_t>(previous_index)];
    return previous == '(' || previous == ',' || is_binary_operator(previous);
}

bool can_close_parenthesis(const std::string &expression)
{
    if (unmatched_open_parentheses(expression) <= 0 || ends_with_incomplete_number(expression)) {
        return false;
    }

    const int previous_index = last_non_space_index(expression);
    if (previous_index < 0) {
        return false;
    }

    return is_value_terminator(expression[static_cast<size_t>(previous_index)]);
}

bool can_append_postfix(const std::string &expression, char token)
{
    if (ends_with_incomplete_number(expression)) {
        return false;
    }

    const int previous_index = last_non_space_index(expression);
    if (previous_index < 0) {
        return false;
    }

    const char previous = expression[static_cast<size_t>(previous_index)];
    if (!is_value_terminator(previous)) {
        return false;
    }

    if ((token == '!' && previous == '!') || (token == '%' && previous == '%')) {
        return false;
    }

    return true;
}

std::string append_binary_operator(const std::string &expression, char token)
{
    if (ends_with_incomplete_number(expression)) {
        return expression;
    }

    const int previous_index = last_non_space_index(expression);
    if (previous_index < 0) {
        return token == '-' ? std::string(1, token) : expression;
    }

    const char previous = expression[static_cast<size_t>(previous_index)];
    if (previous == '(' || previous == ',') {
        return token == '-' ? expression + token : expression;
    }

    if (is_binary_operator(previous)) {
        const int before_previous = last_non_space_index(expression, previous_index - 1);
        if (before_previous < 0 ||
            expression[static_cast<size_t>(before_previous)] == '(' ||
            expression[static_cast<size_t>(before_previous)] == ',') {
            if (token == '-' && previous != '-') {
                std::string next = expression;
                next[static_cast<size_t>(previous_index)] = token;
                return next;
            }
            return expression;
        }

        std::string next = expression;
        next[static_cast<size_t>(previous_index)] = token;
        return next;
    }

    return is_value_terminator(previous) ? expression + token : expression;
}

}  // namespace

bool token_continues_number(const std::string &expression, const std::string &token)
{
    if (expression.empty() || token.size() != 1) {
        return false;
    }

    const char token_char = token.front();
    const int previous_index = last_non_space_index(expression);
    if (previous_index < 0) {
        return false;
    }

    const char previous = expression[static_cast<size_t>(previous_index)];

    if (std::isdigit(static_cast<unsigned char>(token_char))) {
        if (std::isdigit(static_cast<unsigned char>(previous)) || previous == '.') {
            return true;
        }
        if ((previous == 'e' || previous == 'E') &&
            ends_with_scientific_marker(expression, previous_index)) {
            return true;
        }
        if ((previous == '+' || previous == '-') && previous_index >= 1) {
            const int marker_index = last_non_space_index(expression, previous_index - 1);
            if (marker_index >= 0) {
                const char marker = expression[static_cast<size_t>(marker_index)];
                if ((marker == 'e' || marker == 'E') &&
                    ends_with_scientific_marker(expression, marker_index)) {
                    return true;
                }
            }
        }
        return false;
    }

    if (token_char == '.') {
        return std::isdigit(static_cast<unsigned char>(previous));
    }

    if ((token_char == 'e' || token_char == 'E')) {
        return std::isdigit(static_cast<unsigned char>(previous)) || previous == '.';
    }

    if ((token_char == '+' || token_char == '-') &&
        (previous == 'e' || previous == 'E') &&
        ends_with_scientific_marker(expression, previous_index)) {
        return true;
    }

    return false;
}

bool needs_implicit_multiply(const std::string &expression, const std::string &token,
                             bool allow_number_continuation)
{
    if (expression.empty() || token.empty() || ends_with_incomplete_number(expression)) {
        return false;
    }

    if (allow_number_continuation && token_continues_number(expression, token)) {
        return false;
    }

    const int previous_index = last_non_space_index(expression);
    if (previous_index < 0) {
        return false;
    }

    const char previous = expression[static_cast<size_t>(previous_index)];
    const char first = token.front();
    const bool previous_is_value = is_value_terminator(previous) || previous == '.';
    const bool token_starts_value =
        std::isdigit(static_cast<unsigned char>(first)) || first == '.' ||
        first == '(' || std::isalpha(static_cast<unsigned char>(first));

    return previous_is_value && token_starts_value;
}

std::string append_token_with_implicit_multiply(const std::string &expression,
                                                const std::string &token,
                                                bool allow_number_continuation)
{
    if (token.empty()) {
        return expression;
    }

    if (token == ",") {
        return expression;
    }

    if (allow_number_continuation && token.size() == 1) {
        const char token_char = token.front();

        if (std::isdigit(static_cast<unsigned char>(token_char))) {
            if (token_continues_number(expression, token) || ends_with_incomplete_number(expression)) {
                return expression + token;
            }
        } else if (token_char == '.') {
            if (token_continues_number(expression, token)) {
                if (current_number_has_decimal(expression) || current_number_has_exponent(expression)) {
                    return expression;
                }
                return expression + token;
            }

            if (can_start_number_literal(expression)) {
                return append_token_with_implicit_multiply(expression, "0.", false);
            }
            return expression;
        } else if (token_char == 'e' || token_char == 'E') {
            if (current_number_has_exponent(expression)) {
                return expression;
            }
            if (token_continues_number(expression, token)) {
                return expression + token;
            }
            if (ends_with_incomplete_number(expression)) {
                return expression;
            }
        } else if (token_char == '+' || token_char == '-') {
            if (token_continues_number(expression, token)) {
                const int previous_index = last_non_space_index(expression);
                if (previous_index >= 0 &&
                    (expression[static_cast<size_t>(previous_index)] == '+' ||
                     expression[static_cast<size_t>(previous_index)] == '-')) {
                    const int marker_index = last_non_space_index(expression, previous_index - 1);
                    if (marker_index >= 0) {
                        const char marker = expression[static_cast<size_t>(marker_index)];
                        if ((marker == 'e' || marker == 'E') &&
                            ends_with_scientific_marker(expression, marker_index)) {
                            std::string next = expression;
                            next[static_cast<size_t>(previous_index)] = token_char;
                            return next;
                        }
                    }
                }
                return expression + token;
            }
        }
    }

    if (token == ")") {
        return can_close_parenthesis(expression) ? expression + token : expression;
    }

    if (token.size() == 1 && is_binary_operator(token.front())) {
        return append_binary_operator(expression, token.front());
    }

    if (token.size() == 1 && is_postfix_operator(token.front())) {
        return can_append_postfix(expression, token.front()) ? expression + token : expression;
    }

    if (ends_with_incomplete_number(expression)) {
        return expression;
    }

    std::string next = expression;
    if (needs_implicit_multiply(expression, token, allow_number_continuation)) {
        next.push_back('*');
    }
    next += token;
    return next;
}

}  // namespace calculator
