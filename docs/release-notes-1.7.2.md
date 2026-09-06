# INKademic v1.7.2

Stable promotion of the 1.7.1 release-candidate work for X3, X4, X4 Pro, and
Sticky, with the academic notes and annotation features enabled in every
device build.

## Highlights

- Academic notes, highlights, clippings, bookmarks, and annotation tags across
  all supported device builds.
- X4 Pro frontlight persistence, side-key navigation, USB recovery, wake
  optimization, and the INKademic boot logo.
- Quick resume, idle power saving, atomic settings writes, reduced SD-card
  writes, watchdog protection, post-failure diagnostics, and bootloader
  rollback support.
- Notes Connect for opening the current book's academic notes from the device.
- Browser-based firmware staging through the existing file-transfer interface,
  with image structure, chip, bounds, checksum, size, and SHA-256 validation.
- Nearby EPUB transfers now invalidate derived chapter and image caches after a
  replacement, so an existing book with annotations cannot leave incompatible
  cached data attached to the received ZIP.
- Image decoding keeps the temporary pixel-cache band in X4 Pro PSRAM when
  available and writes cache rows in larger batches, reducing low-memory
  fallbacks and slow repeated decodes on image-heavy EPUBs.
- OTA checks and downloads now use bounded network setup time and service the
  task watchdog; EPUB parsing and large file-index builds also yield regularly,
  preventing the X4 Pro reset seen while checking updates or indexing dense
  books.
- The X4 Pro application target now uses the factory-compatible 16 MB A/B
  partition map, avoiding the smaller X3/X4 app-slot assumptions.
- OTA manifest correction to the canonical repository endpoint:
  `https://api.github.com/repos/jbfarias/INKademic/releases/latest`.

## Firmware files

| Device | File | Size | SHA-256 |
|---|---|---:|---|
| X3 / X4 | `firmware-x3-x4-v1.7.2.bin` | 6,221,520 | `d5ab22d04fa4d81b5985a170bd521aeb2c691d74a9a4cf54e5026243e0a31609` |
| X4 Pro | `firmware-x4-pro-v1.7.2.bin` | 6,126,256 | `9b5237f16b51ebca778f2b72663a26987b3e59ea4f0d4410bf1077d432ac6a50` |
| Sticky | `firmware-sticky-v1.7.2.bin` | 6,018,272 | `1009d39c56b15612f2ecd36207333f74b567477a4e5ad289b4b356b121f0ff96` |
| X4 Pro recovery | `firmware-recovery-x4-pro-v1.7.2.bin` | 386,384 | `b4413945b6376f84b3da51069d08f0ca0f26c61dbe63edcc887f1118e0715edf` |

The recovery image is only for the documented X4 Pro factory-compatible
recovery path. Verify the published SHA-256 values before flashing.

## Important X4 Pro note

The simulator complements, but does not replace, physical validation. For an
X4 Pro installation, keep the recovery procedure available and do not power
off the reader during an image write. The A/B bootloader rollback remains
responsible for returning to the previous application slot if the new image
does not complete its first boot.

## Fork lineage

INKademic remains an independent academic fork based on CrossPoint Reader and
incorporates selected ideas and compatibility work from CrossInk, YACP,
CrossNotes, and BookOrbit. The complete attribution and integration notes are
in [Fork lineage and references](./fork-lineage.md).
