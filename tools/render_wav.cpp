// Offline test tool: render a mono 48 kHz 16-bit PCM WAV file through one of
// the three embedded NAM amp models, using the exact same NamAudio code path
// that runs on the Daisy hardware. This is for testing on your computer with
// no board attached.
//
// mp3/other formats need converting first (ffmpeg handles decoding):
//   ffmpeg -i solo.mp3 -ar 48000 -ac 1 -sample_fmt s16 solo_48k_mono.wav
//
// Build from the repo root (after `make model` has generated the embedded
// weights header at build/generated/embedded_a2_data.h):
//   g++ -std=c++17 -O2 -Ibuild/generated -Isrc \
//     -Ilibs/NeuralAmpModelerCore -Ilibs/NeuralAmpModelerCore/NAM \
//     -Ilibs/NeuralAmpModelerCore/Dependencies/eigen \
//     -Ilibs/NeuralAmpModelerCore/Dependencies/nlohmann \
//     -DNAM_SAMPLE_FLOAT -DNAM_USE_INLINE_GEMM \
//     tools/render_wav.cpp src/audio/nam_audio.cpp src/models/amp_models.cpp \
//     src/models/a2_lite.cpp src/audio/nam_processor.cpp \
//     libs/NeuralAmpModelerCore/NAM/dsp.cpp -o render_wav
//
// Run (amp_id: 1=Fender Twin65, 2=Vox AC30, 3=Marshall JCM800):
//   ./render_wav solo_48k_mono.wav solo_fender.wav 1
//
// Then, if you want an mp3 back:
//   ffmpeg -i solo_fender.wav solo_fender.mp3

#include "audio/nam_audio.h"
#include "models/amp_models.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t chunk_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};
#pragma pack(pop)

bool ReadWav(const std::string &path, std::vector<float> &samples) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open " << path << "\n";
        return false;
    }
    WavHeader header{};
    file.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!file || std::memcmp(header.riff, "RIFF", 4) != 0 || std::memcmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << path << ": not a RIFF/WAVE file\n";
        return false;
    }
    if (header.audio_format != 1 || header.num_channels != 1 || header.bits_per_sample != 16 || header.sample_rate != 48000) {
        std::cerr << path << ": need mono 16-bit PCM at 48000 Hz. Convert first, e.g.\n"
                   << "  ffmpeg -i " << path << " -ar 48000 -ac 1 -sample_fmt s16 fixed.wav\n";
        return false;
    }
    // Skip to the "data" chunk; the fmt chunk may carry extra bytes.
    file.seekg(20 + header.fmt_size, std::ios::beg);
    char chunk_id[4];
    uint32_t chunk_size = 0;
    while (file.read(chunk_id, 4)) {
        file.read(reinterpret_cast<char *>(&chunk_size), sizeof(chunk_size));
        if (std::memcmp(chunk_id, "data", 4) == 0)
            break;
        file.seekg(chunk_size, std::ios::cur);
    }
    if (!file) {
        std::cerr << path << ": no data chunk found\n";
        return false;
    }
    std::vector<int16_t> raw(chunk_size / 2);
    file.read(reinterpret_cast<char *>(raw.data()), chunk_size);
    samples.resize(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i)
        samples[i] = raw[i] / 32768.0f;
    return true;
}

bool WriteWav(const std::string &path, const std::vector<float> &samples) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot create " << path << "\n";
        return false;
    }
    std::vector<int16_t> raw(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
        raw[i] = static_cast<int16_t>(clamped * 32767.0f);
    }
    const uint32_t data_size = static_cast<uint32_t>(raw.size() * sizeof(int16_t));
    WavHeader header{};
    std::memcpy(header.riff, "RIFF", 4);
    header.chunk_size = 36 + data_size;
    std::memcpy(header.wave, "WAVE", 4);
    std::memcpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.audio_format = 1;
    header.num_channels = 1;
    header.sample_rate = 48000;
    header.bits_per_sample = 16;
    header.block_align = static_cast<uint16_t>(header.num_channels * header.bits_per_sample / 8);
    header.byte_rate = header.sample_rate * header.block_align;
    file.write(reinterpret_cast<const char *>(&header), sizeof(header));
    file.write("data", 4);
    file.write(reinterpret_cast<const char *>(&data_size), sizeof(data_size));
    file.write(reinterpret_cast<const char *>(raw.data()), data_size);
    return file.good();
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " input.wav output.wav amp_id(1=Fender 2=Vox 3=Marshall)\n";
        return 2;
    }
    const int amp_id = std::atoi(argv[3]);
    if (amp_id < 1 || amp_id > 3) {
        std::cerr << "amp_id must be 1, 2, or 3\n";
        return 2;
    }

    std::vector<float> input_samples;
    if (!ReadWav(argv[1], input_samples))
        return 1;

    NamAudio audio;
    if (!audio.LoadAmpModel(static_cast<AmpId>(amp_id))) {
        std::cerr << "Failed to load amp model " << amp_id << "\n";
        return 1;
    }

    std::vector<float> output_samples(input_samples.size());
    std::array<float, NamAudio::kBlockSize> in_block{}, left{}, right{};
    for (std::size_t offset = 0; offset < input_samples.size();) {
        const std::size_t frames = std::min(NamAudio::kBlockSize, input_samples.size() - offset);
        std::copy(input_samples.begin() + static_cast<long>(offset),
                   input_samples.begin() + static_cast<long>(offset + frames), in_block.begin());
        audio.Process(in_block.data(), left.data(), right.data(), frames, /*bypass_model=*/false);
        std::copy(left.begin(), left.begin() + static_cast<long>(frames),
                   output_samples.begin() + static_cast<long>(offset));
        offset += frames;
    }

    if (!WriteWav(argv[2], output_samples))
        return 1;

    std::cout << "Rendered " << AmpName(static_cast<AmpId>(amp_id)) << " -> " << argv[2] << "\n";
    return 0;
}