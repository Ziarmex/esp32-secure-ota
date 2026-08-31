# Embedded TLS trust anchor

`server_ca.pem` is embedded into the firmware (via `EMBED_TXTFILES`) and used as
the only CA the device will trust for HTTPS manifest/image downloads.

- The committed file comes from `tools/test-pki/generate.sh` and is a
  **TEST-ONLY** localhost CA.
- For anything real, replace it with the CA that signs your production server
  certificate (and prefer a proper certificate chain / public CA).
- Regenerate the test CA with:

  ```bash
  ./tools/test-pki/generate.sh
  ```
