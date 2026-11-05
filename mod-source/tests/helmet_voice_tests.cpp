#include "HelmetVoiceEffect.hpp"
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <cstdint>

static int checks = 0;
static void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
    ++checks;
    std::cout << "PASS " << description << '\n';
}
static std::vector<short> tone(int frames, double frequency = 800.0, double amplitude = .2) {
    std::vector<short> result(frames);
    for (int i = 0; i < frames; ++i)
        result[i] = static_cast<short>(32767.0 * amplitude * std::sin(i * 6.283185307179586 * frequency / 48000.0));
    return result;
}
static void append(std::vector<short>& to, const std::vector<short>& from) { to.insert(to.end(), from.begin(), from.end()); }
static void silence(std::vector<short>& to, int frames) { to.insert(to.end(), frames, short{0}); }
static std::vector<short> render(const std::vector<short>& input, float resHz = 2000, float resDb = 3,
                               float highPass = 623, float lowPass = 3100, float drive = 2.4f, float mix = 1,
                               float clarityHz = 2500, float clarityDb = 2) {
    auto output = input;
    helmetVoice::Effect effect;
    effect.process(output.data(), static_cast<int>(output.size()), 1, highPass, lowPass, drive, mix, resHz, resDb, clarityHz, clarityDb);
    return output;
}
static double energy(const std::vector<short>& input, int start, int end) {
    double result = 0;
    for (int i = start; i < end; ++i) result += static_cast<double>(input[i]) * input[i];
    return result;
}
static double spectralPower(const std::vector<short>& input, double hz, int begin = 24000, int end = 48000) {
    double real = 0, imaginary = 0;
    for (int i = begin; i < end; ++i) {
        const double phase = i * 6.283185307179586 * hz / 48000.0;
        real += input[i] * std::cos(phase);
        imaginary += input[i] * std::sin(phase);
    }
    return (real * real + imaginary * imaginary) / (static_cast<double>(end - begin) * (end - begin));
}
static double gainAt(double frequency, float resHz = 2000, float resDb = 3, float clarityHz = 2500, float clarityDb = 2) {
    auto input = tone(48000, frequency, .03);
    const auto output = render(input, resHz, resDb, 623, 3100, 2.4f, 1, clarityHz, clarityDb);
    return std::sqrt(energy(output, 24000, 48000) / energy(input, 24000, 48000));
}
static bool allSilent(const std::vector<short>& input, size_t begin = 0) {
    return std::all_of(input.begin() + begin, input.end(), [](short sample) { return sample == 0; });
}
static void runTests() {
    auto quiet = std::vector<short>(48000, 0);
    require(render(quiet) == quiet, "silence produces exact zero with no generated audio");
    auto input = tone(48000);
    const auto voice = render(input);
    int crossings = 0;
    for (int i = 12001; i < 36000; ++i) if (voice[i - 1] < 0 && voice[i] >= 0) ++crossings;
    const double pitch = crossings * 2.0;
    require(std::fabs(pitch - 800.0) <= 2.0, "bandpass helmet voice retains the forward 800 Hz pitch");
    std::cout << "Helmet measured pitch: " << pitch << " Hz\n";
    std::vector<short> impulse(12000, 0); impulse[0] = 12000;
    require(render(impulse)[0] != 0, "voice begins in the first sample without buffering or garbling");
    const double lowGain = gainAt(80), middleGain = gainAt(1000), highGain = gainAt(12000);
    require(lowGain < middleGain * .05 && highGain < middleGain * .08,
            "full-wet bandpass rejects deep bass and high fizz without a dry-voice floor");
    std::cout << "Helmet gains: 80 Hz " << lowGain << ", 1000 Hz " << middleGain << ", 12000 Hz " << highGain << '\n';
    const double lowerCutoff = gainAt(623, 2000, 0, 2500, 0), upperCutoff = gainAt(3100, 2000, 0, 2500, 0);
    require(std::fabs(lowerCutoff - .7071) < .012 && std::fabs(upperCutoff - .7071) < .012,
            "623 Hz and 3100 Hz cutoffs give approximately -3 dB response with both EQs bypassed");
    std::cout << "Bandpass cutoff gains without EQ: " << lowerCutoff << '/' << upperCutoff << '\n';

    const double middleBoost = gainAt(2000) / gainAt(2000, 2000, 0);
    const double lowBoost = gainAt(1000) / gainAt(1000, 2000, 0);
    const double upperBoost = gainAt(3000) / gainAt(3000, 2000, 0);
    const double measuredDb = 20 * std::log10(middleBoost);
    require(measuredDb > 2.9 && measuredDb < 3.1, "default presence adds approximately 3 dB at the requested 2 kHz center");
    require(middleBoost > lowBoost * 1.10 && middleBoost > upperBoost * 1.08,
            "presence EQ focuses the boost around 2 kHz rather than raising every frequency equally");
    std::cout << "Presence gain ratios at 1/2/3 kHz: " << lowBoost << '/' << middleBoost << '/' << upperBoost
              << "; center boost " << measuredDb << " dB\n";
    require(gainAt(1200, 1200, 3) > gainAt(1200, 2000, 3) * 1.15,
            "changing the resonance center moves the presence emphasis");
    require(render(input, 500, 0) == render(input, 4000, 0),
            "zero-dB presence is an exact bypass independent of the center setting");
    helmetVoice::Effect changingEq;
    auto beforeBypass = tone(4800, 1200, .6);
    changingEq.process(beforeBypass.data(), 4800, 1, 623, 3100, 2, 1, 1200, 9);
    auto bypassedGap = std::vector<short>(9600, 0);
    changingEq.process(bypassedGap.data(), 9600, 1, 623, 3100, 2, 1, 1200, 0);
    auto restoredGain = std::vector<short>(4800, 0);
    changingEq.process(restoredGain.data(), 4800, 1, 623, 3100, 2, 1, 1200, 9);
    require(allSilent(restoredGain), "zero-dB bypass clears stored EQ state so restoring gain cannot revive a tail");

    const auto clarityFixture = tone(48000, 2500, .12);
    const auto boostedClarity = render(clarityFixture);
    const auto bypassedClarity = render(clarityFixture, 2000, 3, 623, 3100, 2.4f, 1, 2500, 0);
    const double clarityRatio = std::sqrt(spectralPower(boostedClarity, 2500) / spectralPower(bypassedClarity, 2500));
    const double clarityMeasuredDb = 20 * std::log10(clarityRatio);
    require(clarityMeasuredDb > 1.98 && clarityMeasuredDb < 2.02,
            "post-amp clarity adds the requested 2 dB at 2500 Hz independently of amplifier compression");
    const double clarityLowRatio = gainAt(1000) / gainAt(1000, 2000, 3, 2500, 0);
    const double clarityHighRatio = gainAt(3800) / gainAt(3800, 2000, 3, 2500, 0);
    require(clarityRatio > clarityLowRatio * 1.15 && clarityRatio > clarityHighRatio * 1.05,
            "new clarity emphasis is centered near 2500 Hz rather than a uniform gain increase");
    std::cout << "Clarity boost at 2500 Hz: " << clarityMeasuredDb << " dB; ratios at 1000/2500/3800 Hz: "
              << clarityLowRatio << '/' << clarityRatio << '/' << clarityHighRatio << '\n';
    require(gainAt(1800, 2000, 3, 1800, 2) > gainAt(1800, 2000, 3, 3500, 2) * 1.10,
            "clarity center control moves its emphasis separately from the 2 kHz resonance");
    require(render(input, 2000, 3, 623, 3100, 2.4f, 1, 1000, 0) == render(input, 2000, 3, 623, 3100, 2.4f, 1, 4000, 0),
            "zero-dB clarity is an exact bypass independent of its center frequency");
    helmetVoice::Effect changingClarity;
    auto clarityExcitation = tone(4800, 2500, .6);
    changingClarity.process(clarityExcitation.data(), 4800, 1, 623, 3100, 2.4f, 1, 2000, 3, 2500, 6);
    auto clarityGap = std::vector<short>(9600, 0);
    changingClarity.process(clarityGap.data(), 9600, 1, 623, 3100, 2.4f, 1, 2000, 3, 2500, 0);
    auto restoredClarity = std::vector<short>(4800, 0);
    changingClarity.process(restoredClarity.data(), 4800, 1, 623, 3100, 2.4f, 1, 2000, 3, 2500, 6);
    require(allSilent(restoredClarity), "clarity bypass clears stored state so re-enabling it cannot revive old audio");

    const auto distortionFixture = tone(48000, 800, .55);
    const auto previousDrive = render(distortionFixture, 2000, 3, 623, 3100, 2, 1);
    const auto newDrive = render(distortionFixture);
    const double oldDistortion = std::sqrt(spectralPower(previousDrive, 2400) / spectralPower(previousDrive, 800));
    const double newDistortion = std::sqrt(spectralPower(newDrive, 2400) / spectralPower(newDrive, 800));
    require(newDistortion > oldDistortion * 1.15 && newDistortion < .2,
            "drive 2.4 adds modest in-band harmonic distortion compared with drive 2.0");
    std::cout << "In-band third harmonic ratio drive 2.0/2.4: " << oldDistortion << '/' << newDistortion << '\n';

    const auto loud = tone(48000, 1500, .7);
    const auto finalLowPass = render(loud, 2000, 0, 623, 3100, 4, 1);
    const auto wideLowPass = render(loud, 2000, 0, 623, 10000, 4, 1);
    const double narrowThird = std::sqrt(spectralPower(finalLowPass, 4500) / spectralPower(finalLowPass, 1500));
    const double wideThird = std::sqrt(spectralPower(wideLowPass, 4500) / spectralPower(wideLowPass, 1500));
    require(narrowThird < wideThird * .65, "final low-pass also suppresses high harmonics generated by the amplifier");
    std::cout << "Third harmonic ratio narrow/wide final filter: " << narrowThird << '/' << wideThird << '\n';
    const auto softAmp = render(loud, 2000, 0, 623, 10000, 1, 1);
    const double softThird = std::sqrt(spectralPower(softAmp, 4500) / spectralPower(softAmp, 1500));
    require(wideThird > softThird * 1.5, "amp drive remains independently adjustable under the new bandpass");

    const auto impulseOutput = render(impulse);
    require(energy(impulseOutput, 0, 480) > 0 && allSilent(impulseOutput, 1440),
            "presence response decays after an impulse instead of self-oscillating");
    const auto maximumEqImpulse = render(impulse, 500, 9, 80, 10000, 1, 1, 1000, 6);
    require(allSilent(maximumEqImpulse, 4800), "maximum resonance and clarity gains settle to exact silence with the lowest cutoffs");
    auto stopInput = tone(9600, 2000, .9); silence(stopInput, 14400);
    require(allSilent(render(stopInput), 14400), "speech stops without a static layer or extended synthetic tail");

    auto phrase = tone(4800); silence(phrase, 4800); append(phrase, tone(9600, 2000, .4)); silence(phrase, 12000);
    const auto onePacket = render(phrase);
    auto manyPackets = phrase;
    helmetVoice::Effect segmented;
    const int lengths[] = {1, 17, 479, 960, 133, 2048, 3};
    for (int i = 0, packet = 0; i < static_cast<int>(manyPackets.size()); ++packet) {
        const int length = std::min(lengths[packet % 7], static_cast<int>(manyPackets.size()) - i);
        segmented.process(manyPackets.data() + i, length, 1);
        i += length;
    }
    require(manyPackets == onePacket, "bandpass, EQ and amp are sample-exact across arbitrary packet boundaries");
    const auto rightInput = tone(48000, 2100, .1);
    const auto rightVoice = render(rightInput);
    std::vector<short> stereo(input.size() * 2);
    for (size_t i = 0; i < input.size(); ++i) { stereo[2 * i] = input[i]; stereo[2 * i + 1] = rightInput[i]; }
    helmetVoice::Effect stereoEffect;
    stereoEffect.process(stereo.data(), static_cast<int>(input.size()), 2);
    bool independentChannels = true;
    for (size_t i = 0; i < input.size(); ++i) independentChannels &= stereo[2 * i] == voice[i] && stereo[2 * i + 1] == rightVoice[i];
    require(independentChannels, "stereo channels have independent filters and match their separate mono renders");
    short sentinel[] = {100, -200, 300, -400};
    const short expected[] = {100, -200, 300, -400};
    stereoEffect.process(nullptr, 4, 1); stereoEffect.process(sentinel, 0, 1); stereoEffect.process(sentinel, 1, 3);
    require(std::memcmp(sentinel, expected, sizeof(sentinel)) == 0, "invalid buffers and unsupported channels remain untouched");
    require(render(input, 2000, 9, 623, 3100, 6, 0) == input,
            "zero effect mix is exact original PCM even with maximum resonance and amp settings");

    auto explicitDefaults = phrase;
    helmetVoice::Effect explicitEffect;
    explicitEffect.process(explicitDefaults.data(), static_cast<int>(explicitDefaults.size()), 1, 623, 3100, 2.4f, 1, 2000, 3, 2500, 2);
    require(explicitDefaults == onePacket, "R11 defaults retain the bandpass and resonance while adding clarity and mild distortion");
    auto invalid = phrase;
    helmetVoice::Effect invalidEffect;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    invalidEffect.process(invalid.data(), static_cast<int>(invalid.size()), 1, nan, nan, nan, nan, nan, nan, nan, nan);
    require(invalid == onePacket, "non-finite controls fall back to documented finite defaults");
    auto clamped = phrase, extremes = phrase;
    helmetVoice::Effect clampEffect, extremeEffect;
    clampEffect.process(clamped.data(), static_cast<int>(clamped.size()), 1, 80, 10000, 6, 1, 500, 9, 1000, 6);
    extremeEffect.process(extremes.data(), static_cast<int>(extremes.size()), 1, -999, 99999, 999, 999, -999, 999, -999, 999);
    require(clamped == extremes, "out-of-range frequency, resonance, clarity and amp controls clamp to supported limits");
    const auto lowFixture = tone(48000, 300, .1);
    require(energy(render(lowFixture, 2000, 0, 1200), 24000, 48000) < energy(render(lowFixture, 2000, 0, 80), 24000, 48000) * .05,
            "high-pass control changes low-frequency attenuation");
    const auto highFixture = tone(48000, 2500, .1);
    require(energy(render(highFixture, 2000, 0, 623, 1200), 24000, 48000) < energy(render(highFixture, 2000, 0, 623, 10000), 24000, 48000) * .1,
            "low-pass control changes upper-frequency attenuation");
    auto fullScale = tone(48000, 1000, .9999);
    auto hot = render(fullScale, 1000, 9, 80, 10000, 1, 1, 1000, 6);
    double alignment = 0, hotEnergy = 0, sourceEnergy = 0;
    for (int i = 12000; i < 48000; ++i) {
        alignment += static_cast<double>(fullScale[i]) * hot[i];
        hotEnergy += static_cast<double>(hot[i]) * hot[i];
        sourceEnergy += static_cast<double>(fullScale[i]) * fullScale[i];
    }
    require(alignment / std::sqrt(hotEnergy * sourceEnergy) > .90,
            "full-scale voice and maximum EQ remain bounded with no wrapping polarity inversion");
    stereoEffect.reset();
    quiet.assign(48000, 0); stereoEffect.process(quiet.data(), 48000, 1);
    require(allSilent(quiet), "full reset clears every bandpass and presence filter state");
    auto replay = phrase;
    segmented.resetSound();
    segmented.process(replay.data(), static_cast<int>(replay.size()), 1);
    require(replay == onePacket, "talkspurt reset restores a fresh deterministic effect state");
}

static int preview(const char* inputPath, const char* outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
    auto u16 = [&](size_t p) { uint16_t v; std::memcpy(&v, bytes.data() + p, 2); return v; };
    auto u32 = [&](size_t p) { uint32_t v; std::memcpy(&v, bytes.data() + p, 4); return v; };
    if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
        throw std::runtime_error("Expected RIFF WAVE input");
    uint16_t channels = 0;
    size_t audioOffset = 0, audioBytes = 0;
    for (size_t p = 12; p + 8 <= bytes.size();) {
        const size_t size = u32(p + 4);
        if (size > bytes.size() - p - 8) throw std::runtime_error("Truncated WAV");
        if (!std::memcmp(bytes.data() + p, "fmt ", 4)) {
            if (size < 16 || u16(p + 8) != 1 || u32(p + 12) != 48000 || u16(p + 22) != 16)
                throw std::runtime_error("Expected PCM16 48000 Hz");
            channels = u16(p + 10);
        }
        if (!std::memcmp(bytes.data() + p, "data", 4)) { audioOffset = p + 8; audioBytes = size; }
        p += 8 + size + (size & 1);
    }
    if ((channels != 1 && channels != 2) || audioBytes == 0 || audioBytes % (channels * 2))
        throw std::runtime_error("Unsupported WAV channels/data");
    std::vector<short> samples(audioBytes / 2);
    std::memcpy(samples.data(), bytes.data() + audioOffset, audioBytes);
    samples.insert(samples.end(), 24000 * channels, short{0}); // Preserve the natural short filter decay.
    helmetVoice::Effect effect;
    effect.process(samples.data(), static_cast<int>(samples.size() / channels), channels);
    std::ofstream output(outputPath, std::ios::binary);
    auto w16 = [&](uint16_t v) { output.write(reinterpret_cast<const char*>(&v), 2); };
    auto w32 = [&](uint32_t v) { output.write(reinterpret_cast<const char*>(&v), 4); };
    const auto dataSize = static_cast<uint32_t>(samples.size() * 2);
    output.write("RIFF", 4); w32(dataSize + 36); output.write("WAVEfmt ", 8);
    w32(16); w16(1); w16(channels); w32(48000); w32(48000 * channels * 2);
    w16(channels * 2); w16(16); output.write("data", 4); w32(dataSize);
    output.write(reinterpret_cast<const char*>(samples.data()), dataSize);
    if (!output) throw std::runtime_error("Cannot write preview");
    std::cout << "Helmet effect-only preview written; 623-3100 Hz, presence 2000 Hz/+3 dB, clarity 2500 Hz/+2 dB, drive 2.4, frames "
              << samples.size() / channels << '\n';
    return 0;
}
int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--preview") return preview(argv[2], argv[3]);
        if (argc != 1) throw std::runtime_error("Usage: helmet_voice_tests [--preview INPUT.wav OUTPUT.wav]");
        runTests();
        std::cout << checks << " helmet checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
