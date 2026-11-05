#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

// DTR2:0 = none; 1 = DT decoder; 3 = DT decoder/scrambler; 4 = ordinary helmet speaker.
// Retain strict DTR1 decoding for old protocol fixtures; DTR8 guards new settings.
namespace deathTrooper {
inline bool helmetFromField(std::string_view field) noexcept {
    return field == "DTR1:1" || field == "DTR1:3" || field == "DTR2:1" || field == "DTR2:3";
}
inline bool scramblerFromField(std::string_view field) noexcept {
    return field == "DTR1:3" || field == "DTR2:3";
}
inline bool speakerHelmetFromField(std::string_view field) noexcept {
    return field == "DTR2:4";
}
inline bool shouldScramble(bool speakerScrambler, bool listenerHelmet) noexcept {
    return speakerScrambler && !listenerHelmet;
}

// Unity-peak band-pass, shared by the speech-excited speaker modes.
class SpeakerBandPass {
public:
    SpeakerBandPass(float frequency, float q) noexcept {
        const float omega = 6.28318530718f * frequency / 48000.0f;
        const float alpha = std::sin(omega) / (2.0f * q);
        b0_ = alpha / (1.0f + alpha);
        a1_ = -2.0f * std::cos(omega) / (1.0f + alpha);
        a2_ = (1.0f - alpha) / (1.0f + alpha);
    }
    float process(float input) noexcept {
        const float output = b0_ * input + z1_;
        z1_ = z2_ - a1_ * output;
        z2_ = -b0_ * input - a2_ * output;
        if (std::fabs(z1_) < 1.0e-20f) z1_ = 0.0f;
        if (std::fabs(z2_) < 1.0e-20f) z2_ = 0.0f;
        return output;
    }
    void reset() noexcept { z1_ = z2_ = 0.0f; }
private:
    float b0_, a1_, a2_;
    float z1_ = 0.0f, z2_ = 0.0f;
};

// Fixed modes model a small metal speaker enclosure. They ring only when
// excited by speech: no generated tone or carrier. Their approximately
// 11/7 ms decay to -60 dB is separate from the room reverb.
class MetalCabinet {
public:
    float process(float input, float amount) noexcept {
        const float body = body_.process(input);
        const float edge = edge_.process(input);
        return (input + amount * (0.65f * body + 0.45f * edge)) / (1.0f + 0.25f * amount);
    }
    void reset() noexcept { body_.reset(); edge_.reset(); }
private:
    SpeakerBandPass body_{1250.0f, 6.0f};
    SpeakerBandPass edge_{2650.0f, 8.0f};
};

// A parallel, speech-driven upper-band rasp. The 4.2 kHz mode feeds gentle
// saturation and a 6.5 kHz one-pole rolloff. No regenerative feedback, oscillator
// or additional buffering; zero amount is an exact bypass of this coloration.
class HighScreech {
public:
    float process(float input, float amount) noexcept {
        const float driven = std::tanh(bite_.process(input) * 4.5f) * 0.45f;
        smooth_ += lowPassAlpha_ * (driven - smooth_);
        if (std::fabs(smooth_) < 1.0e-20f) smooth_ = 0.0f;
        return (input + amount * smooth_) / (1.0f + 0.08f * amount);
    }
    void reset() noexcept { bite_.reset(); smooth_ = 0.0f; }
private:
    SpeakerBandPass bite_{4200.0f, 4.0f};
    const float lowPassAlpha_ = 1.0f - std::exp(-6.28318530718f * 6500.0f / 48000.0f);
    float smooth_ = 0.0f;
};

// Radio-static layer gated by the buffered speech envelope. A separate PRNG
// keeps static amount/packet boundaries from changing the grain permutation.
// Filter the noise before gating so silence yields exactly zero added noise.
class SpeechStatic {
public:
    float process(float speechEnvelope, float amount) noexcept {
        random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
        const float white = static_cast<float>(random_ >> 8) / 8388608.0f - 1.0f;
        low_ += highPassAlpha_ * (white - low_);
        band_ += lowPassAlpha_ * ((white - low_) - band_);
        float gate = std::clamp((speechEnvelope - 0.001f) / 0.014f, 0.0f, 1.0f);
        gate = gate * gate * (3.0f - 2.0f * gate);
        return band_ * 0.25f * amount * gate;
    }
    void reset() noexcept { low_ = band_ = 0.0f; }
private:
    uint32_t random_ = 0x6d2b79f5u;
    const float highPassAlpha_ = 1.0f - std::exp(-6.28318530718f * 600.0f / 48000.0f);
    const float lowPassAlpha_ = 1.0f - std::exp(-6.28318530718f * 6000.0f / 48000.0f);
    float low_ = 0.0f, band_ = 0.0f;
};

// 8/12-bit quantization at a held 12 kHz update rate gives digital crunch.
// The output container stays PCM16. Select 16 to bypass this stage.
class DigitalCrusher {
public:
    float process(float input, int bits) noexcept {
        if (bits != bits_) { reset(); bits_ = bits; }
        if (bits >= 16) return input;
        if (remaining_ == 0) {
            const float levels = bits == 12 ? 2048.0f : 128.0f;
            held_ = std::round(std::clamp(input, -1.0f, 1.0f - 1.0f / levels) * levels) / levels;
            remaining_ = 4;
        }
        --remaining_;
        return held_;
    }
    void reset() noexcept { remaining_ = 0; held_ = 0.0f; }
private:
    int bits_ = 8, remaining_ = 0;
    float held_ = 0.0f;
};

// The 90 Hz carrier multiplies the voice; it is never added by itself.
class RingModulator {
public:
    float process(float input, float amount) noexcept {
        const float result = input * ((1.0f - amount) + amount * std::sin(phase_));
        phase_ += 6.28318530718f * 90.0f / 48000.0f;
        if (phase_ >= 6.28318530718f) phase_ -= 6.28318530718f;
        return result;
    }
    void reset() noexcept { phase_ = 0.0f; }
private:
    float phase_ = 0.0f;
};

// Fourth-order Butterworth high-pass (24 dB/octave), -3 dB at cutoff.
class RadioHighPass {
public:
    void setCutoff(float frequency) noexcept {
        if (frequency == cutoff_) return;
        first_.configure(frequency, 0.5411961f);
        second_.configure(frequency, 1.3065630f);
        cutoff_ = frequency;
    }
    float process(float input) noexcept { return second_.process(first_.process(input)); }
    void reset() noexcept { first_.reset(); second_.reset(); }
private:
    class Section {
    public:
        void configure(float frequency, float q) noexcept {
            const double omega = 6.283185307179586 * frequency / 48000.0;
            const double cosine = std::cos(omega);
            const double alpha = std::sin(omega) / (2.0 * q);
            b0_ = (1.0 + cosine) * 0.5 / (1.0 + alpha);
            a1_ = -2.0 * cosine / (1.0 + alpha);
            a2_ = (1.0 - alpha) / (1.0 + alpha);
        }
        float process(float input) noexcept {
            const double output = b0_ * input + z1_;
            z1_ = -2.0 * b0_ * input - a1_ * output + z2_;
            z2_ = b0_ * input - a2_ * output;
            if (std::fabs(z1_) < 1.0e-20f) z1_ = 0.0f;
            if (std::fabs(z2_) < 1.0e-20f) z2_ = 0.0f;
            return static_cast<float>(output);
        }
        void reset() noexcept { z1_ = z2_ = 0.0f; }
    private:
        // Double state avoids low-cutoff cancellation leaving PCM-level residue.
        double b0_ = 1.0, a1_ = 0.0, a2_ = 0.0;
        double z1_ = 0.0, z2_ = 0.0;
    };
    Section first_, second_;
    float cutoff_ = -1.0f;
};

// Two-pole Butterworth upper rolloff darkens both dry and wet radio branches.
class RadioLowPass {
public:
    void setCutoff(float frequency) noexcept {
        if (frequency == cutoff_) return;
        const double omega = 6.283185307179586 * frequency / 48000.0;
        const double cosine = std::cos(omega);
        const double alpha = std::sin(omega) / 1.4142135623730951;
        b0_ = (1.0 - cosine) * 0.5 / (1.0 + alpha);
        a1_ = -2.0 * cosine / (1.0 + alpha);
        a2_ = (1.0 - alpha) / (1.0 + alpha);
        cutoff_ = frequency;
    }
    float process(float input) noexcept {
        const double output = b0_ * input + z1_;
        z1_ = 2.0 * b0_ * input - a1_ * output + z2_;
        z2_ = b0_ * input - a2_ * output;
        if (std::fabs(z1_) < 1.0e-20) z1_ = 0.0;
        if (std::fabs(z2_) < 1.0e-20) z2_ = 0.0;
        return static_cast<float>(output);
    }
    void reset() noexcept { z1_ = z2_ = 0.0; }
private:
    double b0_ = 1.0, a1_ = 0.0, a2_ = 0.0, z1_ = 0.0, z2_ = 0.0;
    float cutoff_ = -1.0f;
};

// N consecutive sub-threshold samples close this retriggerable level gate.
class HardSquelch {
public:
    bool process(float input, float threshold, int holdFrames) noexcept {
        const float level = std::fabs(input);
        if (level >= threshold && level > 0.0f) { remaining_ = holdFrames; return true; }
        if (remaining_ > 0) --remaining_;
        return remaining_ > 0;
    }
    void reset() noexcept { remaining_ = 0; }
private:
    int remaining_ = 0;
};

// Speech-driven, overdriven helmet amplifier. Four 50 ms grains are shuffled,
// reversed and pitched down before the amp, ring modulation, metal-speaker
// modes, optional reverb, static, digital crushing and a sharp high-pass.
// Squelch cuts dry voice/static immediately; a separate voice-only wet branch
// may ring for a bounded time. Tail length zero restores the hard-cut behavior.
// This keeps the texture of a distorted voice without a clean/dry speech path.
// Undecoded playback has 200 ms algorithmic latency; decoded speech has none.
// Gameplay obfuscation, not encryption or a guarantee of zero intelligibility.
// One instance per remote TS client; no allocations or locks in the audio loop.
class Effect {
public:
    static constexpr int GrainFrames = 2400;
    static constexpr int WindowFrames = GrainFrames * 4;
    static constexpr int ReverbFramesA = 625; // 13 ms: small enclosure, not a hall.
    static constexpr int ReverbFramesB = 911; // 19 ms.

    void reset() noexcept {
        // Mark buffers invalid instead of clearing 38 KB every clear-voice packet.
        cursor_ = 0;
        ready_ = false;
        clearSignalPath();
        squelch_.reset();
        gateWasOpen_ = false;
    }

    void process(short* samples, int frames, int channels,
                 float reverbMix = 0.25f, float reverbDecay = 0.35f,
                 float highPassHz = 150.0f, float pitchSemitones = -6.0f,
                 float ampDrive = 32.0f, float metallic = 0.75f,
                 float screech = 0.25f, float staticAmount = 0.65f,
                 float bitDepth = 8.0f, float ringAmount = 0.12f,
                 bool hardGate = true, float gateThresholdDb = -48.0f,
                 float gateHoldMs = 1.0f, float lowPassHz = 3200.0f,
                 float tailSeconds = 0.22f) noexcept {
        if (!samples || frames <= 0 || channels < 1 || channels > 2) return;
        reverbMix = std::isfinite(reverbMix) ? std::clamp(reverbMix, 0.0f, 0.6f) : 0.25f;
        reverbDecay = std::isfinite(reverbDecay) ? std::clamp(reverbDecay, 0.05f, 1.5f) : 0.35f;
        highPassHz = std::isfinite(highPassHz) ? std::clamp(highPassHz, 80.0f, 1200.0f) : 150.0f;
        pitchSemitones = std::isfinite(pitchSemitones) ? std::clamp(pitchSemitones, -12.0f, 0.0f) : -6.0f;
        ampDrive = std::isfinite(ampDrive) ? std::clamp(ampDrive, 8.0f, 48.0f) : 32.0f;
        metallic = std::isfinite(metallic) ? std::clamp(metallic, 0.0f, 1.0f) : 0.75f;
        screech = std::isfinite(screech) ? std::clamp(screech, 0.0f, 1.0f) : 0.25f;
        staticAmount = std::isfinite(staticAmount) ? std::clamp(staticAmount, 0.0f, 1.0f) : 0.65f;
        bitDepth = std::isfinite(bitDepth) ? bitDepth : 8.0f;
        const int bits = bitDepth < 10.0f ? 8 : (bitDepth < 14.0f ? 12 : 16);
        ringAmount = std::isfinite(ringAmount) ? std::clamp(ringAmount, 0.0f, 1.0f) : 0.12f;
        gateThresholdDb = std::isfinite(gateThresholdDb) ? std::clamp(gateThresholdDb, -70.0f, -20.0f) : -48.0f;
        gateHoldMs = std::isfinite(gateHoldMs) ? std::clamp(gateHoldMs, 0.0f, 10.0f) : 1.0f;
        lowPassHz = std::isfinite(lowPassHz) ? std::clamp(lowPassHz, 1800.0f, 12000.0f) : 3200.0f;
        tailSeconds = std::isfinite(tailSeconds) ? std::clamp(tailSeconds, 0.0f, 0.6f) : 0.22f;
        const int tailFrames = static_cast<int>(std::lround(tailSeconds * 48000.0f));
        // Live changes can shorten an existing tail without restarting it.
        if (tailRemaining_ > 0 && tailFrames == 0) clearSignalPath();
        tailRemaining_ = std::min(tailRemaining_, tailFrames);
        const float gateThreshold = std::pow(10.0f, gateThresholdDb / 20.0f);
        const int holdFrames = static_cast<int>(std::lround(gateHoldMs * 48.0f));
        if (!hardGate) squelch_.reset();
        const float pitchRatio = std::pow(2.0f, pitchSemitones / 12.0f);
        const float feedbackA = std::pow(0.001f, (static_cast<float>(ReverbFramesA) / 48000.0f) / reverbDecay);
        const float feedbackB = std::pow(0.001f, (static_cast<float>(ReverbFramesB) / 48000.0f) / reverbDecay);
        outputFilter_.setCutoff(highPassHz);
        wetOutputFilter_.setCutoff(highPassHz);
        toneFilter_.setCutoff(lowPassHz);
        wetToneFilter_.setCutoff(lowPassHz);
        for (int i = 0; i < frames; ++i) {
            int sum = 0;
            for (int ch = 0; ch < channels; ++ch) sum += samples[i * channels + ch];
            capture_[cursor_] = static_cast<short>(sum / channels);

            float input = 0.0f;
            if (ready_) {
                const int grain = cursor_ / GrainFrames;
                const int within = cursor_ % GrainFrames;
                // At -12 semitones, play the middle 25 ms of each selected grain
                // backwards at half speed over 50 ms. Deliberately discard the
                // rest for garbling; this is not a transparent time stretcher.
                // Centered fractional reads keep every setting inside its grain
                // and preserve the existing fixed latency and packet continuity.
                const float position = (GrainFrames - 1) * (1.0f + pitchRatio) * 0.5f - pitchRatio * within;
                const int index = static_cast<int>(position);
                const float fraction = position - index;
                const int base = order_[grain] * GrainFrames;
                const float a = static_cast<float>(playback_[base + index]);
                const float b = static_cast<float>(playback_[base + std::min(index + 1, GrainFrames - 1)]);
                const float edge = std::min(1.0f, static_cast<float>(std::min(within, GrainFrames - 1 - within)) / 180.0f);
                input = (a + (b - a) * fraction) / 32768.0f * edge;
            }
            const bool gateOpen = !hardGate || squelch_.process(input, gateThreshold, holdFrames);
            short output = 0;
            if (gateOpen) {
                tailRemaining_ = 0;
                high_ += 0.018f * (input - high_);
                low_ += 0.27f * ((input - high_) - low_);
                presenceLow_ += 0.12f * (low_ - presenceLow_);
                envelope_ += 0.002f * (std::fabs(input) - envelope_);
                // Emphasize speech detail before saturation for amplifier bite.
                const float preamp = low_ + metallic * 1.1f * (low_ - presenceLow_);
                const float clipped = std::clamp(std::tanh(preamp * ampDrive), -0.72f, 0.72f) / 0.72f;
                const float gain = std::min(0.62f, std::sqrt(envelope_) * 1.7f);
                const float amp = screech_.process(cabinet_.process(ring_.process(clipped * gain, ringAmount), metallic), screech);
                const float wet = advanceReverb(amp, reverbMix, feedbackA, feedbackB);
                const float withStatic = amp * (1.0f - reverbMix * 0.35f) + static_.process(envelope_, staticAmount);
                const float dryFiltered = toneFilter_.process(outputFilter_.process(crusher_.process(withStatic, bits)));
                // Dedicated wet filters never receive static or crusher history.
                const float wetFiltered = wetToneFilter_.process(wetOutputFilter_.process(wet));
                output = static_cast<short>(std::clamp(dryFiltered + wetFiltered, -0.75f, 0.75f) * 32767.0f);
            } else {
                if (gateWasOpen_) {
                    clearDryPath();
                    tailRemaining_ = tailFrames;
                    if (tailRemaining_ == 0) clearSignalPath();
                }
                if (tailRemaining_ > 0) {
                    const float wet = advanceReverb(0.0f, reverbMix, feedbackA, feedbackB);
                    const float wetFiltered = wetToneFilter_.process(wetOutputFilter_.process(wet));
                    const float fade = std::min(1.0f, static_cast<float>(tailRemaining_) / 960.0f);
                    output = static_cast<short>(std::clamp(wetFiltered * fade, -0.75f, 0.75f) * 32767.0f);
                    if (--tailRemaining_ == 0) clearSignalPath();
                }
            }
            gateWasOpen_ = gateOpen;
            for (int ch = 0; ch < channels; ++ch) samples[i * channels + ch] = output;

            if (++cursor_ == WindowFrames) {
                capture_.swap(playback_);
                cursor_ = 0;
                ready_ = true;
                static constexpr int orders[4][4] = {{2,0,3,1}, {3,1,0,2}, {1,3,2,0}, {2,1,3,0}};
                const auto orderBits = random();
                std::copy(std::begin(orders[orderBits & 3]), std::end(orders[orderBits & 3]), order_.begin());
            }
        }
    }

private:
    float advanceReverb(float input, float mix, float feedbackA, float feedbackB) noexcept {
        const float echoA = reverbWritten_ >= ReverbFramesA ? reverbA_[reverbCursorA_] : 0.0f;
        const float echoB = reverbWritten_ >= ReverbFramesB ? reverbB_[reverbCursorB_] : 0.0f;
        dampA_ += 0.35f * (echoA - dampA_);
        dampB_ += 0.28f * (echoB - dampB_);
        reverbA_[reverbCursorA_] = input + dampA_ * feedbackA;
        reverbB_[reverbCursorB_] = input + dampB_ * feedbackB;
        reverbCursorA_ = (reverbCursorA_ + 1) % ReverbFramesA;
        reverbCursorB_ = (reverbCursorB_ + 1) % ReverbFramesB;
        reverbWritten_ = std::min(reverbWritten_ + 1, ReverbFramesB);
        return (echoA + echoB) * mix;
    }
    void clearDryPath() noexcept {
        low_ = high_ = envelope_ = presenceLow_ = 0.0f;
        cabinet_.reset(); screech_.reset(); static_.reset();
        crusher_.reset(); ring_.reset(); outputFilter_.reset(); toneFilter_.reset();
    }
    void clearSignalPath() noexcept {
        // Never clear grain storage/cursor here: closure is not a new talkspurt.
        clearDryPath();
        wetOutputFilter_.reset(); wetToneFilter_.reset();
        reverbCursorA_ = reverbCursorB_ = reverbWritten_ = 0;
        dampA_ = dampB_ = 0.0f;
        tailRemaining_ = 0;
    }
    uint32_t random() noexcept {
        random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
        return random_;
    }
    uint32_t random_ = 0x91d347abu;
    std::array<short, WindowFrames> capture_{};
    std::array<short, WindowFrames> playback_{};
    std::array<int, 4> order_{{2,0,3,1}};
    int cursor_ = 0;
    bool ready_ = false;
    float low_ = 0.0f, high_ = 0.0f, envelope_ = 0.0f;
    float presenceLow_ = 0.0f;
    MetalCabinet cabinet_;
    HighScreech screech_;
    SpeechStatic static_;
    DigitalCrusher crusher_;
    RingModulator ring_;
    RadioHighPass outputFilter_;
    RadioHighPass wetOutputFilter_;
    RadioLowPass toneFilter_, wetToneFilter_;
    HardSquelch squelch_;
    bool gateWasOpen_ = false;
    int tailRemaining_ = 0;
    std::array<float, ReverbFramesA> reverbA_{};
    std::array<float, ReverbFramesB> reverbB_{};
    int reverbCursorA_ = 0, reverbCursorB_ = 0, reverbWritten_ = 0;
    float dampA_ = 0.0f, dampB_ = 0.0f;
};
}
