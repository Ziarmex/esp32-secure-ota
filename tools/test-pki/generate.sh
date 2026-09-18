#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Generate a throwaway CA + localhost server certificate for the OTA demo and
# copy the CA into main/certs/ so the firmware trusts the local server.
#
# TEST ONLY - never ship these keys.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt \
    -subj "/CN=ESP32-Secure-OTA TEST CA (DO NOT TRUST)/O=Test Only"

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
    -subj "/CN=localhost/O=Test Only"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out server.crt -days 3650 -sha256 -extfile server.ext

rm -f server.csr ca.srl
cp ca.crt ../../main/certs/server_ca.pem

echo "Test PKI generated in $DIR"
echo "Embedded CA copied to main/certs/server_ca.pem"
