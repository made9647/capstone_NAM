#pragma once
#include <cmath>

inline float TestInput(int sample) {
    if (sample < 4800)
        return 0.0f;
    if (sample == 4800)
        return 1.0f;
    if (sample < 9600)
        return 0.0f;
    if (sample < 14400)
        return 0.2f;
    return 0.7f * std::sin(static_cast<float>(sample) * 0.071f) + 0.2f * std::sin(static_cast<float>(sample) * 0.013f);
}
inline constexpr int kTestSamples = 24000;
