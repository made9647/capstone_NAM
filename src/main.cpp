#include "daisy_seed.h"
#include "audio/nam_audio.h"
#include <atomic>
#include <cstdint>

daisy::DaisySeed seed;
static NamAudio audio;
static std::atomic<uint32_t> audio_callbacks{0};
static std::atomic<uint32_t> max_callback_us{0};
static std::atomic<uint32_t> overruns{0};
static std::atomic<uint32_t> requested_amp{1};
static_assert(ATOMIC_INT_LOCK_FREE == 2, "Audio counters must be lock-free");
// Only changed while audio is stopped.
static bool bypass = false;

static void RecordCallbackTiming(uint32_t elapsed_us) {
    uint32_t previous = max_callback_us.load(std::memory_order_relaxed);
    while (elapsed_us > previous && !max_callback_us.compare_exchange_weak(previous, elapsed_us, std::memory_order_relaxed)) {
    }
    if (elapsed_us >= 1000)
        overruns.fetch_add(1, std::memory_order_relaxed);
    audio_callbacks.fetch_add(1, std::memory_order_relaxed);
}

void AudioCallback(daisy::AudioHandle::InputBuffer input_channels, daisy::AudioHandle::OutputBuffer output_channels, size_t frame_count) {
    const uint32_t start_us = daisy::System::GetUs();
    audio.Process(input_channels[0], output_channels[0], output_channels[1], frame_count, bypass);
    const uint32_t elapsed_us = daisy::System::GetUs() - start_us;
    RecordCallbackTiming(elapsed_us);
}

// USB interrupt: record a command only. Never allocate or construct a model
// here.
void OnUsbReceive(uint8_t *data, uint32_t *byte_count) {
    for (uint32_t i = 0; i < *byte_count; ++i)
        if (data[i] >= '0' && data[i] <= '3')
            requested_amp.store(data[i] - '0', std::memory_order_relaxed);
}

struct AppState {
    AmpId selected_amp = AmpId::Fender;
    uint32_t current_command = 1;
    bool model_ready = false;
    uint32_t last_report_ms = 0;
    bool led_on = false;
};

static void InitHardware() {
    seed.Init();
    seed.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    seed.SetAudioBlockSize(NamAudio::kBlockSize);
    seed.StartLog(false);
    seed.usb_handle.SetReceiveCallback(OnUsbReceive, daisy::UsbHandle::FS_INTERNAL);
}

static void ApplyPendingAmpCommand(AppState &state) {
    const uint32_t command = requested_amp.load(std::memory_order_relaxed);
    if (command == state.current_command)
        return;

    // Stop DMA before changing the model or its state. Construction and
    // prewarming may allocate and take longer than an audio block.
    seed.StopAudio();
    bypass = command == 0;
    if (!bypass) {
        state.selected_amp = static_cast<AmpId>(command);
        state.model_ready = audio.LoadAmpModel(state.selected_amp);
    }
    state.current_command = command;
    seed.StartAudio(AudioCallback);
}

static void ReportStatus(AppState &state) {
    const uint32_t now_ms = daisy::System::GetNow();
    if (now_ms - state.last_report_ms < 1000)
        return;

    state.last_report_ms = now_ms;
    state.led_on = !state.led_on;
    seed.SetLed(state.led_on);
    seed.PrintLine("NAM A2-Lite: amp=%s ready=%u bypass=%u callbacks=%lu "
                   "max_us=%lu overruns=%lu | 0=clean 1=Twin65 2=AC30 3=JCM800",
                   AmpName(state.selected_amp), static_cast<unsigned>(state.model_ready), static_cast<unsigned>(bypass || !state.model_ready), static_cast<unsigned long>(audio_callbacks.exchange(0)), static_cast<unsigned long>(max_callback_us.exchange(0)), static_cast<unsigned long>(overruns.exchange(0)));
}

int main() {
    InitHardware();

    AppState state;
    state.model_ready = audio.LoadAmpModel(state.selected_amp);
    seed.StartAudio(AudioCallback);
    state.last_report_ms = daisy::System::GetNow();

    while (true) {
        ApplyPendingAmpCommand(state);
        ReportStatus(state);
        seed.DelayMs(1);
    }
}
