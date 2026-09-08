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
