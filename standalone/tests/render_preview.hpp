#pragma once
#include <fstream>
#include <string>
#include <cstring>

// Development-only fixture renderer. Accepts PCM16 at TFAR's 48 kHz sample rate.
inline int renderPreview(const char* inputPath, const char* outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
    auto u16 = [&](size_t p) {uint16_t v; std::memcpy(&v, bytes.data()+p, 2); return v;};
    auto u32 = [&](size_t p) {uint32_t v; std::memcpy(&v, bytes.data()+p, 4); return v;};
    if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data()+8, "WAVE", 4))
        throw std::runtime_error("Expected RIFF WAVE input");
    uint16_t channels = 0;
    size_t audioOffset = 0, audioBytes = 0;
    for (size_t p = 12; p + 8 <= bytes.size();) {
        const size_t size = u32(p+4);
        if (p + 8 + size > bytes.size()) throw std::runtime_error("Truncated WAV");
        if (!std::memcmp(bytes.data()+p, "fmt ", 4)) {
            if (size < 16 || u16(p+8) != 1 || u32(p+12) != 48000 || u16(p+22) != 16)
                throw std::runtime_error("Expected PCM16 48000 Hz");
            channels = u16(p+10);
        }
        if (!std::memcmp(bytes.data()+p, "data", 4)) {audioOffset=p+8; audioBytes=size;}
        p += 8 + size + (size & 1);
    }
    if ((channels != 1 && channels != 2) || audioBytes == 0 || audioBytes % (channels*2))
        throw std::runtime_error("Unsupported WAV channels/data");
    std::vector<short> dry(audioBytes / 2);
    std::memcpy(dry.data(), bytes.data()+audioOffset, audioBytes);
    auto wet = dry;
    // Pad out the grain delay/tail so the final syllable is audible in the preview.
    wet.insert(wet.end(), 72000 * channels, short{0});
    deathTrooper::Effect effect;
    effect.process(wet.data(), static_cast<int>(wet.size()/channels), channels);
    std::vector<short> combined = dry;
    combined.insert(combined.end(), 48000 * channels, short{0});
    combined.insert(combined.end(), wet.begin(), wet.end());
    std::ofstream output(outputPath, std::ios::binary);
    auto w16 = [&](uint16_t v) {output.write(reinterpret_cast<char*>(&v), 2);};
    auto w32 = [&](uint32_t v) {output.write(reinterpret_cast<char*>(&v), 4);};
    const auto dataSize = static_cast<uint32_t>(combined.size()*2);
    output.write("RIFF", 4); w32(dataSize + 36); output.write("WAVEfmt ", 8);
    w32(16); w16(1); w16(channels); w32(48000); w32(48000*channels*2);
    w16(channels*2); w16(16); output.write("data", 4); w32(dataSize);
    output.write(reinterpret_cast<const char*>(combined.data()), dataSize);
    if (!output) throw std::runtime_error("Cannot write preview");
    std::cout << "Preview written: clear speech, one-second gap, scrambled speech.\n";
    return 0;
}
