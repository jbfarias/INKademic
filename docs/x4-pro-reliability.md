# INKademic X4 Pro reliability and OTA

This page documents the reliability layer introduced for the X4 Pro and the
academic workflow shared by all INKademic targets.

## Reading and academic workflow

- Quick Resume keeps the last reader frame on the panel and lowers the idle
  CPU frequency after the inactivity threshold, so waking is fast without
  keeping the full reader pipeline active.
- The existing reading-pace samples provide WPM and estimated time remaining
  in the reader status bar and reading statistics.
- **Notes Connect** is available in the reader menu under the bookmarks tab.
  It starts the local hotspot and presents a QR code for
  `/highlights?path=<current-book>`. The browser opens the academic Notes page
  with the current EPUB selected, where notes/tags can be edited and exported.
- Configurable power, Home, side/front-button, chord, and Quick Action
  mappings remain shared across X3/X4, Sticky, and X4 Pro.

## Persistence, failure recovery, and battery

- Persistable JSON stores stream directly to a temporary file instead of
  constructing a second large `String`, call `sync()`, then atomically replace
  the active generation while retaining a `.bak` copy.
- Repeated saves with the same serialized snapshot are skipped, reducing SD
  wear. Overflowed JSON documents and incomplete writes are rejected.
- A 15-second main-task watchdog feeds on both sides of the cooperative loop.
  Interrupt, task, and general watchdog resets are written to
  `/crash_report.txt` with the reset reason and retained logs.
- Deep sleep stops SD/Wi-Fi users before power rails are released, saves the
  wake policy, and preserves the Quick Resume frame only when selected.

## Signed OTA and rollback

X4 Pro OTA accepts only a release containing both assets:

```text
firmware-x4-pro-<version>.bin
firmware-x4-pro-<version>.bin.sig
```

The `.sig` asset is 64 raw Ed25519 signature bytes over the firmware's raw
SHA-256 digest. The public key is compiled into
`include/OtaUpdatePublicKey.h`; the matching private key must stay in the
release secret store and must never be committed. Generate the signature with:

```sh
python3 scripts/sign_ota.py firmware-x4-pro-v<version>.bin \
  --key /secure/inkademic-ota-ed25519.pem
```

The release workflows expect the matching PEM in the
`INKADEMIC_OTA_ED25519_PRIVATE_KEY` Actions secret and publish the `.sig`
asset alongside the X4 Pro binary. If the signature is absent or invalid, the
X4 Pro refuses the OTA before selecting the new slot.

The bootloader has application rollback enabled. The new slot is marked
healthy only after the first complete boot and initial render; a panic or
watchdog before that point leaves the bootloader free to return to the
previous slot.

Unsigned legacy files can still be installed through the existing physical
USB/SD recovery path; they are intentionally not accepted by X4 Pro network
OTA.
