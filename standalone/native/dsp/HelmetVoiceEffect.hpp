#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Full-face helmet speaker coloration: high-pass, presence EQ, mild amplifier
// saturation, clarity EQ, then low-pass. No generated sound or speech buffer.
namespace helmetVoice {
class Effect {
public:
    static constexpr int SampleRate = 48000;
    void reset() noexcept {
        for (auto& filter : highPass_) filter.reset();
        for (auto& filter : presence_) filter.reset();
        for (auto& filter : clarity_) filter.reset();
        for (auto& filter : lowPass_) filter.reset();
    }
    void resetSound() noexcept { reset(); }

    void process(short* samples, int frames, int channels,
                 float highPassHz = 623.0f, float lowPassHz = 3100.0f,
                 float drive = 2.4f, float mix = 1.0f,
                 float resonanceHz = 2000.0f, float resonanceDb = 3.0f,
                 float clarityHz = 2500.0f, float clarityDb = 2.0f) noexcept {
        if (!samples || frames <= 0 || channels < 1 || channels > 2) return;
        highPassHz = finiteClamp(highPassHz, 80.0f, 1200.0f, 623.0f);
        lowPassHz = finiteClamp(lowPassHz, 1200.0f, 10000.0f, 3100.0f);
        drive = finiteClamp(drive, 1.0f, 6.0f, 2.4f);
        mix = finiteClamp(mix, 0.0f, 1.0f, 1.0f);
        resonanceHz = finiteClamp(resonanceHz, 500.0f, 4000.0f, 2000.0f);
        resonanceDb = finiteClamp(resonanceDb, 0.0f, 9.0f, 3.0f);
        clarityHz = finiteClamp(clarityHz, 1000.0f, 4000.0f, 2500.0f);
        clarityDb = finiteClamp(clarityDb, 0.0f, 6.0f, 2.0f);
        for (int channel = 0; channel < channels; ++channel) {
            highPass_[channel].configure(highPassHz, true);
            presence_[channel].configure(resonanceHz, resonanceDb);
            clarity_[channel].configure(clarityHz, clarityDb);
            lowPass_[channel].configure(lowPassHz, false);
        }
        for (int frame = 0; frame < frames; ++frame) {
            for (int channel = 0; channel < channels; ++channel) {
                const float input = samples[frame * channels + channel] / 32768.0f;
                const float shaped = presence_[channel].process(highPass_[channel].process(input));
                const float amplified = std::tanh(shaped * drive) / drive;
                // Keep this low-pass last so the amplifier's new harmonics also
                // pass through the requested upper-frequency speaker rolloff.
                // Post-amp placement keeps the requested clarity boost from
                // being reduced by the amplifier's compression.
                const float speaker = lowPass_[channel].process(clarity_[channel].process(amplified));
                const float output = input + mix * (speaker - input);
                samples[frame * channels + channel] = static_cast<short>(
                    std::clamp(output, -1.0f, 32767.0f / 32768.0f) * 32768.0f);
            }
        }
    }

private:
    class Biquad {
    public:
        void configure(float frequency, bool highPass) noexcept {
            if (frequency == frequency_ && highPass == highPassMode_) return;
            const double omega = 6.283185307179586 * frequency / SampleRate;
            const double cosine = std::cos(omega);
            const double alpha = std::sin(omega) / 1.4142135623730951;
            b0_ = (1.0 + (highPass ? cosine : -cosine)) * 0.5 / (1.0 + alpha);
            b1_ = (highPass ? -2.0 : 2.0) * b0_;
            a1_ = -2.0 * cosine / (1.0 + alpha);
            a2_ = (1.0 - alpha) / (1.0 + alpha);
            frequency_ = frequency;
            highPassMode_ = highPass;
        }
        float process(float input) noexcept {
            const double output = b0_ * input + z1_;
            z1_ = b1_ * input - a1_ * output + z2_;
            z2_ = b0_ * input - a2_ * output;
            clearTinyState();
            return static_cast<float>(output);
        }
        void reset() noexcept { z1_ = z2_ = 0.0; }
    private:
        void clearTinyState() noexcept {
            if (std::fabs(z1_) < 1.0e-20) z1_ = 0.0;
            if (std::fabs(z2_) < 1.0e-20) z2_ = 0.0;
        }
        double b0_ = 1.0, b1_ = 0.0, a1_ = 0.0, a2_ = 0.0;
        double z1_ = 0.0, z2_ = 0.0;
        float frequency_ = -1.0f;
        bool highPassMode_ = false;
    };

    // Peaking EQ with fixed Q = 1.2. Gain is dB, not a feedback/ringing amount.
    // Coefficients and state use double precision for reliable quiet tails.
    class PresenceEQ {
    public:
        void configure(float frequency, float gainDb) noexcept {
            if (frequency == frequency_ && gainDb == gainDb_) return;
            frequency_ = frequency;
            gainDb_ = gainDb;
            if (gainDb == 0.0f) {
                reset(); // A later boost must not revive a bypassed old tail.
                return;
            }
            const double amplitude = std::pow(10.0, gainDb / 40.0);
            const double omega = 6.283185307179586 * frequency / SampleRate;
            const double cosine = std::cos(omega);
            const double alpha = std::sin(omega) / (2.0 * 1.2);
            const double denominator = 1.0 + alpha / amplitude;
            b0_ = (1.0 + alpha * amplitude) / denominator;
            b1_ = -2.0 * cosine / denominator;
            b2_ = (1.0 - alpha * amplitude) / denominator;
            a1_ = -2.0 * cosine / denominator;
            a2_ = (1.0 - alpha / amplitude) / denominator;
        }
        float process(float input) noexcept {
            if (gainDb_ == 0.0f) return input; // Exact bypass at zero dB.
            const double output = b0_ * input + z1_;
            z1_ = b1_ * input - a1_ * output + z2_;
            z2_ = b2_ * input - a2_ * output;
            if (std::fabs(z1_) < 1.0e-20) z1_ = 0.0;
            if (std::fabs(z2_) < 1.0e-20) z2_ = 0.0;
            return static_cast<float>(output);
        }
        void reset() noexcept { z1_ = z2_ = 0.0; }
    private:
        double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
        double z1_ = 0.0, z2_ = 0.0;
        float frequency_ = -1.0f, gainDb_ = 0.0f;
    };

    static float finiteClamp(float value, float minimum, float maximum, float fallback) noexcept {
        return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
    }
    std::array<Biquad, 2> highPass_, lowPass_;
    std::array<PresenceEQ, 2> presence_, clarity_;
};
}
