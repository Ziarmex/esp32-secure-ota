import json

import pytest

from manifest import (
    REQUIRED_FIELDS,
    build_manifest,
    is_newer,
    parse_version,
    verify_manifest,
    write_manifest,
)


@pytest.fixture()
def firmware(tmp_path):
    image = tmp_path / "firmware.bin"
    image.write_bytes(b"\xE9" + bytes(range(256)) * 16)
    return image


def test_build_manifest_fields(firmware):
    manifest = build_manifest(firmware, "1.2.3")
    assert set(REQUIRED_FIELDS).issubset(manifest)
    assert manifest["version"] == "1.2.3"
    assert manifest["firmware"] == "firmware.bin"
    assert manifest["size"] == firmware.stat().st_size
    assert len(manifest["sha256"]) == 64


def test_manifest_roundtrip(firmware, tmp_path):
    out = tmp_path / "manifest.json"
    write_manifest(firmware, "1.0.0", out)
    verify_manifest(out, firmware)  # must not raise


def test_tampered_firmware_detected(firmware, tmp_path):
    out = tmp_path / "manifest.json"
    write_manifest(firmware, "1.0.0", out)
    firmware.write_bytes(firmware.read_bytes() + b"\x00")
    with pytest.raises(ValueError, match="size mismatch"):
        verify_manifest(out, firmware)


def test_bitflip_same_size_detected(firmware, tmp_path):
    out = tmp_path / "manifest.json"
    write_manifest(firmware, "1.0.0", out)
    data = bytearray(firmware.read_bytes())
    data[10] ^= 0xFF
    firmware.write_bytes(bytes(data))
    with pytest.raises(ValueError, match="sha256 mismatch"):
        verify_manifest(out, firmware)


def test_missing_field_rejected(firmware, tmp_path):
    out = tmp_path / "manifest.json"
    manifest = build_manifest(firmware, "1.0.0")
    del manifest["sha256"]
    out.write_text(json.dumps(manifest))
    with pytest.raises(ValueError, match="missing field"):
        verify_manifest(out, firmware)


def test_version_ordering():
    assert parse_version("1.2.10") == (1, 2, 10)
    assert is_newer("1.2.0", "1.1.9")
    assert not is_newer("1.1.0", "1.1.0")
    assert not is_newer("1.0.9", "1.1.0")
