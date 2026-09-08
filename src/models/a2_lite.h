#pragma once

#include "NAM/dsp.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Fixed 3-channel, 23-layer A2-Lite WaveNet. Only configurations validated by
// convert_a2.py are accepted. Weights must outlive this object (embedded
// flash).
class A2Lite final : public nam::DSP {
  public:
    static constexpr std::size_t kWeights = 1871;
    explicit A2Lite(const float *weights);
    void Reset(double sample_rate_hz, int max_block_size) override;
    void process(float **input, float **output, int frame_count) override;

  private:
    struct Layer {
        const float *weights;
        std::vector<float> history;
        uint32_t history_write_index = 0;
        uint32_t history_wrap_mask;
        int kernel_size;
        int dilation;
    };
    int GetPrewarmSamples() override { return 6347; }

    static void ProcessLayer(Layer &layer, float conditioning_sample, float *residual_features, float *skip_accumulator);
    float ProcessHead(const float *skip_accumulator);
    float ProcessSample(float input_sample);

    const float *weights_;
    const float *head_weights_;
    std::array<Layer, 23> layers_;
    std::array<float, 16 * 3> head_history_{};
    uint32_t head_history_write_index_ = 0;
};
