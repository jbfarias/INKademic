# INKademic v1.7.0-rc.2

Release candidate dated 5 September 2026.

## Included firmware

| Device | File | Size | SHA-256 |
| --- | --- | ---: | --- |
| X3 / X4 | `firmware-x3-x4-v1.7.0-rc.2.bin` | 6,322,608 bytes | `973e41ce47aa37e1b118ef536eae07044f442592177df76962c4ea810caa4c2a` |
| X4 Pro | `firmware-x4-pro-v1.7.0-rc.2.bin` | 6,216,688 bytes | `577d442f3b56484d74b20a151f285df83619e4fde29896c2335355a4995689b5` |
| Sticky | `firmware-sticky-v1.7.0-rc.2.bin` | 6,117,904 bytes | `189e66bf2c8f0f991f3309f262def7f75d972b8732680e8a325de6a1cc9b5bc0` |

## Included improvements

- Academic notes, highlights, clippings, bookmarks, and annotation tags are enabled in every device build.
- The X4 Pro uses the same INKademic academic workflow as X3/X4 and Sticky, including touch navigation, persistence, frontlight behavior, and the USB identity.
- Added date-aware academic filters and Markdown, JSON, and CSV exports with document identity, chapter, quote, stable layout metadata, and note context.
- Preserved touch navigation for footnotes and academic references, including table clippings and return-to-origin behavior.
- Avoided repeated recent-book and automatic statistics writes when their data is unchanged.
- Kept note writes durable and rejected oversized or newer unreadable note data instead of replacing it with an empty file.
- Added Notes Connect from the reader, with the current book preselected in the academic web workspace.
- Added streaming atomic JSON persistence, no-op write detection, watchdog recovery, reset-cause diagnostics, and bounded JSON handling to reduce SD wear and OOM risk.
- Added OTA signature metadata and X4 Pro Ed25519 verification over the SHA-256 digest, with signed-image enforcement and A/B rollback support.
- Preserved quick resume, sleep power savings, reading-time/WPM indicators, configurable shortcuts, and safe shutdown across all builds.
- Separated pioarduino SDK cache decisions by chip and SDK configuration.
- Stopped the SD backend before deep sleep, preserved the X4 Pro frontlight policy across wake and Quick Lock, and hardened compressed EPUB reads against invalid or oversized storage results.
- Applied the new INKademic logo in the web companion pages and firmware web portal.

## Forks and references

The integration was based on CrossPoint Reader, the CrossInk academic work, and the FreeInk SDK. Compatible ideas were reviewed from [CrossXT](https://github.com/yazdipour/CrossXT), [CrossInk-Bookorbit](https://github.com/agosez/CrossInk-Bookorbit), [CrossNotes](https://github.com/HilbergK/CrossNotes), and [YACP](https://github.com/Sichroteph/YACP/f). The complete attribution and compatibility notes are in [fork lineage and references](./fork-lineage.md).

These projects are references for selected features and compatibility decisions; they are not wholesale runtime dependencies of INKademic.

## Validation

- X3/X4, Sticky, and X4 Pro production builds completed successfully with OTA-size checks.
- Simulator builds for X3/X4 and X4 Pro completed successfully and reported `INKademic version 1.7.0-rc.2`.
- The X4 Pro production image uses 94.9% of the OTA application partition, leaving 336,912 bytes free.
- The default image leaves 230,992 bytes in its OTA application partition; Sticky leaves 435,696 bytes.
- X4 Pro and X3/X4 simulator builds completed successfully; the X4 Pro smoke test passed with three page turns.
- Host-side syntax, signing-script help, and whitespace validation passed for the build and release changes.

The X4 Pro release also includes `firmware-x4-pro-v1.7.0-rc.2.bin.sig`, a 64-byte Ed25519 signature over the firmware SHA-256 digest. The GitHub Actions secret `INKADEMIC_OTA_ED25519_PRIVATE_KEY` must contain the matching PEM key for future automated releases.

This is a release candidate. Physical X4 Pro validation should be completed using [the validation checklist](x4-pro-validation.md) before promoting it to a stable release.
