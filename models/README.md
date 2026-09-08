# Amp models

`local/` holds the three downloaded A2-Lite captures used by the firmware:

- `fender-twin65-a2-lite.nam`
- `vox-ac30-chimey-a2-lite.nam`
- `marshall-jcm800-g5-a2-lite.nam`

Run `bash scripts/download_models.sh` from the repository root to obtain them.
These files are Git-ignored. Their T3K licenses permit local use but require
author permission to redistribute the models or firmware containing their weights.

`scripts/convert_a2.py` validates the supported 48 kHz architecture and packs
all three models into `build/generated/embedded_a2_data.h`. Make regenerates
that header when an input model or the converter changes, or the header is missing.
