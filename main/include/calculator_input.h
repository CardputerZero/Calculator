#ifndef CALCULATOR_INPUT_H
#define CALCULATOR_INPUT_H

#include <string>

namespace calculator {

bool token_continues_number(const std::string &expression, const std::string &token);
bool needs_implicit_multiply(const std::string &expression, const std::string &token,
                             bool allow_number_continuation);
std::string append_token_with_implicit_multiply(const std::string &expression,
                                                const std::string &token,
                                                bool allow_number_continuation);

}  // namespace calculator

#endif
