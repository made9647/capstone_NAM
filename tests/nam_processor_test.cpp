#include "NAM/dsp.h"
#include "audio/nam_processor.h"
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

static std::unique_ptr<nam::DSP> MakeModel() { return std::make_unique<nam::DSP>(1, 1, 48000.0); }

class PrewarmModel : public nam::DSP {
  public:
    PrewarmModel() : DSP(1, 1, 48000.0) {}
    int GetPrewarmSamples() override { return 96; }
    void process(float **input, float **output, int frames) override {
        samples += frames;
        DSP::process(input, output, frames);
    }
    int samples = 0;
};

class FailingModel : public nam::DSP {
  public:
    FailingModel() : DSP(1, 1, 48000.0) {}
    void Reset(double, int) override { throw std::runtime_error("test failure"); }
};

int main() {
    NamProcessor processor;
    std::array<float, 48> input{}, output{}, expected{};
    output.fill(123.0f);
    assert(!processor.Process(input.data(), output.data(), 48));
    assert(output[0] == 123.0f);
    assert(!processor.SetModel(nullptr, 48000.0, 48));
    assert(!processor.SetModel(MakeModel(), 44100.0, 48));
    assert(!processor.SetModel(MakeModel(), 48000.0, 0));
    assert(processor.SetModel(MakeModel(), 48000.0, 48));
    assert(!processor.SetModel(MakeModel(), std::numeric_limits<double>::quiet_NaN(), 48));
    assert(!processor.SetModel(std::make_unique<nam::DSP>(2, 1, 48000.0), 48000.0, 48));
    assert(!processor.SetModel(std::make_unique<nam::DSP>(1, 1, -1.0), 48000.0, 48));
    assert(processor.SetModel(MakeModel(), 48000.0, 48));
    auto warming = std::make_unique<PrewarmModel>();
    auto *warmed = warming.get();
    warming->SetPrewarmOnReset(false);
    assert(processor.SetModel(std::move(warming), 48000.0, 48));
    assert(warmed->samples == 96); // SetModel must prewarm exactly once.
    auto reference = MakeModel();
    reference->Reset(48000.0, 48);
    assert(!processor.SetModel(std::make_unique<FailingModel>(), 48000.0, 48));
    assert(!processor.Process(input.data(), output.data(), 49));
    assert(!processor.Process(nullptr, output.data(), 48));
    assert(!processor.Process(input.data(), input.data(), 48));
    assert(output[0] == 123.0f);
    for (int block = 0; block < 10; ++block) {
        const int frames = block % 2 ? 17 : 48;
        for (int i = 0; i < frames; ++i)
            input[i] = std::sin(static_cast<float>(block * 48 + i) * 0.1f);
        float *in = input.data();
        float *out = expected.data();
        reference->process(&in, &out, frames);
        assert(processor.Process(input.data(), output.data(), frames));
        for (int i = 0; i < frames; ++i)
            assert(std::isfinite(output[i]) && std::fabs(output[i] - expected[i]) < 1e-6f);
    }
}
