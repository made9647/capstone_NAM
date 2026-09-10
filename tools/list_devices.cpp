// Lists every audio device PortAudio can see, with its index, name, and
// channel counts. Run this after plugging your guitar cable into the
// headphone/mic jack, so the list reflects what's currently plugged in.
//
// Build (MSYS2 MinGW64 shell, from the repo root):
//   g++ -std=c++17 tools/list_devices.cpp -lportaudio -o list_devices.exe
//
// Run:
//   ./list_devices.exe

#include <portaudio.h>
#include <cstdio>

int main() {
    Pa_Initialize();
    const int count = Pa_GetDeviceCount();
    const int default_in = Pa_GetDefaultInputDevice();
    const int default_out = Pa_GetDefaultOutputDevice();
    printf("Found %d audio device(s):\n\n", count);
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo *api = Pa_GetHostApiInfo(info->hostApi);
        printf("[%d] (%s) %s  (in: %d ch, out: %d ch)%s%s\n",
               i, api->name, info->name, info->maxInputChannels, info->maxOutputChannels,
               i == default_in ? "  <- default input" : "",
               i == default_out ? "  <- default output" : "");
    }
    Pa_Terminate();
    return 0;
}