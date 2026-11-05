#include "DeathTrooperEffect.hpp"
#include "VoiceChannelConversion.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <limits>
#include <memory>
#include "render_preview.hpp"

static int checks = 0;
void require(bool condition, const char* name) {
    if (!condition) throw std::runtime_error(name);
    ++checks;
    std::cout << "PASS " << name << '\n';
}

// Measure normalized periodicity inside settled grains. Heavy quantization and
// static add zero crossings, so crossing counts no longer estimate voice pitch.
// Choose the first strong correlation peak to avoid octave/subharmonic errors.
static double measuredPitch(const std::vector<short>& samples) {
    std::array<double, 242> correlation{};
    for (int lag = 29; lag <= 241; ++lag) {
        double cross = 0.0, energyA = 0.0, energyB = 0.0;
        for (size_t grain = deathTrooper::Effect::WindowFrames;
             grain + deathTrooper::Effect::GrainFrames <= samples.size();
             grain += deathTrooper::Effect::GrainFrames) {
            const size_t begin = grain + 480;
            const size_t end = grain + deathTrooper::Effect::GrainFrames - 480 - lag;
            for (size_t i = begin; i < end; ++i) {
                const double a = samples[i], b = samples[i + lag];
                cross += a * b;
                energyA += a * a;
                energyB += b * b;
            }
        }
        correlation[lag] = energyA > 0.0 && energyB > 0.0 ? cross / std::sqrt(energyA * energyB) : 0.0;
    }
    const double strongest = *std::max_element(correlation.begin() + 30, correlation.begin() + 241);
    for (int lag = 30; lag <= 240; ++lag) {
        const double previous = correlation[lag - 1], peak = correlation[lag], next = correlation[lag + 1];
        if (peak > 0.65 && peak >= strongest - 0.035 && peak > previous && peak >= next) {
            const double offset = 0.5 * (previous - next) / (previous - 2.0 * peak + next);
            return 48000.0 / (lag + offset);
        }
    }
    return 0.0;
}

// Exercise the cabinet independently of scrambling so discarded grain samples
// cannot hide an unstable mode or disguise a long tail as speech buffering.
static double cabinetGainAt(double hz, float amount) {
    deathTrooper::MetalCabinet cabinet;
    double inputEnergy = 0.0, outputEnergy = 0.0;
    for (int i = 0; i < 24000; ++i) {
        const float input = static_cast<float>(0.2 * std::sin(i * 6.28318530718 * hz / 48000.0));
        const float output = cabinet.process(input, amount);
        if (i >= 12000) {
            inputEnergy += static_cast<double>(input) * input;
            outputEnergy += static_cast<double>(output) * output;
        }
    }
    return std::sqrt(outputEnergy / inputEnergy);
}

static void metallicChecks() {
    const double lowMode = cabinetGainAt(1250.0, 1.0f);
    const double highMode = cabinetGainAt(2650.0, 1.0f);
    const double betweenModes = cabinetGainAt(1800.0, 1.0f);
    require(lowMode > betweenModes * 1.20 && highMode > betweenModes * 1.20,
            "metal cabinet emphasizes both resonant bands over the region between them");
    std::cout << "Cabinet RMS gains: 1250 Hz " << lowMode << ", 1800 Hz " << betweenModes
              << ", 2650 Hz " << highMode << '\n';
    require(std::abs(cabinetGainAt(1250.0, 0.0f) - 1.0) < 0.0001 &&
            std::abs(cabinetGainAt(2650.0, 0.0f) - 1.0) < 0.0001,
            "zero metallic amount bypasses cabinet coloration");

    deathTrooper::MetalCabinet impulse;
    double earlyEnergy = 0.0, lateEnergy = 0.0;
    bool finite = true;
    for (int i = 0; i < 4800; ++i) {
        const float sample = impulse.process(i == 0 ? 1.0f : 0.0f, 1.0f);
        finite &= std::isfinite(sample) && std::abs(sample) <= 2.0f;
        if (i >= 1 && i < 480) earlyEnergy += static_cast<double>(sample) * sample;
        if (i >= 960) lateEnergy += static_cast<double>(sample) * sample;
    }
    require(finite && earlyEnergy > 1e-8 && lateEnergy < earlyEnergy * 1e-6,
            "speech-excited metal ring decays by 20 ms without self-oscillation");
    impulse.process(1.0f, 1.0f);
    impulse.reset();
    bool resetSilent = true;
    for (int i = 0; i < 4800; ++i) resetSilent &= impulse.process(0.0f, 1.0f) == 0.0f;
    require(resetSilent, "cabinet reset removes all resonant tail");

    std::vector<short> fixture(24000);
    uint32_t random = 0x5d3a284bu;
    for (auto& sample : fixture) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        sample = static_cast<short>(static_cast<int>(random & 0xffffu) - 32768);
    }
    // Heap allocation is test-only: leave enough stack for the existing native
    // regression fixtures, each of which owns a full per-speaker speech buffer.
    auto render = [&](float drive, float amount) {
        auto output = fixture;
        auto effect = std::make_unique<deathTrooper::Effect>();
        effect->process(output.data(), static_cast<int>(output.size()), 1,
                        0.08f, 0.05f, 150.0f, -12.0f, drive, amount);
        return output;
    };
    const auto defaults = render(32.0f, 0.75f);
    const auto invalid = render(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
    require(defaults == invalid, "non-finite drive and metallic settings use finite defaults");
    const auto maximum = render(48.0f, 1.0f);
    require(render(-99.0f, -10.0f) == render(8.0f, 0.0f) &&
            render(100.0f, 10.0f) == maximum,
            "drive and metallic settings clamp to supported limits");
    require(std::all_of(maximum.begin(), maximum.end(), [](short sample) {
                return std::abs(static_cast<int>(sample)) <= 24575;
            }), "maximum drive and metal resonance stay within output limiter");
}

// Probe the separate upper-band stage at equal input levels. This distinguishes
// added screech from a uniform gain increase and keeps grain selection out of
// the response/decay measurements.
static double screechGainAt(double hz, float amount) {
    deathTrooper::HighScreech screech;
    double inputEnergy = 0.0, outputEnergy = 0.0;
    for (int i = 0; i < 24000; ++i) {
        const float input = static_cast<float>(0.2 * std::sin(i * 6.28318530718 * hz / 48000.0));
        const float output = screech.process(input, amount);
        if (i >= 12000) {
            inputEnergy += static_cast<double>(input) * input;
            outputEnergy += static_cast<double>(output) * output;
        }
    }
    return std::sqrt(outputEnergy / inputEnergy);
}

static void screechChecks() {
    const double deepGain = screechGainAt(400.0, 1.0f);
    const double upperGain = screechGainAt(4200.0, 1.0f);
    const double topGain = screechGainAt(10000.0, 1.0f);
    require(upperGain > deepGain * 1.25 && upperGain > topGain * 1.25 && upperGain > 1.2,
            "screech emphasizes the upper voice band rather than uniformly raising volume");
    std::cout << "Screech RMS gains: 400 Hz " << deepGain << ", 4200 Hz " << upperGain
              << ", 10000 Hz " << topGain << '\n';

    deathTrooper::HighScreech bypass;
    bool unchanged = true;
    for (int i = 0; i < 4800; ++i) {
        const float input = static_cast<float>(0.5 * std::sin(i * 0.51) + 0.2 * std::sin(i * 0.05));
        unchanged &= bypass.process(input, 0.0f) == input;
    }
    require(unchanged, "zero screech amount is a sample-exact bypass");

    deathTrooper::HighScreech impulse;
    double earlyEnergy = 0.0, lateEnergy = 0.0;
    bool finite = true;
    for (int i = 0; i < 4800; ++i) {
        const float sample = impulse.process(i == 0 ? 1.0f : 0.0f, 1.0f);
        finite &= std::isfinite(sample) && std::abs(sample) <= 2.0f;
        if (i >= 1 && i < 480) earlyEnergy += static_cast<double>(sample) * sample;
        if (i >= 960) lateEnergy += static_cast<double>(sample) * sample;
    }
    require(finite && earlyEnergy > 1e-8 && lateEnergy < earlyEnergy * 1e-6,
            "upper-band screech decays by 20 ms without a sustained tone");
    impulse.process(1.0f, 1.0f);
    impulse.reset();
    bool resetSilent = true;
    for (int i = 0; i < 4800; ++i) resetSilent &= impulse.process(0.0f, 1.0f) == 0.0f;
    require(resetSilent, "screech reset removes the resonant and low-pass tail");

    std::vector<short> fixture(48000);
    uint32_t random = 0x6bc35d19u;
    for (auto& sample : fixture) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        sample = static_cast<short>(static_cast<int>(random & 0xffffu) - 32768);
    }
    auto render = [&](float amount) {
        auto output = fixture;
        auto effect = std::make_unique<deathTrooper::Effect>();
        effect->process(output.data(), static_cast<int>(output.size()), 1,
                        0.6f, 1.5f, 80.0f, -12.0f, 48.0f, 1.0f, amount);
        return output;
    };
    const auto defaults = render(0.25f);
    require(render(std::numeric_limits<float>::quiet_NaN()) == defaults &&
            render(std::numeric_limits<float>::infinity()) == defaults,
            "non-finite screech settings use the finite default");
    const auto maximum = render(1.0f);
    require(render(-10.0f) == render(0.0f) && render(10.0f) == maximum,
            "screech settings clamp to the supported range");
    require(std::all_of(maximum.begin(), maximum.end(), [](short sample) {
                return std::abs(static_cast<int>(sample)) <= 24575;
            }), "maximum screech, drive, metal and reverb stay within the output limiter");
}

static void staticChecks() {
    deathTrooper::SpeechStatic gated;
    bool quiet = true;
    for (int i = 0; i < 4800; ++i) {
        quiet &= gated.process(0.0f, 1.0f) == 0.0f;
        quiet &= gated.process(0.0005f, 1.0f) == 0.0f;
        quiet &= gated.process(0.2f, 0.0f) == 0.0f;
    }
    require(quiet, "static is exactly zero below the speech gate or at zero amount");

    double energy = 0.0, mean = 0.0;
    bool finite = true;
    for (int i = 0; i < 48000; ++i) {
        const float noise = gated.process(0.05f, 0.65f);
        energy += static_cast<double>(noise) * noise;
        mean += noise;
        finite &= std::isfinite(noise) && std::abs(noise) <= 0.25f;
    }
    const double rms = std::sqrt(energy / 48000.0);
    require(finite && rms > 0.005 && rms < 0.1 && std::abs(mean / 48000.0) < 0.001,
            "open speech gate produces bounded static with negligible DC");
    std::cout << "Static RMS at default amount and open gate: " << rms << '\n';
    gated.reset();
    quiet = true;
    for (int i = 0; i < 4800; ++i) quiet &= gated.process(0.0f, 1.0f) == 0.0f;
    require(quiet, "static reset followed by silence leaves no hiss");

    deathTrooper::SpeechStatic first, twin, otherSpeaker;
    bool independent = true;
    for (int i = 0; i < 12000; ++i) {
        const float a = first.process(0.05f, 0.25f);
        // Consuming a different speaker's stream must not advance this one.
        for (int j = 0; j < i % 3; ++j) otherSpeaker.process(0.1f, 1.0f);
        independent &= a == twin.process(0.05f, 0.25f);
    }
    require(independent, "per-speaker static streams are independent and deterministic");

    std::vector<short> burst(96000, 0);
    for (int i = 0; i < 24000; ++i)
        burst[i] = static_cast<short>(16000 * std::sin(i * 0.097) + 5000 * std::sin(i * 0.041));
    auto render = [&](float amount, bool maximum = false) {
        auto output = burst;
        auto effect = std::make_unique<deathTrooper::Effect>();
        effect->process(output.data(), static_cast<int>(output.size()), 1,
                        0.6f, 1.5f, 80.0f, -12.0f,
                        maximum ? 48.0f : 32.0f, maximum ? 1.0f : 0.75f,
                        maximum ? 1.0f : 0.65f, amount, 16.0f, 0.0f, false);
        return output;
    };
    const auto off = render(0.0f);
    const auto defaults = render(0.65f);
    bool speechChanged = false;
    int maximumTailDifference = 0;
    for (int i = 12000; i < 30000; ++i) speechChanged |= off[i] != defaults[i];
    // Reverb is intentionally at its maximum here: static must not enter its
    // delay lines and keep hissing under a long residual voice tail.
    for (int i = 48000; i < 96000; ++i) {
        maximumTailDifference = std::max(maximumTailDifference, std::abs(static_cast<int>(off[i]) - defaults[i]));
    }
    std::cout << "Static-off late-tail difference: " << maximumTailDifference << " PCM units\n";
    require(speechChanged && maximumTailDifference == 0,
            "static adds speech texture without a noise bed in long ungated reverb tails");

    auto toggled = burst;
    auto toggledEffect = std::make_unique<deathTrooper::Effect>();
    toggledEffect->process(toggled.data(), 24000, 1, 0.6f, 1.5f, 80.0f, -12.0f, 32.0f, 0.75f, 0.65f, 0.65f, 16.0f, 0.0f, false);
    toggledEffect->process(toggled.data() + 24000, 72000, 1, 0.6f, 1.5f, 80.0f, -12.0f, 32.0f, 0.75f, 0.65f, 0.0f, 16.0f, 0.0f, false);
    int toggleDifference = 0;
    for (size_t i = 30000; i < toggled.size(); ++i) toggleDifference = std::max(toggleDifference, std::abs(static_cast<int>(toggled[i]) - off[i]));
    std::cout << "Static-toggle settled difference: " << toggleDifference << " PCM units\n";
    require(std::equal(toggled.begin() + 30000, toggled.end(), off.begin() + 30000),
            "turning static off preserves the grain sequence and underlying voice");

    require(render(std::numeric_limits<float>::quiet_NaN()) == defaults &&
            render(std::numeric_limits<float>::infinity()) == defaults,
            "non-finite static settings use the finite default");
    const auto maximum = render(1.0f);
    require(render(-10.0f) == off && render(10.0f) == maximum,
            "static settings clamp to the supported range");
    const auto allMaximum = render(1.0f, true);
    require(std::all_of(allMaximum.begin(), allMaximum.end(), [](short sample) {
                return std::abs(static_cast<int>(sample)) <= 24575;
            }), "maximum static and amplifier settings stay within the output limiter");
}

static double radioHighPassGainAt(double hz) {
    deathTrooper::RadioHighPass filter;
    filter.setCutoff(150.0f);
    double inputEnergy = 0.0, outputEnergy = 0.0;
    for (int i = 0; i < 48000; ++i) {
        const float input = static_cast<float>(0.2 * std::sin(i * 6.28318530718 * hz / 48000.0));
        const float output = filter.process(input);
        if (i >= 24000) {
            inputEnergy += static_cast<double>(input) * input;
            outputEnergy += static_cast<double>(output) * output;
        }
    }
    return std::sqrt(outputEnergy / inputEnergy);
}

static double toneAmplitude(const std::vector<float>& samples, double hz) {
    double real = 0.0, imaginary = 0.0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const double phase = i * 6.28318530718 * hz / 48000.0;
        real += samples[i] * std::cos(phase);
        imaginary += samples[i] * std::sin(phase);
    }
    return 2.0 * std::sqrt(real * real + imaginary * imaginary) / samples.size();
}

static void digitalRadioChecks() {
    deathTrooper::DigitalCrusher crusher8, crusher12, bypass;
    bool discrete = true, held = true, bypassExact = true;
    double error8 = 0.0, error12 = 0.0;
    float firstHeld = 0.0f;
    for (int i = 0; i < 19200; ++i) {
        const float input = -0.9f + 1.8f * i / 19200.0f;
        const float eight = crusher8.process(input, 8);
        const float twelve = crusher12.process(input, 12);
        discrete &= std::fabs(eight * 128.0f - std::round(eight * 128.0f)) < 1e-5f;
        discrete &= std::fabs(twelve * 2048.0f - std::round(twelve * 2048.0f)) < 1e-5f;
        if (i % 4 == 0) {
            firstHeld = eight;
            error8 += std::fabs(eight - input);
            error12 += std::fabs(twelve - input);
        } else held &= eight == firstHeld;
        bypassExact &= bypass.process(input, 16) == input;
    }
    require(discrete && error12 < error8 * 0.2, "8-bit and 12-bit modes quantize at their stated resolution");
    require(held, "digital crunch holds each quantized sample for four frames");
    require(bypassExact, "16-bit mode bypasses quantization and sample hold exactly");

    const double gain75 = radioHighPassGainAt(75.0);
    const double gain150 = radioHighPassGainAt(150.0);
    const double gain300 = radioHighPassGainAt(300.0);
    require(gain75 < 0.10 && gain150 > 0.64 && gain150 < 0.78 && gain300 > 0.95,
            "fourth-order radio high-pass sharply rejects bass below 150 Hz");
    std::cout << "Radio high-pass gains: 75 Hz " << gain75 << ", 150 Hz " << gain150
              << ", 300 Hz " << gain300 << '\n';
    deathTrooper::RadioHighPass resetFilter;
    resetFilter.setCutoff(150.0f);
    resetFilter.process(1.0f);
    resetFilter.reset();
    bool filterSilent = true;
    for (int i = 0; i < 4800; ++i) filterSilent &= resetFilter.process(0.0f) == 0.0f;
    require(filterSilent, "radio high-pass reset removes its entire tail");

    deathTrooper::RingModulator ring, unmodulated;
    std::vector<float> modulated(48000), dry(48000);
    bool ringBypass = true;
    for (int i = 0; i < 48000; ++i) {
        const float input = static_cast<float>(0.3 * std::sin(i * 6.28318530718 * 1000.0 / 48000.0));
        modulated[i] = ring.process(input, 0.12f);
        dry[i] = unmodulated.process(input, 0.0f);
        ringBypass &= dry[i] == input;
    }
    const double lowerSideband = toneAmplitude(modulated, 910.0);
    const double upperSideband = toneAmplitude(modulated, 1090.0);
    require(lowerSideband > 0.012 && lowerSideband < 0.025 &&
            upperSideband > 0.012 && upperSideband < 0.025 &&
            toneAmplitude(dry, 910.0) < 0.001 && toneAmplitude(modulated, 1000.0) > 0.24,
            "mild 90 Hz ring modulation adds sidebands while retaining the voice carrier");
    require(ringBypass, "zero ring modulation leaves the speech samples unchanged");
    std::cout << "Ring sideband amplitudes: 910 Hz " << lowerSideband << ", 1090 Hz " << upperSideband << '\n';

    deathTrooper::HardSquelch gate;
    bool initialSilent = true;
    for (int i = 0; i < 100; ++i) initialSilent &= !gate.process(0.0f, 0.004f, 48);
    bool timing = gate.process(0.1f, 0.004f, 48);
    for (int i = 1; i < 48; ++i) timing &= gate.process(0.0f, 0.004f, 48);
    timing &= !gate.process(0.0f, 0.004f, 48);
    timing &= !gate.process(0.0f, 0.004f, 48);
    require(initialSilent && timing, "hard squelch closes on exactly the 48th sub-threshold frame");
    gate.reset();
    bool crossedSafely = false, dropped = false;
    for (int i = 0; i < 4800; ++i) {
        const float input = static_cast<float>(0.1 * std::sin(i * 6.28318530718 * 100.0 / 48000.0));
        const bool open = gate.process(input, 0.004f, 48);
        if (crossedSafely && !open) dropped = true;
        crossedSafely |= open;
    }
    require(crossedSafely && !dropped, "one-ms squelch hold bridges ordinary voiced zero crossings");
    gate.reset();
    require(!gate.process(0.0f, 0.004f, 48) && gate.process(0.1f, 0.004f, 0) &&
            !gate.process(0.0f, 0.004f, 0), "gate reset and zero-hold mode close immediately on silence");

    const int window = deathTrooper::Effect::WindowFrames;
    std::vector<short> interruptedSpeech(window * 6, 0);
    for (int i = 0; i < window; ++i) {
        const short sample = static_cast<short>(11000 + 4000 * std::sin(i * 0.08));
        interruptedSpeech[i] = sample;
        interruptedSpeech[window * 2 + i] = sample;
    }
    auto render = [&](const std::vector<short>& fixture, float staticAmount, float ringAmount,
                      float bits = 8.0f, float threshold = -48.0f, float holdMs = 1.0f) {
        auto output = fixture;
        auto effect = std::make_unique<deathTrooper::Effect>();
        effect->process(output.data(), static_cast<int>(output.size()), 1,
                        0.6f, 1.5f, 150.0f, -5.0f, 48.0f, 1.0f, 1.0f,
                        staticAmount, bits, ringAmount, true, threshold, holdMs, 3200.0f, 0.0f);
        return output;
    };
    const auto maximum = render(interruptedSpeech, 1.0f, 1.0f);
    const bool pauseZero = std::all_of(maximum.begin() + window * 2 + 48, maximum.begin() + window * 3,
                                     [](short s) {return s == 0;});
    const bool endZero = std::all_of(maximum.begin() + window * 4 + 48, maximum.end(),
                                   [](short s) {return s == 0;});
    require(pauseZero && endZero, "final squelch cuts all static and maximum reverb to exact zero in pauses");
    require(std::any_of(maximum.begin() + window * 3 + 48, maximum.begin() + window * 3 + 480,
                        [](short s) {return s != 0;}), "speech reopens without another 200 ms buffer warmup");
    auto noEarlierSpeech = interruptedSpeech;
    std::fill(noEarlierSpeech.begin(), noEarlierSpeech.begin() + window, short{0});
    const auto previouslyActive = render(interruptedSpeech, 0.0f, 0.0f);
    const auto previouslyQuiet = render(noEarlierSpeech, 0.0f, 0.0f);
    require(std::equal(previouslyActive.begin() + window * 3, previouslyActive.begin() + window * 4,
                       previouslyQuiet.begin() + window * 3),
            "reopened speech contains no stored tail from the previous talk burst");
    require(std::all_of(maximum.begin(), maximum.end(), [](short s) {return std::abs(static_cast<int>(s)) <= 24575;}),
            "maximum digital-radio effects remain within the output limit");

    const auto normal = render(interruptedSpeech, 0.65f, 0.12f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    require(render(interruptedSpeech, 0.65f, nan, nan, nan, nan) == normal,
            "invalid bit depth, ring and squelch settings use finite R6 defaults");
    require(render(interruptedSpeech, 0.65f, -10.0f, -99.0f, -99.0f, -10.0f) ==
                render(interruptedSpeech, 0.65f, 0.0f, 8.0f, -70.0f, 0.0f) &&
            render(interruptedSpeech, 0.65f, 10.0f, 99.0f, 0.0f, 99.0f) ==
                render(interruptedSpeech, 0.65f, 1.0f, 16.0f, -20.0f, 10.0f),
            "digital-radio controls clamp to supported ranges");
    require(render(interruptedSpeech, 0.65f, 0.12f, 9.0f) == normal &&
            render(interruptedSpeech, 0.65f, 0.12f, 10.0f) == render(interruptedSpeech, 0.65f, 0.12f, 12.0f) &&
            render(interruptedSpeech, 0.65f, 0.12f, 14.0f) == render(interruptedSpeech, 0.65f, 0.12f, 16.0f),
            "bit-depth settings select only the supported 8, 12 and 16-bit modes");
}

static double radioLowPassGainAt(double hz) {
    deathTrooper::RadioLowPass filter;
    filter.setCutoff(3200.0f);
    double inputEnergy = 0.0, outputEnergy = 0.0;
    for (int i = 0; i < 48000; ++i) {
        const float input = static_cast<float>(0.2 * std::sin(i * 6.28318530718 * hz / 48000.0));
        const float output = filter.process(input);
        if (i >= 24000) {
            inputEnergy += static_cast<double>(input) * input;
            outputEnergy += static_cast<double>(output) * output;
        }
    }
    return std::sqrt(outputEnergy / inputEnergy);
}

static void darkTailChecks() {
    const double gain800 = radioLowPassGainAt(800.0);
    const double gain3200 = radioLowPassGainAt(3200.0);
    const double gain6400 = radioLowPassGainAt(6400.0);
    require(gain800 > 0.98 && gain3200 > 0.68 && gain3200 < 0.74 && gain6400 < 0.24,
            "dark voice low-pass retains the low voice band and attenuates upper fizz");
    std::cout << "Dark low-pass gains: 800 Hz " << gain800 << ", 3200 Hz " << gain3200
              << ", 6400 Hz " << gain6400 << '\n';
    deathTrooper::RadioLowPass filter;
    filter.setCutoff(3200.0f);
    filter.process(1.0f);
    filter.reset();
    bool resetSilent = true;
    for (int i = 0; i < 4800; ++i) resetSilent &= filter.process(0.0f) == 0.0f;
    require(resetSilent, "dark low-pass reset leaves no stored ringing");

    // A complete, nonzero speech window makes the delayed gate boundary known
    // despite grain shuffling and pitch shift. Preserve the 200 ms window; the
    // requested longer sound is a wet tail, not a slower or rebuffered voice.
    const int window = deathTrooper::Effect::WindowFrames;
    // The final grain sample has a zero edge fade. Every other sample remains
    // above -48 dBFS, so closure is the 48th zero starting at 2*window - 1.
    const int gateClosed = window * 2 + 46;
    const int tailFrames = 10560; // 220 ms at 48 kHz.
    std::vector<short> burst(window * 7, 0);
    for (int i = 0; i < window; ++i)
        burst[i] = static_cast<short>(28000 + 3000 * std::sin(i * 0.08));
    auto render = [&](const std::vector<short>& input, float staticAmount = 0.65f,
                      float lowPass = 3200.0f, float tail = 0.22f,
                      float mix = 0.25f, float decay = 0.35f, bool packets = false) {
        auto output = input;
        auto effect = std::make_unique<deathTrooper::Effect>();
        const int sizes[] = {1, 47, 257, 480, 963, 1271};
        for (int offset = 0, packet = 0; offset < static_cast<int>(output.size()); ++packet) {
            const int frames = packets ? std::min(sizes[packet % 6], static_cast<int>(output.size()) - offset)
                                       : static_cast<int>(output.size());
            effect->process(output.data() + offset, frames, 1,
                            mix, decay, 150.0f, -6.0f, 32.0f, 0.75f, 0.25f,
                            staticAmount, 8.0f, 0.12f, true, -48.0f, 1.0f, lowPass, tail);
            offset += frames;
        }
        return output;
    };
    const auto defaults = render(burst);
    auto implicitDefaults = burst;
    auto defaultEffect = std::make_unique<deathTrooper::Effect>();
    defaultEffect->process(implicitDefaults.data(), static_cast<int>(implicitDefaults.size()), 1);
    require(implicitDefaults == defaults, "default effect selects the darker R7 voice and 220 ms metallic tail");
    long long earlyEnergy = 0, lateEnergy = 0;
    for (int i = gateClosed; i < gateClosed + 2400; ++i)
        earlyEnergy += static_cast<long long>(defaults[i]) * defaults[i];
    for (int i = gateClosed + tailFrames - 2400; i < gateClosed + tailFrames; ++i)
        lateEnergy += static_cast<long long>(defaults[i]) * defaults[i];
    require(earlyEnergy > 10000 && lateEnergy < earlyEnergy / 4,
            "closed speech gate retains a decaying voice-only metallic tail");
    require(std::all_of(defaults.begin() + gateClosed + tailFrames, defaults.end(), [](short s) {return s == 0;}),
            "bounded metallic tail ends in exact silence after 220 ms");
    const auto persistentTail = render(burst, 0.0f, 3200.0f, 0.22f, 0.6f, 1.5f);
    require(std::any_of(persistentTail.begin() + gateClosed + tailFrames - 480,
                        persistentTail.begin() + gateClosed + tailFrames, [](short s) {return s != 0;}) &&
            std::all_of(persistentTail.begin() + gateClosed + tailFrames, persistentTail.end(), [](short s) {return s == 0;}),
            "tail duration caps even a long reverb decay at its configured deadline");
    const auto noStatic = render(burst, 0.0f);
    const auto maximumStatic = render(burst, 1.0f);
    require(!std::equal(noStatic.begin() + window, noStatic.begin() + gateClosed, maximumStatic.begin() + window) &&
            std::equal(noStatic.begin() + gateClosed, noStatic.end(), maximumStatic.begin() + gateClosed),
            "static changes speech texture but cannot enter any sample of the closed-gate wet tail");
    const auto hardCut = render(burst, 1.0f, 3200.0f, 0.0f, 0.6f, 1.5f);
    require(std::all_of(hardCut.begin() + gateClosed, hardCut.end(), [](short s) {return s == 0;}),
            "zero tail duration retains the immediate all-effects hard cut");
    const auto dry = render(burst, 1.0f, 3200.0f, 0.22f, 0.0f);
    require(std::all_of(dry.begin() + gateClosed, dry.end(), [](short s) {return s == 0;}),
            "zero reverb mix leaves no tail or residual static after speech");

    auto reopened = burst;
    std::copy(burst.begin(), burst.begin() + window, reopened.begin() + window * 2);
    const auto reopenOutput = render(reopened, 0.0f, 3200.0f, 0.22f, 0.6f, 1.5f);
    const auto tailOnly = render(burst, 0.0f, 3200.0f, 0.22f, 0.6f, 1.5f);
    require(!std::equal(reopenOutput.begin() + window * 3, reopenOutput.begin() + window * 3 + 64,
                        tailOnly.begin() + window * 3),
            "speech reopening during a tail adds new voice immediately without buffer warmup");
    auto noFirstBurst = reopened;
    std::fill(noFirstBurst.begin(), noFirstBurst.begin() + window, short{0});
    const auto freshVoice = render(noFirstBurst, 0.0f, 3200.0f, 0.22f, 0.6f, 1.5f);
    require(!std::equal(reopenOutput.begin() + window * 3, reopenOutput.begin() + window * 3 + 480,
                        freshVoice.begin() + window * 3),
            "reopened voice overlays the existing metallic resonance before the tail expires");

    const int disableTailAt = gateClosed + 1000;
    auto disableLiveTail = [&](const std::vector<short>& input) {
        auto output = input;
        auto effect = std::make_unique<deathTrooper::Effect>();
        effect->process(output.data(), disableTailAt, 1,
                        0.6f, 1.5f, 150.0f, -6.0f, 32.0f, 0.75f, 0.25f,
                        0.0f, 8.0f, 0.12f, true, -48.0f, 1.0f, 3200.0f, 0.22f);
        effect->process(output.data() + disableTailAt, static_cast<int>(output.size()) - disableTailAt, 1,
                        0.6f, 1.5f, 150.0f, -6.0f, 32.0f, 0.75f, 0.25f,
                        0.0f, 8.0f, 0.12f, true, -48.0f, 1.0f, 3200.0f, 0.0f);
        return output;
    };
    const auto disabledTail = disableLiveTail(reopened);
    const auto quietWhenDisabled = disableLiveTail(noFirstBurst);
    require(std::any_of(disabledTail.begin() + gateClosed, disabledTail.begin() + disableTailAt,
                        [](short s) {return s != 0;}) &&
            std::all_of(disabledTail.begin() + disableTailAt, disabledTail.begin() + window * 3,
                        [](short s) {return s == 0;}) &&
            std::equal(disabledTail.begin() + window * 3, disabledTail.end(), quietWhenDisabled.begin() + window * 3),
            "setting an active tail to zero cuts it immediately and cannot revive it on reopening");

    auto afterExpiry = burst;
    std::copy(burst.begin(), burst.begin() + window, afterExpiry.begin() + window * 3);
    auto quietBefore = afterExpiry;
    std::fill(quietBefore.begin(), quietBefore.begin() + window, short{0});
    const auto expiredOutput = render(afterExpiry, 0.0f);
    const auto previouslyQuiet = render(quietBefore, 0.0f);
    require(std::equal(expiredOutput.begin() + window * 4, expiredOutput.end(), previouslyQuiet.begin() + window * 4),
            "a later speech burst cannot revive an expired wet tail");
    require(render(reopened, 0.65f, 3200.0f, 0.22f, 0.25f, 0.35f, true) == render(reopened),
            "speech, tail, and reopening remain identical across variable packet boundaries");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    require(render(burst, 0.65f, nan, nan) == defaults && render(burst, 0.65f, inf, inf) == defaults,
            "non-finite low-pass and tail settings use the finite R7 defaults");
    require(render(burst, 0.65f, -100.0f, -1.0f) == render(burst, 0.65f, 1800.0f, 0.0f) &&
            render(burst, 0.65f, 50000.0f, 4.0f) == render(burst, 0.65f, 12000.0f, 0.6f),
            "darkness and metallic-tail controls clamp to supported limits");

    auto effect = std::make_unique<deathTrooper::Effect>();
    auto beforeReset = burst;
    effect->process(beforeReset.data(), gateClosed + 100, 1);
    effect->reset();
    std::vector<short> silence(window * 3, 0);
    effect->process(silence.data(), static_cast<int>(silence.size()), 1);
    require(std::all_of(silence.begin(), silence.end(), [](short s) {return s == 0;}),
            "full reset during an active tail clears voice, reverb, static and speech buffers");
}

int main(int argc, char** argv) {
    try {
        if (argc == 3) return renderPreview(argv[1], argv[2]);
        if (argc == 2 && std::strcmp(argv[1], "--digital-radio") == 0) {
            digitalRadioChecks();
            std::cout << checks << " digital-radio checks passed\n";
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--dark-tail") == 0) {
            darkTailChecks();
            std::cout << checks << " dark-tail checks passed\n";
            return 0;
        }
        require(!deathTrooper::shouldScramble(false, false) &&
                !deathTrooper::shouldScramble(false, true) &&
                 deathTrooper::shouldScramble(true, false) &&
                !deathTrooper::shouldScramble(true, true), "four speaker/listener helmet combinations");
        require(deathTrooper::helmetFromField("DTR1:1") &&
                deathTrooper::helmetFromField("DTR1:3") &&
                !deathTrooper::helmetFromField("DTR1:0") &&
                !deathTrooper::helmetFromField("") &&
                !deathTrooper::helmetFromField("DTR3:1") &&
                !deathTrooper::helmetFromField("true") &&
                !deathTrooper::helmetFromField("DTR1:1garbage"), "strict versioned protocol field");
        require(deathTrooper::scramblerFromField("DTR1:3") &&
                !deathTrooper::scramblerFromField("DTR1:1") &&
                !deathTrooper::scramblerFromField("DTR1:2") &&
                !deathTrooper::scramblerFromField("DTR1:0"), "ACE off keeps decoder without outgoing scramble");
        require(deathTrooper::helmetFromField("DTR2:1") && deathTrooper::helmetFromField("DTR2:3") &&
                !deathTrooper::scramblerFromField("DTR2:1") && deathTrooper::scramblerFromField("DTR2:3"),
                "DTR2 preserves Death Trooper decoder and ACE scrambler states");
        require(deathTrooper::speakerHelmetFromField("DTR2:4") &&
                !deathTrooper::helmetFromField("DTR2:4") && !deathTrooper::scramblerFromField("DTR2:4"),
                "ordinary helmet speaker state never grants Death Trooper decoding or scrambling");
        require(!deathTrooper::speakerHelmetFromField("DTR1:4") && !deathTrooper::speakerHelmetFromField("DTR2:5") &&
                !deathTrooper::speakerHelmetFromField("DTR2:3") && !deathTrooper::speakerHelmetFromField("DTR2:4garbage") &&
                !deathTrooper::speakerHelmetFromField(""), "ordinary helmet state rejects unknown and malformed fields");
        short monoCanary[] = {1234, -5678, 29000};
        short stereoFrames[4]{};
        tfarAudio::toStereo(monoCanary, stereoFrames, 2, 1);
        require(stereoFrames[0] == 1234 && stereoFrames[1] == 1234 && stereoFrames[2] == -5678 && stereoFrames[3] == -5678,
                "mono callback conversion duplicates each sample without reading a second channel");
        const short processedStereo[] = {30000, 30000, 6000, -10000};
        tfarAudio::fromStereo(processedStereo, monoCanary, 2, 1);
        require(monoCanary[0] == 30000 && monoCanary[1] == -2000 && monoCanary[2] == 29000,
                "mono downmix avoids overflow and preserves the sample-count boundary");
        short surround[] = {11, 22, 33, 44, 55, 66, 77, 88};
        tfarAudio::toStereo(surround, stereoFrames, 2, 4);
        tfarAudio::fromStereo(stereoFrames, surround, 2, 4);
        require(surround[0] == 11 && surround[1] == 22 && surround[2] == 0 && surround[3] == 0 &&
                surround[4] == 55 && surround[5] == 66 && surround[6] == 0 && surround[7] == 0,
                "wider callback layouts preserve the first stereo pair and clear remaining channels");

        deathTrooper::Effect silentEffect;
        std::vector<short> silent(48000 * 2, 0);
        silentEffect.process(silent.data(), 48000, 2);
        require(std::all_of(silent.begin(), silent.end(), [](short s) {return s == 0;}), "silence generates no noise");

        std::vector<short> positive(48000, 12000), signedWave(48000);
        for (size_t i = 0; i < signedWave.size(); ++i)
            signedWave[i] = (i % 117 < 58) ? 12000 : -12000;
        deathTrooper::Effect positiveEffect, signedEffect;
        positiveEffect.process(positive.data(), 48000, 1);
        signedEffect.process(signedWave.data(), 48000, 1);
        require(positive != signedWave, "amp retains speech waveform texture rather than envelope-only noise");
        require(std::all_of(signedWave.begin(), signedWave.begin() + deathTrooper::Effect::WindowFrames,
                           [](short s) {return s == 0;}), "no dry speech leaks during scrambling-buffer warmup");
        require(std::any_of(signedWave.begin(), signedWave.end(), [](short s) {return s != 0;}), "speech produces distorted amplifier output");

        std::vector<short> oneBlock(48000), manyBlocks;
        for (int i = 0; i < 48000; ++i) oneBlock[i] = static_cast<short>(18000 * std::sin(i * 0.05));
        manyBlocks = oneBlock;
        deathTrooper::Effect oneEffect, manyEffect;
        oneEffect.process(oneBlock.data(), 48000, 1);
        const int packetSizes[] = {1, 257, 480, 963, 1271};
        for (int i = 0, packet = 0; i < 48000; ++packet) {
            const int frames = std::min(packetSizes[packet % 5], 48000 - i);
            manyEffect.process(manyBlocks.data() + i, frames, 1);
            i += frames;
        }
        require(oneBlock == manyBlocks, "state continuous across variable packet boundaries");

        std::vector<short> pitchFixture(deathTrooper::Effect::WindowFrames * 4);
        for (size_t i = 0; i < pitchFixture.size(); ++i)
            pitchFixture[i] = static_cast<short>(14000 * std::sin(i * 6.28318530718 * 800.0 / 48000.0));
        auto loweredPitch = pitchFixture;
        auto originalPitch = pitchFixture;
        deathTrooper::Effect loweredPitchEffect, originalPitchEffect;
        loweredPitchEffect.process(loweredPitch.data(), static_cast<int>(loweredPitch.size()), 1, 0.0f, 0.05f, 150.0f);
        originalPitchEffect.process(originalPitch.data(), static_cast<int>(originalPitch.size()), 1, 0.0f, 0.05f, 150.0f, 0.0f);
        const double deepHz = measuredPitch(loweredPitch);
        const double unchangedHz = measuredPitch(originalPitch);
        std::cout << "Measured pitch: default " << deepHz << " Hz, zero semitones " << unchangedHz << " Hz\n";
        require(std::abs(deepHz - 565.6854249492) < 3.0, "default minus-six-semitone pitch converts 800 Hz to about 565.7 Hz");
        require(std::abs(unchangedHz - 800.0) < 3.0, "zero-semitone setting preserves 800 Hz pitch after scrambling");

        auto tooLowPitch = pitchFixture;
        auto tooHighPitch = pitchFixture;
        auto invalidPitch = pitchFixture;
        auto minimumPitch = pitchFixture;
        deathTrooper::Effect lowPitchLimit, highPitchLimit, invalidPitchEffect;
        auto minimumPitchEffect = std::make_unique<deathTrooper::Effect>();
        minimumPitchEffect->process(minimumPitch.data(), static_cast<int>(minimumPitch.size()), 1, 0.0f, 0.05f, 150.0f, -12.0f);
        lowPitchLimit.process(tooLowPitch.data(), static_cast<int>(tooLowPitch.size()), 1, 0.0f, 0.05f, 150.0f, -48.0f);
        highPitchLimit.process(tooHighPitch.data(), static_cast<int>(tooHighPitch.size()), 1, 0.0f, 0.05f, 150.0f, 12.0f);
        invalidPitchEffect.process(invalidPitch.data(), static_cast<int>(invalidPitch.size()), 1, 0.0f, 0.05f, 150.0f, std::numeric_limits<float>::quiet_NaN());
        require(tooLowPitch == minimumPitch && tooHighPitch == originalPitch, "out-of-range pitch values clamp to supported octave range");
        require(invalidPitch == loweredPitch, "invalid pitch value falls back to minus-six-semitone default");

        std::vector<short> stereo(48000 * 2, std::numeric_limits<short>::min());
        deathTrooper::Effect stereoEffect;
        stereoEffect.process(stereo.data(), 48000, 2);
        bool equalChannels = true;
        for (int i = 0; i < 48000; ++i) equalChannels &= stereo[i*2] == stereo[i*2+1];
        require(equalChannels, "mono replacement preserves equal channels before TFAR spatialization");
        require(std::all_of(stereo.begin(), stereo.end(), [](short s) {return std::abs(static_cast<int>(s)) <= 26214;}), "full-scale input bounded below clipping");

        short sentinels[] = {123, 456, 789};
        stereoEffect.process(sentinels, 1, 3);
        stereoEffect.process(nullptr, 1, 2);
        stereoEffect.process(sentinels, 0, 1);
        require(sentinels[0] == 123 && sentinels[1] == 456 && sentinels[2] == 789, "invalid buffers and channel counts untouched");

        stereoEffect.reset();
        std::fill(stereo.begin(), stereo.end(), short{0});
        stereoEffect.process(stereo.data(), 48000, 2);
        require(std::all_of(stereo.begin(), stereo.end(), [](short s) {return s == 0;}), "state reset leaves no residual voice or noise");

        std::vector<short> dryTail(96000, 0);
        for (int i = 0; i < 12000; ++i) dryTail[i] = static_cast<short>(20000 * std::sin(i * 0.11));
        auto wetTail = dryTail;
        deathTrooper::Effect noReverb, withReverb;
        noReverb.process(dryTail.data(), 96000, 1, 0.0f, 0.8f, 150.0f, -5.0f, 32.0f, 0.75f, 0.65f, 0.0f, 16.0f, 0.0f, false);
        withReverb.process(wetTail.data(), 96000, 1, 0.5f, 1.3f, 150.0f, -5.0f, 32.0f, 0.75f, 0.65f, 0.0f, 16.0f, 0.0f, false);
        long long dryEnergy = 0, wetEnergy = 0;
        for (size_t i = 30000; i < 60000; ++i) {
            dryEnergy += std::abs(static_cast<int>(dryTail[i]));
            wetEnergy += std::abs(static_cast<int>(wetTail[i]));
        }
        require(wetEnergy > dryEnergy + 100, "reverb prolongs distorted amp after source stops");
        require(std::all_of(wetTail.end()-4800, wetTail.end(), [](short s) {return std::abs(static_cast<int>(s)) < 100;}), "reverb feedback decays rather than building up");

        std::vector<short> lowCut(48000);
        for (int i = 0; i < 48000; ++i) lowCut[i] = static_cast<short>(18000 * std::sin(i * 6.28318530718 * 100.0 / 48000.0));
        auto highCut = lowCut;
        deathTrooper::Effect filter80, filter900;
        filter80.process(lowCut.data(), 48000, 1, 0.0f, 0.05f, 80.0f);
        filter900.process(highCut.data(), 48000, 1, 0.0f, 0.05f, 900.0f);
        double lowCutEnergy = 0, highCutEnergy = 0;
        for (size_t i = 12000; i < lowCut.size(); ++i) {
            lowCutEnergy += static_cast<double>(lowCut[i]) * lowCut[i];
            highCutEnergy += static_cast<double>(highCut[i]) * highCut[i];
        }
        require(highCutEnergy < lowCutEnergy * 0.8, "higher high-pass cutoff reduces low-frequency output energy");

        metallicChecks();
        screechChecks();
        staticChecks();
        digitalRadioChecks();
        darkTailChecks();

        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
