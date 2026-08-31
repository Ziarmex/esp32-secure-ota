#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Generate an RSA-3072 Secure Boot v2 signing key.
#
# KEEP THIS FILE SECRET. Anyone who holds it can sign firmware your devices
# will trust. It must never be committed to git.
set -euo pipefail

KEY="${1:-secure_boot_signing_key.pem}"

if [[ -e "$KEY" ]]; then
    echo "Refusing to overwrite existing key: $KEY" >&2
    exit 1
fi

openssl genrsa -out "$KEY" 3072
chmod 600 "$KEY"

echo "Generated RSA-3072 Secure Boot v2 signing key: $KEY"
echo "Back it up offline and never commit it."
