// A2-Lite equations and weight ordering follow NeuralAmpModelerCore's WaveNet.
// See NAM-LICENSE for the upstream MIT notice.
#include "models/a2_lite.h"
#include <algorithm>

namespace {
constexpr int kChannels = 3;
constexpr int kWeightsPerTap = kChannels * kChannels;
constexpr int kLayerTailWeights = 18;
constexpr int kHeadTaps = 16;
constexpr int kHeadMask = kHeadTaps - 1;
constexpr int kHeadBiasOffset = kHeadTaps * kChannels;
constexpr int kHeadScaleOffset = kHeadBiasOffset + 1;

constexpr int kKernelSizes[] = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 15, 15, 6, 6, 6, 6, 6, 6, 6};
constexpr int kDilations[] = {1, 3, 7, 17, 41, 101, 239, 1, 3, 7, 17, 41, 101, 239, 1, 13, 1, 3, 7, 17, 41, 101, 239};
} // namespace

A2Lite::A2Lite(const float *weights) : DSP(1, 1, 48000.0), weights_(weights) {
    const float *next = weights + kChannels;
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        auto &layer = layers_[i];
        layer.kernel_size = kKernelSizes[i];
        layer.dilation = kDilations[i];
        uint32_t history_size = 1;
        // A power-of-two ring buffer lets the mask wrap each delayed tap.
        while (history_size <= static_cast<uint32_t>((layer.kernel_size - 1) * layer.dilation))
            history_size *= 2;
        layer.history_wrap_mask = history_size - 1;
        layer.history.resize(history_size * kChannels, 0.0f);
        layer.weights = next;
        next += layer.kernel_size * kWeightsPerTap + kLayerTailWeights;
    }
    head_weights_ = next;
}

void A2Lite::Reset(double sample_rate_hz, int max_block_size) {
    for (auto &layer : layers_) {
        std::fill(layer.history.begin(), layer.history.end(), 0.0f);
        layer.history_write_index = 0;
    }
    head_history_.fill(0.0f);
    head_history_write_index_ = 0;
    DSP::Reset(sample_rate_hz, max_block_size);
}

void A2Lite::ProcessLayer(Layer &layer, float conditioning_sample, float *residual_features, float *skip_accumulator) {
    float *current = &layer.history[layer.history_write_index * kChannels];
    std::copy(residual_features, residual_features + kChannels, current);

    // After the convolution: bias[3], input conditioning[3],
    // residual projection[9], and residual bias[3].
    const float *bias = layer.weights + layer.kernel_size * kWeightsPerTap;
    const float *conditioning = bias + kChannels;
    const float *residual_weights = conditioning + kChannels;
    const float *residual_bias = residual_weights + kWeightsPerTap;
    float activation[kChannels] = {bias[0], bias[1], bias[2]};
    const float *tap_weights = layer.weights;
    for (int tap = 0; tap < layer.kernel_size; ++tap, tap_weights += kWeightsPerTap) {
        const uint32_t delay = static_cast<uint32_t>((layer.kernel_size - 1 - tap) * layer.dilation);
        const uint32_t history_read_index = (layer.history_write_index - delay) & layer.history_wrap_mask;
        const float *history = &layer.history[history_read_index * kChannels];
        // Preserve the upstream accumulation order for each output channel.
        activation[0] += tap_weights[0] * history[0];
        activation[0] += tap_weights[3] * history[1];
        activation[0] += tap_weights[6] * history[2];
        activation[1] += tap_weights[1] * history[0];
        activation[1] += tap_weights[4] * history[1];
        activation[1] += tap_weights[7] * history[2];
        activation[2] += tap_weights[2] * history[0];
        activation[2] += tap_weights[5] * history[1];
        activation[2] += tap_weights[8] * history[2];
    }

    for (int channel = 0; channel < kChannels; ++channel) {
        activation[channel] += conditioning[channel] * conditioning_sample;
        activation[channel] = activation[channel] >= 0.0f ? activation[channel] : 0.01f * activation[channel];
        skip_accumulator[channel] += activation[channel];
    }
    for (int channel = 0; channel < kChannels; ++channel) {
        const float *projection = residual_weights + channel * kChannels;
        residual_features[channel] += residual_bias[channel] + projection[0] * activation[0] + projection[1] * activation[1] + projection[2] * activation[2];
    }
    layer.history_write_index = (layer.history_write_index + 1) & layer.history_wrap_mask;
}

float A2Lite::ProcessHead(const float *skip_accumulator) {
    std::copy(skip_accumulator, skip_accumulator + kChannels, &head_history_[head_history_write_index_ * kChannels]);
    float output = head_weights_[kHeadBiasOffset];
    for (uint32_t tap = 0; tap < kHeadTaps; ++tap) {
        const uint32_t history_read_index = (head_history_write_index_ - kHeadMask + tap) & kHeadMask;
        const float *history = &head_history_[history_read_index * kChannels];
        for (int channel = 0; channel < kChannels; ++channel)
            output += head_weights_[tap * kChannels + channel] * history[channel];
    }
    head_history_write_index_ = (head_history_write_index_ + 1) & kHeadMask;
    return output * head_weights_[kHeadScaleOffset];
}

float A2Lite::ProcessSample(float input_sample) {
    float residual_features[kChannels] = {weights_[0] * input_sample, weights_[1] * input_sample, weights_[2] * input_sample};
    float skip_accumulator[kChannels] = {};
    for (auto &layer : layers_)
        ProcessLayer(layer, input_sample, residual_features, skip_accumulator);
    return ProcessHead(skip_accumulator);
}

void A2Lite::process(float **input, float **output, int frame_count) {
    for (int i = 0; i < frame_count; ++i)
        output[0][i] = ProcessSample(input[0][i]);
}
