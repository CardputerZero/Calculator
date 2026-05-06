#ifndef CALCULATOR_AUDIO_H
#define CALCULATOR_AUDIO_H

namespace calculator {

enum class AudioCue {
    SoftClick,
    Function,
    Accent,
    Confirm,
};

void set_audio_enabled(bool enabled);
bool audio_enabled();
bool audio_available();
void play_digit_cue(int digit);
void play_cue(AudioCue cue);

}  // namespace calculator

#endif
