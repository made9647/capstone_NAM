# NAM Seed3

Neural amp modeling on the Daisy Seed3 using libDaisy, with three A2-Lite models:
Fender '65 Twin Reverb, Vox AC30 Chimey, and Marshall JCM800 (gain 5).

## Setup

On macOS, install [Homebrew](https://brew.sh) and Apple's Command Line Tools
(`xcode-select --install`). On Ubuntu, the installer uses `apt-get` and requests
`sudo` access when needed. Run from the repository root:

```sh
bash scripts/install.sh
bash scripts/download_models.sh
make
```

`make install` runs the same installer. It installs the ARM compiler, DFU uploader,
and `jq`, then fetches
pinned versions of libDaisy and NAM Core. The download script saves the three
Tone3000 models to Git-ignored `models/local/`. The build embeds their weights
in the firmware. On Ubuntu, the installer also installs the build and download
prerequisites, including the ARM C/C++ libraries and Python 3.

## Upload and play

Hold **BOOT**, press and release **RESET**, then release **BOOT** to enter DFU mode.

```sh
make upload
make monitor
```

The firmware starts with Fender selected. Press a key in the serial monitor
to switch models or bypass; no Enter is needed:

| Key | Selection |
| --- | --- |
| `0` | Clean bypass |
| `1` | Fender '65 Twin Reverb |
| `2` | Vox AC30 Chimey |
| `3` | Marshall JCM800 (gain 5) |

Audio runs at 48 kHz, with the left input processed and sent to both outputs.
Switching models or toggling bypass briefly interrupts playback. These are
amp-only models; cabinet filtering and hardware gain calibration are not implemented.

If multiple serial ports are connected, use `make monitor PORT=/dev/cu.usbmodem…`.
Exit the monitor with **Ctrl-A**, then **K**, then **Y**.

## Development

| Command | Purpose |
| --- | --- |
| `make` | Build firmware and embed all three models. |
| `make test` | Compare against upstream NAM and run sanitized host audio tests. |
| `make clean` | Remove build outputs; keep downloaded models and dependencies. |
| `make format` | Format C/C++ source in `src/` and `tests/`. |
| `make compiledb` | Generate the clangd compilation database. |
| `make help` | List commands. |

Formatting and clangd setup require `brew install clang-format compiledb`.
Host tests require a C++20 compiler.


## Prerequisites

### Everyone (works on any Linux, including WSL)
- `git`
- `python3`
- `jq`
- A C++ compiler — `sudo apt install build-essential` on Ubuntu/WSL. Host
  tests specifically need C++20 support (GCC 10+; modern Ubuntu ships 12+).
- On Ubuntu/WSL, also install `libdigest-sha-perl` — it provides the
  `shasum` command that `scripts/download_models.sh` uses (Ubuntu ships
  `sha256sum` by default, not `shasum`).

```sh
sudo apt update
sudo apt install -y build-essential git jq python3 libdigest-sha-perl
```

### Only if building/flashing real firmware
- ARM cross-compiler (`arm-none-eabi-g++`). On macOS, `make install` gets
  this via Homebrew automatically. On Ubuntu/WSL, Ubuntu's packaged version
  is often outdated or missing, so download it directly from
  [Arm's GNU toolchain page](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
  and add its `bin/` folder to your PATH.
- `dfu-util` — `sudo apt install dfu-util` on Ubuntu/WSL.
- If flashing from WSL specifically, you'll also need
  [usbipd-win](https://github.com/dorssel/usbipd-win) installed on the
  Windows side to pass the USB device through.

### Only if testing offline with `tools/render_wav.cpp` (no hardware needed)
- `ffmpeg` — `sudo apt install ffmpeg` — used to convert guitar recordings
  to the mono/16-bit/48kHz WAV format the tool expects.

### Fetched automatically, not manually downloaded
- **libDaisy** and **NeuralAmpModelerCore** — cloned at pinned commits by
  `scripts/install.sh` on macOS. On Ubuntu/WSL, clone them manually (see
  below) until the install script supports apt.
- **The three amp model captures** — fetched from Tone3000 by
  `scripts/download_models.sh`. Not stored in the repo (see `.gitignore`);
  Tone3000's license permits local use only, not redistribution.

### Manual dependency setup on Ubuntu/WSL (until install.sh supports apt)
```sh
mkdir -p libs
git clone --no-checkout https://github.com/electro-smith/libDaisy.git libs/libDaisy
git -C libs/libDaisy checkout --detach cc146d5065dd8286078a662e2830bf820c37a612
git -C libs/libDaisy submodule update --init --recursive

git clone --no-checkout https://github.com/sdatkinson/NeuralAmpModelerCore.git libs/NeuralAmpModelerCore
git -C libs/NeuralAmpModelerCore checkout --detach 2563c0fd4cb1f9ce457d89a761738ea15097e1f3
git -C libs/NeuralAmpModelerCore submodule update --init --recursive
```

## Testing tools (no hardware required)

`tools/` has small standalone programs for testing the amp models on a
computer, without needing the actual pedal. They all reuse the exact same
`NamAudio`/`NamProcessor`/`A2Lite` code that runs on the real firmware, so
what you hear from them is what the pedal will actually sound like.

| Tool | What it does | Where it runs |
| --- | --- | --- |
| `render_wav.cpp` | Reads a WAV file, runs it through one amp model, writes a new WAV file | Anywhere — Linux, WSL, or native Windows |
| `list_devices.cpp` | Lists every audio device your computer can see, with an index for each | Native Windows (MSYS2) only |
| `live_mic.cpp` | Streams live audio from a chosen input device, through an amp model, out to a chosen output device, in real time | Native Windows (MSYS2) only |

### `render_wav.cpp`

Uses the prerequisites already listed above (WSL/Linux included). Build
from the repo root, after the usual `download_models.sh` +
`convert_a2.py` steps:

```sh
g++ -std=c++17 -O2 -Ibuild/generated -Isrc \
  -Ilibs/NeuralAmpModelerCore -Ilibs/NeuralAmpModelerCore/NAM \
  -Ilibs/NeuralAmpModelerCore/Dependencies/eigen \
  -Ilibs/NeuralAmpModelerCore/Dependencies/nlohmann \
  -DNAM_SAMPLE_FLOAT -DNAM_USE_INLINE_GEMM \
  tools/render_wav.cpp src/audio/nam_audio.cpp src/models/amp_models.cpp \
  src/models/a2_lite.cpp src/audio/nam_processor.cpp \
  libs/NeuralAmpModelerCore/NAM/dsp.cpp -o render_wav
```

Usage:

```sh
ffmpeg -i yourfile.wav -ar 48000 -ac 1 -sample_fmt s16 solo_48k_mono.wav
./render_wav solo_48k_mono.wav fender_out.wav 1   # 1=Fender 2=Vox 3=Marshall
```

### `list_devices.cpp` and `live_mic.cpp` (native Windows only)

These need real, live access to a microphone and speakers, which WSL
cannot provide — it runs in a virtualized environment with no direct
hardware audio access (`list_devices` finds 0 devices under WSL). They
need a genuinely **native Windows build**, via **MSYS2**, so Windows can
hand over real audio device access the way it would to any other Windows
program.

**1. Install MSYS2** from [msys2.org](https://www.msys2.org/) (default
install location, `C:\msys64`, is fine).

**2. Open "MSYS2 MinGW64"** specifically from the Start Menu — not "MSYS2
MSYS" and not "MSYS2 UCRT64". This is the shell that produces real native
Windows `.exe` programs with full Windows API access, required for audio
device access to work.

**3. Update and install packages:**

```sh
pacman -Syu
```

(If it asks you to close and reopen the terminal partway through, do so,
then re-run `pacman -Syu` to finish.)

```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-portaudio git mingw-w64-x86_64-python mingw-w64-x86_64-jq perl
```

`mingw-w64-x86_64-portaudio` is the key addition here — PortAudio is the
cross-platform library that actually talks to the mic/speakers.

**4. Clone the repo into a native Windows path** (a real `C:\...` path,
not `\\wsl$\...`, so the build is genuinely native):

```sh
git clone <this repo's URL> /c/Users/<you>/projects/<folder-name>
cd /c/Users/<you>/projects/<folder-name>
```

**5. Repeat the NAM Core clone, model download, and header generation
steps** from the sections above, run again here — this native Windows
checkout needs its own copies, separate from any WSL/Linux one.

**6. Build `list_devices` and find your device indices:**

```sh
g++ -std=c++17 tools/list_devices.cpp -lportaudio -o list_devices.exe
./list_devices.exe
```

This lists every device, tagged with which Windows audio API exposes it
(MME, DirectSound, WASAPI, WDM-KS). Each physical device appears once per
API — **use the `Windows WASAPI` entries**, the modern, lowest-latency
option. Note the index numbers for your input device (mic/headset) and
output device (speakers/headphones).

**7. Build `live_mic`:**

```sh
g++ -std=c++17 -O2 -Ibuild/generated -Isrc \
  -Ilibs/NeuralAmpModelerCore -Ilibs/NeuralAmpModelerCore/NAM \
  -Ilibs/NeuralAmpModelerCore/Dependencies/eigen \
  -Ilibs/NeuralAmpModelerCore/Dependencies/nlohmann \
  -DNAM_SAMPLE_FLOAT -DNAM_USE_INLINE_GEMM \
  tools/live_mic.cpp src/audio/nam_audio.cpp src/models/amp_models.cpp \
  src/models/a2_lite.cpp src/audio/nam_processor.cpp \
  libs/NeuralAmpModelerCore/NAM/dsp.cpp -lportaudio -o live_mic.exe
```

**8. Run it:**

```sh
./live_mic.exe 1 15 13
```

Arguments: `amp_id input_device_index output_device_index`
(`amp_id`: `1`=Fender, `2`=Vox, `3`=Marshall). Replace `15`/`13` with your
own device indices from step 6.

Windows may prompt for microphone permission the first time — allow it.
Press **Enter** in the terminal to stop. To change amps, stop it and
re-run with a different `amp_id` (switching live isn't supported yet).

### Known limitations

- **Latency**: this goes through Windows' shared audio stack, not a
  low-latency ASIO path, so expect a noticeable delay (tens of
  milliseconds) — fine for testing tone, not tuned for comfortable
  real-time playing yet.
- **Signal quality depends on your input.** A built-in laptop mic or a
  headset mic picking up sound acoustically will sound noticeably worse
  than a real instrument-level input, due to background noise and the
  mic's own frequency response.
- **No live amp-switching** in `live_mic` yet — stop and restart with a
  different `amp_id` to change amps.