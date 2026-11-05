#pragma once

// TFAR processes stereo internally. Mono callbacks must never read/write a
// nonexistent second channel; wider layouts retain the original first pair.
namespace tfarAudio {
inline void toStereo(const short* input, short* stereo, int frames, int channels) noexcept {
    for (int i = 0; i < frames; ++i) {
        stereo[i * 2] = input[i * channels];
        stereo[i * 2 + 1] = input[i * channels + (channels == 1 ? 0 : 1)];
    }
}
inline void fromStereo(const short* stereo, short* output, int frames, int channels) noexcept {
    for (int i = 0; i < frames; ++i) {
        if (channels == 1) {
            output[i] = static_cast<short>((static_cast<int>(stereo[i * 2]) + stereo[i * 2 + 1]) / 2);
        } else {
            output[i * channels] = stereo[i * 2];
            output[i * channels + 1] = stereo[i * 2 + 1];
            for (int ch = 2; ch < channels; ++ch) output[i * channels + ch] = 0;
        }
    }
}
}
