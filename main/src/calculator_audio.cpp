#include "calculator_audio.h"

#include "global_config.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#if defined(CONFIG_TINYALSA_COMPONENT_ENABLED) && defined(__linux__)
#include <tinyalsa/asoundlib.h>
#endif

namespace calculator {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr unsigned kDefaultSampleRate = 22050;
constexpr size_t kTonePaddingFrames = 48;

bool read_env_uint(const char *name, unsigned int *value)
{
    const char *text = std::getenv(name);
    if (text == nullptr || *text == '\0') {
        return false;
    }

    char *end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }

    *value = static_cast<unsigned int>(parsed);
    return true;
}

bool read_env_enabled(const char *name, bool fallback)
{
    const char *text = std::getenv(name);
    if (text == nullptr || *text == '\0') {
        return fallback;
    }

    return std::strcmp(text, "0") != 0 &&
           std::strcmp(text, "false") != 0 &&
           std::strcmp(text, "False") != 0 &&
           std::strcmp(text, "off") != 0 &&
           std::strcmp(text, "OFF") != 0;
}

#if defined(CONFIG_TINYALSA_COMPONENT_ENABLED) && defined(__linux__)

class AudioEngine {
public:
    AudioEngine()
        : enabled_(read_env_enabled("CALCULATOR_SOUND_ENABLED", true))
    {
    }

    ~AudioEngine()
    {
        if (pcm_ != nullptr) {
            pcm_close(pcm_);
            pcm_ = nullptr;
        }
    }

    void set_enabled(bool enabled)
    {
        enabled_ = enabled;
    }

    bool enabled() const
    {
        return enabled_;
    }

    bool available()
    {
        return ensure_backend();
    }

    void play_digit(int digit)
    {
        // 1-7 map to do/re/mi/fa/so/la/xi, then continue into the next octave
        // for 8/9/0 so repeated digit entry still sounds melodic.
        static constexpr std::array<double, 10> kDigitFrequencies = {
            659.25, 261.63, 293.66, 329.63, 349.23,
            392.00, 440.00, 493.88, 523.25, 587.33,
        };

        if (digit < 0 || digit >= static_cast<int>(kDigitFrequencies.size())) {
            play_cue(AudioCue::SoftClick);
            return;
        }

        play_tone_sequence({kDigitFrequencies[static_cast<size_t>(digit)]}, 0.030, 0.12);
    }

    void play_cue(AudioCue cue)
    {
        switch (cue) {
        case AudioCue::SoftClick:
            play_tone_sequence({196.00}, 0.016, 0.08);
            break;
        case AudioCue::Function:
            play_tone_sequence({246.94}, 0.020, 0.09);
            break;
        case AudioCue::Accent:
            play_tone_sequence({329.63}, 0.022, 0.10);
            break;
        case AudioCue::Confirm:
            play_tone_sequence({392.00, 523.25}, 0.016, 0.10);
            break;
        }
    }

private:
    bool ensure_backend()
    {
        if (backend_initialized_) {
            return backend_available_;
        }
        backend_initialized_ = true;

        unsigned int card = 0;
        unsigned int device = 0;
        if (!read_env_uint("CALCULATOR_AUDIO_CARD", &card) ||
            !read_env_uint("CALCULATOR_AUDIO_DEVICE", &device)) {
            detect_first_playback_device(&card, &device);
        }

        unsigned int sample_rate = kDefaultSampleRate;
        read_env_uint("CALCULATOR_AUDIO_RATE", &sample_rate);

        std::memset(&config_, 0, sizeof(config_));
        config_.channels = 1;
        config_.rate = sample_rate;
        config_.period_size = 128;
        config_.period_count = 4;
        config_.format = PCM_FORMAT_S16_LE;
        config_.start_threshold = 0;
        config_.stop_threshold = 0;
        config_.silence_threshold = 0;
        config_.silence_size = 0;

        pcm_ = pcm_open(card, device, PCM_OUT, &config_);
        if (!pcm_is_ready(pcm_)) {
            if (pcm_ != nullptr) {
                std::fprintf(stderr, "calculator audio unavailable on pcm %u,%u: %s\n",
                             card, device, pcm_get_error(pcm_));
                pcm_close(pcm_);
                pcm_ = nullptr;
            }
            return false;
        }

        backend_available_ = true;
        sample_rate_ = sample_rate;
        return true;
    }

    bool detect_first_playback_device(unsigned int *card, unsigned int *device) const
    {
        FILE *fp = std::fopen("/proc/asound/pcm", "r");
        if (fp == nullptr) {
            return false;
        }

        char line[256];
        while (std::fgets(line, sizeof(line), fp) != nullptr) {
            if (std::strstr(line, "playback") == nullptr) {
                continue;
            }

            unsigned int parsed_card = 0;
            unsigned int parsed_device = 0;
            if (std::sscanf(line, "%u-%u:", &parsed_card, &parsed_device) == 2) {
                *card = parsed_card;
                *device = parsed_device;
                std::fclose(fp);
                return true;
            }
        }

        std::fclose(fp);
        return false;
    }

    void play_tone_sequence(std::initializer_list<double> freqs, double tone_seconds,
                            double amplitude)
    {
        if (!enabled_ || !ensure_backend()) {
            return;
        }

        std::vector<int16_t> samples;
        const size_t tone_frames = static_cast<size_t>(sample_rate_ * tone_seconds);
        const size_t total_frames =
            freqs.size() * (tone_frames + kTonePaddingFrames);
        samples.reserve(total_frames);

        for (double freq : freqs) {
            append_tone(samples, freq, tone_frames, amplitude);
            samples.insert(samples.end(), kTonePaddingFrames, 0);
        }

        if (samples.empty()) {
            return;
        }

        if (pcm_writei(pcm_, samples.data(), samples.size()) < 0) {
            std::fprintf(stderr, "calculator audio write failed, disabling backend\n");
            pcm_close(pcm_);
            pcm_ = nullptr;
            backend_available_ = false;
            backend_initialized_ = true;
        }
    }

    void append_tone(std::vector<int16_t> &samples, double frequency, size_t frames,
                     double amplitude) const
    {
        if (frames == 0 || frequency <= 0.0) {
            return;
        }

        const size_t fade_frames = frames < 12 ? frames / 2 : 6;
        for (size_t i = 0; i < frames; ++i) {
            double envelope = 1.0;
            if (fade_frames > 0 && i < fade_frames) {
                envelope = static_cast<double>(i) / static_cast<double>(fade_frames);
            } else if (fade_frames > 0 && i + fade_frames >= frames) {
                envelope =
                    static_cast<double>(frames - i - 1) / static_cast<double>(fade_frames);
            }
            if (envelope < 0.0) {
                envelope = 0.0;
            }

            const double phase = kTwoPi * frequency *
                                 static_cast<double>(i) / static_cast<double>(sample_rate_);
            const double sample = std::sin(phase) * amplitude * envelope;
            samples.push_back(static_cast<int16_t>(std::lround(sample * 32767.0)));
        }
    }

    bool enabled_ = true;
    bool backend_initialized_ = false;
    bool backend_available_ = false;
    unsigned int sample_rate_ = kDefaultSampleRate;
    struct pcm_config config_ {};
    struct pcm *pcm_ = nullptr;
};

#else

class AudioEngine {
public:
    AudioEngine()
        : enabled_(read_env_enabled("CALCULATOR_SOUND_ENABLED", false))
    {
    }

    void set_enabled(bool enabled)
    {
        enabled_ = enabled;
    }

    bool enabled() const
    {
        return enabled_;
    }

    bool available()
    {
        return false;
    }

    void play_digit(int) {}
    void play_cue(AudioCue) {}

private:
    bool enabled_ = false;
};

#endif

AudioEngine &audio_engine()
{
    static AudioEngine engine;
    return engine;
}

}  // namespace

void set_audio_enabled(bool enabled)
{
    audio_engine().set_enabled(enabled);
}

bool audio_enabled()
{
    return audio_engine().enabled();
}

bool audio_available()
{
    return audio_engine().available();
}

void play_digit_cue(int digit)
{
    audio_engine().play_digit(digit);
}

void play_cue(AudioCue cue)
{
    audio_engine().play_cue(cue);
}

}  // namespace calculator
