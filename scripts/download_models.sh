#!/bin/bash
# Download the three required amp captures and extract their existing A2-Lite models.
# T3K: local use permitted; redistribution requires author permission.
set -euo pipefail

cd "$(dirname "$0")/.."
for tool in curl jq shasum; do
    command -v "$tool" >/dev/null || { echo "Missing dependency: $tool" >&2; exit 1; }
done

output_dir="$PWD/models/local"
mkdir -p "$output_dir"
staging=$(mktemp -d "$output_dir/.download.XXXXXX")
trap 'rm -rf "$staging"' EXIT
base_url=https://api.tone3000.com/storage/v1/object/public/models

download_lite() {
    local name=$1 remote=$2 expected_sha=$3 actual_sha
    echo "Downloading $name..."
    curl --fail --silent --show-error --location \
        --connect-timeout 15 --max-time 60 \
        "$base_url/$remote" -o "$staging/source.nam"
    actual_sha=$(shasum -a 256 "$staging/source.nam")
    if [[ ${actual_sha%% *} != "$expected_sha" ]]; then
        echo "$name: upstream checksum changed" >&2
        exit 1
    fi

    # Preserve Lite weights/configuration and inherit container calibration.
    jq -ce '
        . as $container
        | if .architecture != "SlimmableContainer" then
            error("expected an A2 SlimmableContainer")
          else . end
        | [.config.submodels[] | select(.max_value == 0.5) | .model]
        | if length != 1 then error("expected one Lite submodel") else .[0] end
        | .sample_rate = (.sample_rate // $container.sample_rate)
        | .metadata = (($container.metadata // {}) + (.metadata // {}))
        | if .architecture == "WaveNet" and .sample_rate == 48000
             and (.weights | length) == 1871
          then . else error("unexpected Lite model format") end
    ' "$staging/source.nam" > "$staging/$name"
}

download_lite fender-twin65-a2-lite.nam 2zbg20dlaqu_a2.nam \
    98b69400de5d674a7b6bdc6c267b2cb82a250ba99829fa42a7d8b02f9224985a
download_lite vox-ac30-chimey-a2-lite.nam 6e16b996ad4ea65b_a2.nam \
    7c25d76cb0cbe2086197dcf11fdf7a21aaf4deffea413d8558d00bb28bdc39fc
download_lite marshall-jcm800-g5-a2-lite.nam 11fc0db6d4cf7849_a2.nam \
    d9454552f64de2bad497b0e25025010d3c1e770d0b44790c060d0428d29ec772

# Replace existing models only after all downloads and extraction succeed.
for name in fender-twin65-a2-lite.nam vox-ac30-chimey-a2-lite.nam marshall-jcm800-g5-a2-lite.nam; do
    mv "$staging/$name" "$output_dir/$name"
    echo "Saved $output_dir/$name"
done
