#!/usr/bin/env python3
"""Release manifest helpers for esp32-secure-ota.

The manifest is a small JSON document published next to the firmware image:

    {
      "version":  "1.1.0",
      "firmware": "firmware.bin",
      "size":     1048576,
      "sha256":   "<hex digest>"
    }

The device uses ``version`` for gating and downloads ``firmware``. Image
authenticity is enforced by Secure Boot v2 on the chip; the digest here is a
defence-in-depth check for the *server* side and for host tooling.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

REQUIRED_FIELDS = ("version", "firmware", "size", "sha256")


def sha256_file(path: str | Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_manifest(firmware_path: str | Path, version: str,
                   firmware_name: str | None = None) -> dict:
    firmware_path = Path(firmware_path)
    if not firmware_path.is_file():
        raise FileNotFoundError(f"firmware image not found: {firmware_path}")
    return {
        "version": version,
        "firmware": firmware_name or firmware_path.name,
        "size": firmware_path.stat().st_size,
        "sha256": sha256_file(firmware_path),
    }


def write_manifest(firmware_path: str | Path, version: str,
                   out_path: str | Path,
                   firmware_name: str | None = None) -> dict:
    manifest = build_manifest(firmware_path, version, firmware_name)
    Path(out_path).write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def verify_manifest(manifest_path: str | Path,
                    firmware_path: str | Path) -> None:
    """Raise ValueError when the firmware does not match the manifest."""
    manifest = json.loads(Path(manifest_path).read_text())
    for field in REQUIRED_FIELDS:
        if field not in manifest:
            raise ValueError(f"manifest missing field: {field}")

    firmware_path = Path(firmware_path)
    actual_size = firmware_path.stat().st_size
    if actual_size != manifest["size"]:
        raise ValueError(
            f"size mismatch: manifest={manifest['size']} actual={actual_size}")

    actual_sha = sha256_file(firmware_path)
    if actual_sha != manifest["sha256"]:
        raise ValueError(
            f"sha256 mismatch: manifest={manifest['sha256']} actual={actual_sha}")


def parse_version(text: str) -> tuple[int, ...]:
    return tuple(int(part) for part in str(text).split("."))


def is_newer(candidate: str, current: str) -> bool:
    """Semantics identical to the firmware's compare_versions()."""
    return parse_version(candidate) > parse_version(current)
