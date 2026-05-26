# Calculator Keymap

This layout mirrors the Cardputer Zero sticker as much as possible while keeping the system keys available.

## Screen Button Layout

The calculator screen uses four rows that roughly match the physical keyboard rows.

```text
1     2     3     4     5     6     7     8     9     0     Del
C     sin   cos   +     -     /     *     ^     (     )     Rad
.     tan   ln    log   sqrt  =     abs   pi    e     !     OK
2nd   Ans   Rand  1/x   x^2   x^3   x^y   e^x   10^x  %     +/-
```

## Direct Physical Keys

- `1` to `0`: enter digits.
- `.`: enter decimal point.
- `+`, `-`, `*`, `/`, `^`, `(`, `)`: enter operators or parentheses if the sticker modifier layer emits those characters.
- `Backspace`, `Del`, or the on-screen `Del` button: delete the previous token. Hold it to clear the whole expression.
- `Enter`, `OK`, or `Space`: press the currently highlighted on-screen button.
- `=`: evaluate the expression directly.
- `Esc`: clear the whole expression.
- `Home`: exit the calculator program.
- Arrow keys: move the on-screen focus.
- `PageUp` / `PageDown`: browse calculation history.

## Function Layer

- Use the on-screen `2nd` button for alternate functions.
- `2nd + sin` inputs `asin(`.
- `2nd + cos` inputs `acos(`.
- `2nd + tan` inputs `atan(`.
- `2nd + sqrt` inputs `cbrt(`.
- `2nd + Rand` toggles key sound. The label shows `Snd*` when sound is enabled and `Snd` when disabled.

## Notes

- Physical `fn`, `shift`, `ctrl`, and `sym` remain system/modifier keys. The calculator does not consume them directly.
- The screen layout is a reminder for where functions live; direct formula typing still works for normal characters.
- The result area always takes priority. If the formula is too long, only the formula scrolls horizontally.
