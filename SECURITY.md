# Security Policy

## Supported configuration

This is a reference project, not a product. The security-relevant configuration
is the opt-in one in `sdkconfig.secure.defaults` (Secure Boot v2, flash
encryption, anti-rollback). The default build in `sdkconfig.defaults`
intentionally does **not** enable any of that, so it can never brick a board or
mislead you into thinking a device is provisioned.

## Reporting a vulnerability

Open a private security advisory on GitHub, or contact the maintainer directly.
Please include the affected file, the configuration, and a minimal reproduction.

## Security properties (and what provides them)

| Property | Mechanism | Where |
|----------|-----------|-------|
| Only trusted code runs | Secure Boot v2 RSA-3072 / RSA-PSS signature verified by ROM + bootloader | eFuse public-key digest |
| Flash readout does not disclose plaintext | AES-256 flash encryption | `flash_encryption` eFuse key |
| Update authenticity | Signed OTA image validated with `esp_ota_end()` / bootloader | Secure Boot key |
| No downgrade to a vulnerable version | Anti-rollback (`secure_version` in eFuse) | `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK` |
| Recovery from a bad update | Bootloader rollback; app confirms after self-test | `esp_ota_mark_app_valid_cancel_rollback()` |
| Server authenticity | TLS with an embedded CA | `main/certs/server_ca.pem` |

## Test PKI warning

`tools/test-pki/` contains a CA and localhost server key that are **committed on
purpose** so the demo runs out of the box. They are labelled TEST ONLY and must
never be used outside a local bench. The real Secure Boot signing key
(`secure_boot_signing_key.pem`) is git-ignored and must stay offline.

## Threat model

The full threat model — assets, trust boundaries, STRIDE analysis and the
attack/response matrix — lives in [`docs/threat-model.md`](docs/threat-model.md).

Known gaps are listed under "Limitations" in the README and in the threat
model. There is no fault-injection or side-channel resistance claim for a
classic ESP32 without Secure Boot v2 hardware support.
