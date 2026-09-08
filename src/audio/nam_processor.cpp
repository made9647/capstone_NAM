#include "audio/nam_processor.h"

#include "NAM/dsp.h"
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

static_assert(std::is_same<NAM_SAMPLE, float>::value, "NAM must use the same float samples as Daisy");

NamProcessor::NamProcessor() = default;
NamProcessor::~NamProcessor() = default;

bool NamProcessor::SetModel(std::unique_ptr<nam::DSP> model, double sample_rate_hz, std::size_t max_block_size) {
    if (!model || !std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0)
        return false;
    if (max_block_size == 0 || max_block_size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
        return false;
    if (model->GetExpectedSampleRate() != sample_rate_hz)
        return false;

    try {
        model->SetPrewarmOnReset(true);
        model->Reset(sample_rate_hz, static_cast<int>(max_block_size));
    } catch (...) {
        return false;
    }
    // Commit the replacement only after prewarming succeeds.
    model_ = std::move(model);
    max_block_size_ = max_block_size;
    return true;
}

bool NamProcessor::Process(float *input, float *output, std::size_t frame_count) {
    if (!model_ || !input || !output || input == output || frame_count == 0 || frame_count > max_block_size_) {
        return false;
    }
    model_->process(&input, &output, static_cast<int>(frame_count));
    return true;
}
