#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build a signed release image and emit release/manifest.json.
#
# Requires: ESP-IDF exported in the environment, plus an existing
# secure_boot_signing_key.pem (see tools/gen_keys.sh).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -f secure_boot_signing_key.pem ]]; then
    echo "Missing secure_boot_signing_key.pem - run tools/gen_keys.sh first." >&2
    exit 1
fi

VERSION="$(tr -d '[:space:]' < version.txt)"
APP="build/esp32-secure-ota.bin"

idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.secure.defaults" build

mkdir -p release
cp "$APP" release/firmware.bin

python3 - "$VERSION" <<'PY'
import sys
from tools.manifest import write_manifest

version = sys.argv[1]
manifest = write_manifest("release/firmware.bin", version, "release/manifest.json")
print(f"release/manifest.json -> version={manifest['version']} "
      f"size={manifest['size']} sha256={manifest['sha256'][:16]}...")
PY

echo "Release ready in ./release"
