#ifndef CALCULATOR_KEYMAP_H
#define CALCULATOR_KEYMAP_H

#include <cstdint>

namespace calculator {

const char *tca8418_keymap_path();
bool load_tca8418_keymap(const char *path);
bool tca8418_keycode_lookup(uint32_t keycode, uint32_t *character);

}  // namespace calculator

#endif
