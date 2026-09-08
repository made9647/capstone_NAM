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
