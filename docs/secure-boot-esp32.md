# Secure Boot on the classic ESP32 — notes and procedure

This document records the chip-specific facts and the exact, ordered procedure
for enabling Secure Boot on an original ESP32 board. Sources: Espressif ESP-IDF
Secure Boot v2 and Flash Encryption guides (target `esp32`).

## Which Secure Boot version does your chip support?

| ESP32 chip revision | Secure Boot | Scheme | eFuse |
|---------------------|-------------|--------|-------|
| **v3.0 (ECO3) and newer** | **v2** | RSA-3072 / RSA-PSS | `ABS_DONE_1`; key digest in `BLK2` |
| earlier revisions | v1 | RSA-2048 (digest scheme) | different eFuse layout |

Check the revision:

```bash
esptool.py chip_id
# e.g. "Chip is ESP32-D0WD-V3 (revision v3.1)"
```

- `v3.x` → use this repo's `sdkconfig.secure.defaults` (Secure Boot v2).
- `v1.x` / `v2.x` → Secure Boot v2 options will not be available. Use Secure
  Boot v1; the OTA client, manifest and rollback logic here are unaffected, but
  the signing/eFuse steps differ. Do not enable v2 config on old silicon.

## Why v2 and v1 differ

Secure Boot v2 verifies both the second-stage bootloader and the app with an
RSA-PSS signature block appended to each image, and stores only a digest of the
RSA-3072 public key in eFuse. There is exactly one signature block on ESP32
v3.0. A leak of the private key is the only way to forge firmware; no secret
lives on the device, so it resists passive side-channel attacks.

## Procedure (Secure Boot v2)

> Irreversible. eFuse bits cannot be un-burned. Prefer a spare board for the
> first attempt, and back up the signing key offline.

```bash
# 1. Generate the RSA-3072 signing key (once, offline)
./tools/gen_keys.sh
# or, equivalently:
# idf.py secure-generate-signing-key --keyfile secure_boot_signing_key.pem

# 2. Build with Secure Boot v2 + flash encryption enabled
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.secure.defaults" build

# 3. Build the signed bootloader and flash it
idf.py bootloader
idf.py -p <PORT> bootloader-flash

# 4. Flash the (signed) partition table + app
idf.py -p <PORT> flash

# 5. First boot enables Secure Boot in hardware; watch the console
idf.py -p <PORT> monitor
```

During the first boot the bootloader writes the public-key digest to eFuse and
sets `ABS_DONE_1`. If enabled, flash encryption is provisioned at the same
time (do **not** cut power during that pass).

After provisioning, any updated bootloader or app must be signed with the same
key. `idf.py flash` no longer flashes the bootloader.

## Flash encryption (AES-256)

- Classic ESP32 uses **AES-256** with address tweaking — not AES-XTS.
- The secure config uses **development mode** so you can keep re-flashing.
  Development mode still allows decrypting flash via the UART bootloader, so it
  is not a production posture.
- **Release mode** burns `DISABLE_DL_ENCRYPT` / `DISABLE_DL_DECRYPT` and
  write-protects `FLASH_CRYPT_CNT`; from then on, new plaintext is only written
  through the OTA path.
- To move an already-encrypted device to release mode, call
  `esp_flash_encryption_set_release_mode()` once (see the Espressif
  `security/security_features_app` example).

## Anti-rollback

Enabled by `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK`. The bootloader refuses to boot
an image whose `secure_version` is lower than the value burned in eFuse.
Anti-rollback requires **application-controlled confirmation** — automatic
confirmation is unavailable — which is why `app_main.c` calls
`esp_ota_mark_app_valid_cancel_rollback()` only after its self-test passes.

Notes:
- The `secure_version` field is limited (32 bits by default) → at most 32
  rollback steps unless reduced.
- Anti-rollback requires eFuse coding scheme `NONE`.
- Factory/test partitions are not supported with anti-rollback, which is why
  `partitions.csv` has no `factory` partition.

## Rollback (recovery from a bad update)

With `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, a freshly OTA'd image boots as
`ESP_OTA_IMG_PENDING_VERIFY`. The app must either:

- confirm: `esp_ota_mark_app_valid_cancel_rollback()`, or
- reject: `esp_ota_mark_app_invalid_rollback_and_reboot()`.

If the device resets before confirmation, the bootloader rolls back on the next
boot.

## Restrictions after provisioning

- USB-OTG / serial DFU is disabled in ROM once Secure Boot or flash encryption
  is on.
- eFuse keys can no longer be read-protected after Secure Boot is enabled
  (unless `CONFIG_SECURE_BOOT_V2_ALLOW_EFUSE_RD_DIS` was set beforehand).
- JTAG debugging is disabled by eFuse on first boot.
