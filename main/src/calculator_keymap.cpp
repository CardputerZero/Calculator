#include "calculator_keymap.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace calculator {
namespace {

constexpr const char *kKeymapPath =
    "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map";
constexpr size_t kMaxRuntimeEntries = 64;

struct KeymapEntry {
    uint32_t keycode;
    uint32_t character;
};

constexpr KeymapEntry kDefaultKeymap[] = {
    {26, '!'}, {27, '@'}, {39, '#'}, {40, '$'}, {41, '%'}, {43, '^'},
    {51, '&'}, {52, '*'}, {53, '('}, {54, ')'}, {55, '~'}, {69, '`'},
    {70, '_'}, {71, '-'}, {72, '+'}, {73, '='}, {74, '['}, {75, ']'},
    {76, '{'}, {77, '}'}, {79, ';'}, {80, ':'}, {81, '\''}, {82, '"'},
    {83, '<'}, {85, '>'}, {86, '\\'}, {89, '|'}, {90, ','}, {91, '.'},
    {92, '/'}, {93, '?'},
};

std::unordered_map<uint32_t, uint32_t> runtime_keymap;
bool runtime_keymap_loaded = false;

char *trim_ascii(char *text)
{
    while (std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }

    char *end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) {
        *--end = '\0';
    }
    return text;
}

uint32_t keysym_to_character(const char *name)
{
    if (name[0] != '\0' && name[1] == '\0') {
        return static_cast<unsigned char>(name[0]);
    }
    if (std::strcmp(name, "space") == 0) {
        return ' ';
    }

    static constexpr struct {
        const char *name;
        uint32_t character;
    } symbols[] = {
        {"exclam", '!'}, {"at", '@'}, {"numbersign", '#'}, {"dollar", '$'},
        {"percent", '%'}, {"asciicircum", '^'}, {"ampersand", '&'},
        {"asterisk", '*'}, {"parenleft", '('}, {"parenright", ')'},
        {"asciitilde", '~'}, {"grave", '`'}, {"underscore", '_'},
        {"minus", '-'}, {"plus", '+'}, {"equal", '='}, {"bracketleft", '['},
        {"bracketright", ']'}, {"braceleft", '{'}, {"braceright", '}'},
        {"semicolon", ';'}, {"colon", ':'}, {"apostrophe", '\''},
        {"quotedbl", '"'}, {"less", '<'}, {"greater", '>'},
        {"backslash", '\\'}, {"bar", '|'}, {"comma", ','}, {"period", '.'},
        {"slash", '/'}, {"question", '?'},
    };

    for (const auto &symbol : symbols) {
        if (std::strcmp(symbol.name, name) == 0) {
            return symbol.character;
        }
    }
    return 0;
}

}  // namespace

const char *tca8418_keymap_path()
{
    return kKeymapPath;
}

bool load_tca8418_keymap(const char *path)
{
    runtime_keymap.clear();
    runtime_keymap_loaded = false;

    if (path == nullptr || path[0] == '\0') {
        return false;
    }

    FILE *file = std::fopen(path, "r");
    if (file == nullptr) {
        return false;
    }

    char line[256];
    size_t entry_count = 0;
    while (entry_count < kMaxRuntimeEntries &&
           std::fgets(line, sizeof(line), file) != nullptr) {
        char *comment = std::strchr(line, '#');
        if (comment != nullptr) {
            *comment = '\0';
        }

        char *body = trim_ascii(line);
        if (body[0] == '\0') {
            continue;
        }

        uint32_t keycode = 0;
        char symbol_name[64] = {};
        if (std::sscanf(body, "keycode %u = %63s", &keycode, symbol_name) != 2) {
            continue;
        }
        runtime_keymap.emplace(keycode, keysym_to_character(symbol_name));
        ++entry_count;
    }

    std::fclose(file);
    runtime_keymap_loaded = true;
    return true;
}

bool tca8418_keycode_lookup(uint32_t keycode, uint32_t *character)
{
    if (runtime_keymap_loaded) {
        const auto entry = runtime_keymap.find(keycode);
        if (entry == runtime_keymap.end()) {
            return false;
        }
        if (character != nullptr) {
            *character = entry->second;
        }
        return true;
    }

    for (const KeymapEntry &entry : kDefaultKeymap) {
        if (entry.keycode == keycode) {
            if (character != nullptr) {
                *character = entry.character;
            }
            return true;
        }
    }
    return false;
}

}  // namespace calculator
