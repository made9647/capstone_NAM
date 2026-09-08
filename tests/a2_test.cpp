#include "NAM/dsp.h"
#include "allocation_guard.h"
#include "models/amp_models.h"
#include "audio_stimulus.h"
#include "audio/nam_processor.h"
#include <array>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

int main() {
    for (int id = 1; id <= 3; ++id) {
        std::ifstream file("build/tests/a2-" + std::to_string(id) + ".f32", std::ios::binary);
        std::vector<float> reference(kTestSamples);
        file.read(reinterpret_cast<char *>(reference.data()), reference.size() * sizeof(float));
        assert(file.gcount() == static_cast<std::streamsize>(reference.size() * sizeof(float)));
        NamProcessor processor;
        assert(processor.SetModel(CreateAmpModel(static_cast<AmpId>(id)), 48000, 48));
        std::array<float, 48> input{}, output{};
        float maximum = 0;
        double error = 0, energy = 0;
        for (int offset = 0; offset < kTestSamples;) {
            const int frames = std::min(offset % 5 == 0 ? 1 : (offset % 3 == 0 ? 48 : 17), kTestSamples - offset);
            for (int i = 0; i < frames; ++i)
                input[i] = TestInput(offset + i);
            const auto allocations = allocation_count;
            assert(processor.Process(input.data(), output.data(), frames));
            assert(allocation_count == allocations);
            for (int i = 0; i < frames; ++i) {
                assert(std::isfinite(output[i]));
                double delta = output[i] - reference[offset + i];
                maximum = std::max(maximum, static_cast<float>(std::fabs(delta)));
                error += delta * delta;
                energy += reference[offset + i] * reference[offset + i];
            }
            offset += frames;
        }
        std::cout << AmpName(static_cast<AmpId>(id)) << ": max error=" << maximum << " ESR=" << error / energy << std::endl;
        assert(maximum < 1e-4 && error / energy < 1e-8);
        // Reinitialization must reset the entire convolution history.
        assert(processor.SetModel(CreateAmpModel(static_cast<AmpId>(id)), 48000, 48));
        input.fill(0);
        assert(processor.Process(input.data(), output.data(), 48));
        for (int i = 0; i < 48; ++i)
            assert(std::fabs(output[i] - reference[i]) < 1e-4);
    }
}
