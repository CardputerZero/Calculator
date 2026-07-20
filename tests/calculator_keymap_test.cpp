#include "calculator_keymap.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void expect_mapping(uint32_t keycode, uint32_t expected)
{
    uint32_t actual = 0;
    if (!calculator::tca8418_keycode_lookup(keycode, &actual) || actual != expected) {
        std::cerr << "Unexpected key mapping for " << keycode << ": expected="
                  << expected << " actual=" << actual << "\n";
        ++failures;
    }
}

void expect_unmapped(uint32_t keycode)
{
    uint32_t character = 0;
    if (calculator::tca8418_keycode_lookup(keycode, &character)) {
        std::cerr << "Expected key " << keycode << " to be unmapped\n";
        ++failures;
    }
}

std::string make_temporary_file(const std::string &contents)
{
    char path[] = "/tmp/calculator-keymap-XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) {
        return {};
    }
    close(fd);

    std::ofstream file(path);
    file << contents;
    file.close();
    return path;
}

}  // namespace

int main()
{
    calculator::load_tca8418_keymap("/path/that/does/not/exist");
    const std::vector<std::pair<uint32_t, uint32_t>> default_keys = {
        {26, '!'}, {27, '@'}, {39, '#'}, {40, '$'}, {41, '%'}, {43, '^'},
        {51, '&'}, {52, '*'}, {53, '('}, {54, ')'}, {55, '~'}, {69, '`'},
        {70, '_'}, {71, '-'}, {72, '+'}, {73, '='}, {74, '['}, {75, ']'},
        {76, '{'}, {77, '}'}, {79, ';'}, {80, ':'}, {81, '\''}, {82, '"'},
        {83, '<'}, {85, '>'}, {86, '\\'}, {89, '|'}, {90, ','}, {91, '.'},
        {92, '/'}, {93, '?'},
    };
    for (const auto &entry : default_keys) {
        expect_mapping(entry.first, entry.second);
    }

    const std::string path = make_temporary_file(
        "# test override\n"
        "keycode 52 = plus\n"
        "keycode 52 = minus\n"
        "invalid entry\n"
        "keycode 53 = NoSuchCalculatorSymbol\n"
        "keycode 54 = 7\n"
        "keycode 55 = c\n"
        "keycode 56 = space\n");
    if (path.empty()) {
        std::cerr << "Unable to create temporary keymap\n";
        return EXIT_FAILURE;
    }

    if (!calculator::load_tca8418_keymap(path.c_str())) {
        std::cerr << "Unable to load temporary keymap\n";
        ++failures;
    }
    unlink(path.c_str());

    expect_mapping(52, '+');
    expect_mapping(53, 0);
    expect_mapping(54, '7');
    expect_mapping(55, 'c');
    expect_mapping(56, ' ');
    expect_unmapped(26);

    const std::string empty_path = make_temporary_file("");
    if (empty_path.empty() || !calculator::load_tca8418_keymap(empty_path.c_str())) {
        std::cerr << "Unable to load empty temporary keymap\n";
        ++failures;
    }
    unlink(empty_path.c_str());
    expect_unmapped(52);

    calculator::load_tca8418_keymap(nullptr);
    expect_mapping(52, '*');

    if (failures != 0) {
        return EXIT_FAILURE;
    }
    std::cout << "Passed calculator keymap tests\n";
    return EXIT_SUCCESS;
}
