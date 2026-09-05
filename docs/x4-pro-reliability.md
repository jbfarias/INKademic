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

## Minimal SD recovery loader

The repository also provides a separate `recovery-x4-pro` PlatformIO target.
It intentionally omits the reader, UI, network, USB-MSC and persistent logging.
The resulting application only mounts the X4 Pro SDMMC card, opens exactly
`/inkademic-update.bin`, validates the complete ESP image, writes the inactive
factory-compatible OTA slot, updates `otadata`, and reboots.

Validation is not reduced: the loader checks the ESP image header and segment
boundaries, ESP32-S3 chip ID, XOR checksum, appended SHA-256 trailer, and the
destination partition limit before erasing flash. If any check or write fails,
it stays in the recovery loop and does not select the candidate.

Build it with:

```sh
pio run -e recovery-x4-pro
```

Copy the generated `firmware-recovery-x4-pro.bin` to the device only through a
known-good ROM/WebSerial or already-running INKademic recovery path. To use it,
place the full target firmware on the SD card as `/inkademic-update.bin` before
booting the loader. The loader uses the X4 Pro factory-compatible two-slot
partition table; do not flash it with the normal INKademic partition table.

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

## Local Wi-Fi OTA rescue server

The repository includes `scripts/ota_wifi_server.py`, a dependency-free local
server that exposes the GitHub-shaped release manifest consumed by
`OtaUpdater` and streams an OTA app image with its SHA-256 digest. It does not
write flash itself: the reader still performs the normal image, hash, optional
Ed25519 and A/B-slot checks.

Start it from the repository root with a build that is already available on the
computer:

```sh
python3 scripts/ota_wifi_server.py \
  --firmware .pio/build/x4-pro/firmware-x4-pro.bin \
  --version 1.7.1-rescue \
  --base-url http://<IP-do-Mac-na-rede>:8787
```

Check the server from another device at `/health`. The manifest is available at
`/repos/jbfarias/INKademic/releases/latest`, and the optional Unlocker catalog
is at `/api/catalog`.

### macOS hotspot setup with Xteink Unlocker

The Unlocker supplies a virtual network service; macOS supplies the actual
Wi-Fi hotspot. Start Unlocker first so that the **Xteink Unlocker** service is
present, then open **System Settings > General > Sharing** and configure:

1. **Share your connection from:** `Xteink Unlocker`.
2. Under **To devices using**, enable `Wi-Fi`.
3. Open **Wi-Fi Options** and choose a simple network name and an 8-character
   WPA password, for example `11111111`.
4. Enable **Internet Sharing** and approve the macOS administrator prompt.
5. Connect the X4 Pro to that Wi-Fi network and enter the password on the
   reader.

Use the Mac address on this shared Wi-Fi network as `--base-url`; do not use a
different home-LAN address. The Unlocker helper must remain running while the
reader checks for updates.

The device must be able to reach the announced `--base-url`. A build intended
to use the server must define `INKADEMIC_OTA_RELEASE_URL` to that manifest URL.
The X4 Pro RC build also has a narrowly scoped Unlocker compatibility retry:
normal GitHub TLS validation is attempted first, and only a failed request on
the Unlocker hotspot retries using the bridge certificate name while retaining
the CA bundle and firmware digest checks.

The currently installed older RC image does not contain this retry, so the
server and Unlocker cannot retrofit the fix into that binary. Use a working
bootstrap/recovery path to install this release once; later OTA checks can use
the corrected flow.

Do not expose this server to the public Internet. The firmware is selected by
the command line, and the server intentionally has no upload or arbitrary-file
endpoint.
