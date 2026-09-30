# Threat model — ESP32 Secure OTA

Scope: an ESP32 device that receives firmware updates over the network from a
release server. Out of scope: physical de-capping, full side-channel lab
attacks, and supply-chain compromise of Espressif tools.

## Assets

| Asset | Why it matters |
|-------|----------------|
| Firmware image | Compromised code = full device control |
| Secure Boot signing key | Anyone with it can sign trusted firmware |
| Flash encryption key | Protects firmware/at-rest data confidentiality |
| Release manifest | Controls which version a fleet moves to |
| Device identity / provisioning data | Fleet integrity |

## Trust boundaries

```
 [ signing key holder ] --offline sign--> [ build machine ]
                                                |
                                                v
 [ release server ] <---TLS 1.2---> [ ESP32 ] <---flash--- [ attacker w/ physical access ]
```

1. **Build machine ↔ release server** — signed artifacts only.
2. **Release server ↔ device** — TLS with pinned/embedded CA.
3. **Device ↔ flash** — flash encryption + signature verification.
4. **Device ↔ field attacker with a cable** — eFuse read/write protection,
   disabled ROM download mode (optional).

## STRIDE analysis

| Threat | STRIDE | Vector | Mitigation | Residual risk |
|--------|--------|--------|-----------|---------------|
| Malicious firmware flash | Spoofing | Write a rogue image to flash | Secure Boot v2 rejects unsigned/wrong-key image | Needs valid signature |
| Image modification in transit | Tampering | MITM alters firmware | HTTPS + signed image; bootloader re-verifies | TLS trust must hold |
| Flash readout | Info disclosure | Dump flash chips | AES-256 flash encryption | Dev mode leaks if key accessible |
| Downgrade to vulnerable build | Tampering | Serve old version | Anti-rollback (`secure_version` in eFuse) | 32 version steps max |
| Rogue update server | Spoofing | Present a different cert | Embedded CA + TLS verification | CA key must stay secure |
| Replay of a valid old image | Spoofing | Re-serve captured firmware | Version gating + anti-rollback | same as downgrade |
| Bad update bricks device | DoS | Ship crashing firmware | Rollback after failed self-test | Self-test must be meaningful |
| Bypass via debug port | Elevation | Use UART/JTAG to reflash | eFuse disables ROM download / JTAG when provisioning | Must be explicitly enabled |
| Recover signing key from device | Info disclosure | Read key from chip | Public key only is stored; private key stays offline | None from device |

## Attack / response matrix

| Attack | Detection | Response |
|--------|-----------|----------|
| Unsigned image on OTA slot | Bootloader signature check | Refuse boot; fall back to other slot |
| Wrong-key signature | Public-key digest mismatch in eFuse | Refuse boot |
| Tampered manifest/firmware at rest | SHA-256 mismatch (host tooling) | Reject release |
| Untrusted TLS certificate | mbedTLS verification against embedded CA | Abort download |
| Crash before confirmation | Bootloader sees `PENDING_VERIFY` | Roll back to previous image |

## Explicit non-goals

- Protecting against an attacker who obtains the offline signing key.
- Hardware side-channel / fault-injection resistance on silicon without Secure
  Boot v2.
- Confidentiality of data the application deliberately publishes.
