// Live real-time mic-in, amp-processed-out, using PortAudio.
// Runs on NATIVE Windows (via MSYS2 MinGW64) — NOT WSL, since WSL has no
// direct audio device access (confirmed: PortAudio finds 0 devices there).
// Takes explicit input/output device indices (see list_devices.cpp) rather
// than defaults, so you can target your headset mic for input and your
// speakers for output separately.
//
// Build (from an MSYS2 MinGW64 shell — NOT the WSL terminal — after cloning
// the repo natively, running scripts/download_models.sh, and generating
// build/generated/embedded_a2_data.h via `python3 scripts/convert_a2.py
// build/generated/embedded_a2_data.h`):
//   g++ -std=c++17 -O2 -Ibuild/generated -Isrc \
//     -Ilibs/NeuralAmpModelerCore -Ilibs/NeuralAmpModelerCore/NAM \
//     -Ilibs/NeuralAmpModelerCore/Dependencies/eigen \
//     -Ilibs/NeuralAmpModelerCore/Dependencies/nlohmann \
//     -DNAM_SAMPLE_FLOAT -DNAM_USE_INLINE_GEMM \
//     tools/live_mic.cpp src/audio/nam_audio.cpp src/models/amp_models.cpp \
//     src/models/a2_lite.cpp src/audio/nam_processor.cpp \
//     libs/NeuralAmpModelerCore/NAM/dsp.cpp -lportaudio -o live_mic.exe
//
// Run: ./live_mic.exe amp_id input_device_index output_device_index
//   e.g. ./live_mic.exe 1 15 13
// Press Enter to stop. Expect real latency (tens of ms) since this goes
// through Windows' shared-mode audio stack, not a low-latency ASIO path.

#include "audio/nam_audio.h"
#include "models/amp_models.h"
#include <portaudio.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace {

struct CallbackData {
    NamAudio *audio;
};

int AudioCallback(const void *input_buffer, void *output_buffer,
                   unsigned long frame_count,
                   const PaStreamCallbackTimeInfo *,
                   PaStreamCallbackFlags,
                   void *user_data) {
    auto *data = static_cast<CallbackData *>(user_data);
    const auto *in = static_cast<const float *>(input_buffer);
    auto *out = static_cast<float *>(output_buffer);

    if (!in) {
        // Device still warming up / no input yet — output silence.
        std::fill(out, out + static_cast<std::size_t>(frame_count) * 2, 0.0f);
        return paContinue;
    }

    std::size_t offset = 0;
    std::array<float, NamAudio::kBlockSize> mono_in{}, left{}, right{};
    while (offset < frame_count) {
        const std::size_t chunk = std::min(NamAudio::kBlockSize, static_cast<std::size_t>(frame_count) - offset);
        // Input is interleaved stereo (we requested 2 channels for device
        // compatibility); take the left channel as the mono signal.
        for (std::size_t i = 0; i < chunk; ++i)
            mono_in[i] = in[(offset + i) * 2];

        data->audio->Process(mono_in.data(), left.data(), right.data(), chunk, /*bypass_model=*/false);

        for (std::size_t i = 0; i < chunk; ++i) {
            out[(offset + i) * 2 + 0] = left[i];
            out[(offset + i) * 2 + 1] = right[i];
        }
        offset += chunk;
    }
    return paContinue;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " amp_id(1=Fender 2=Vox 3=Marshall) input_device_index output_device_index\n";
        std::cerr << "Run list_devices.exe to find the right indices.\n";
        return 2;
    }
    const int amp_id = std::atoi(argv[1]);
    if (amp_id < 1 || amp_id > 3) {
        std::cerr << "amp_id must be 1, 2, or 3\n";
        return 2;
    }
    const int input_device = std::atoi(argv[2]);
    const int output_device = std::atoi(argv[3]);

    NamAudio audio;
    if (!audio.LoadAmpModel(static_cast<AmpId>(amp_id))) {
        std::cerr << "Failed to load amp model " << amp_id << "\n";
        return 1;
    }

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << "\n";
        return 1;
    }

    const PaDeviceInfo *in_info = Pa_GetDeviceInfo(input_device);
    const PaDeviceInfo *out_info = Pa_GetDeviceInfo(output_device);
    if (!in_info || !out_info) {
        std::cerr << "Invalid device index. Run list_devices.exe to check.\n";
        Pa_Terminate();
        return 1;
    }
    std::cout << "Input:  [" << input_device << "] " << in_info->name << "\n";
    std::cout << "Output: [" << output_device << "] " << out_info->name << "\n";

    PaStreamParameters input_params{};
    input_params.device = input_device;
    input_params.channelCount = 2;
    input_params.sampleFormat = paFloat32;
    input_params.suggestedLatency = in_info->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters output_params{};
    output_params.device = output_device;
    output_params.channelCount = 2;
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = out_info->defaultLowOutputLatency;
    output_params.hostApiSpecificStreamInfo = nullptr;

    CallbackData data{&audio};
    PaStream *stream = nullptr;
    err = Pa_OpenStream(&stream,
                         &input_params,
                         &output_params,
                         NamAudio::kSampleRate,
                         NamAudio::kBlockSize,
                         paNoFlag,
                         AudioCallback,
                         &data);
    if (err != paNoError) {
        std::cerr << "Failed to open stream: " << Pa_GetErrorText(err) << "\n";
        Pa_Terminate();
        return 1;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to start stream: " << Pa_GetErrorText(err) << "\n";
        Pa_Terminate();
        return 1;
    }

    std::cout << "Running " << AmpName(static_cast<AmpId>(amp_id)) << " live. Press Enter to stop.\n";
    std::cin.get();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}