from datetime import datetime, timedelta, timezone
from pathlib import Path

import pytest
from cryptography import x509
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.x509.oid import NameOID

ROOT = Path(__file__).resolve().parents[1]
PKI = ROOT / "tools" / "test-pki"


def load(name):
    return x509.load_pem_x509_certificate((PKI / name).read_bytes())


def verify_issuer_signature(issuer, subject):
    issuer.public_key().verify(
        subject.signature,
        subject.tbs_certificate_bytes,
        padding.PKCS1v15(),
        subject.signature_hash_algorithm,
    )


def test_server_cert_is_signed_by_test_ca():
    verify_issuer_signature(load("ca.crt"), load("server.crt"))


def test_server_cert_has_localhost_san():
    san = load("server.crt").extensions.get_extension_for_class(
        x509.SubjectAlternativeName).value
    assert "localhost" in san.get_values_for_type(x509.DNSName)
    assert "127.0.0.1" in [
        str(ip) for ip in san.get_values_for_type(x509.IPAddress)]


def test_rogue_cert_not_signed_by_ca():
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "rogue.local")])
    now = datetime.now(timezone.utc)
    rogue = (x509.CertificateBuilder()
             .subject_name(name)
             .issuer_name(name)
             .public_key(key.public_key())
             .serial_number(x509.random_serial_number())
             .not_valid_before(now - timedelta(days=1))
             .not_valid_after(now + timedelta(days=1))
             .sign(key, hashes.SHA256()))

    with pytest.raises(InvalidSignature):
        verify_issuer_signature(load("ca.crt"), rogue)


def test_embedded_ca_matches_test_pki():
    embedded = (ROOT / "main" / "certs" / "server_ca.pem").read_bytes()
    assert embedded == (PKI / "ca.crt").read_bytes()
