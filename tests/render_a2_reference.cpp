#include "NAM/get_dsp.h"
#include "audio_stimulus.h"
#include <array>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    // Generic upstream WaveNet, without NAM_ENABLE_A2_FAST, is independent of
    // our fixed-shape engine and offline weight rearrangement.
    auto model = nam::get_dsp(std::filesystem::path(argv[1]));
    model->Reset(48000.0, 48);
    std::ofstream file(argv[2], std::ios::binary);
    std::array<float, 48> input{}, output{};
    for (int offset = 0; offset < kTestSamples; offset += 48) {
        for (int i = 0; i < 48; ++i)
            input[i] = TestInput(offset + i);
        float *in = input.data(), *out = output.data();
        model->process(&in, &out, 48);
        file.write(reinterpret_cast<const char *>(output.data()), sizeof(output));
    }
    return file.good() ? 0 : 1;
}
