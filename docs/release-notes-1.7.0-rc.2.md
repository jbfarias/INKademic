# INKademic v1.7.0-rc.2

Release candidate dated 5 September 2026.

## Included firmware

| Device | File | Size | SHA-256 |
| --- | --- | ---: | --- |
| X3 / X4 | `firmware-x3-x4-v1.7.0-rc.2.bin` | 6,318,128 bytes | `f0a412a16a375ba5edc116a0f8655bdd4a8a180d180ca2f87184372f4e8eb0d3` |
| X4 Pro | `firmware-x4-pro-v1.7.0-rc.2.bin` | 6,211,552 bytes | `58c4fe7ff8c40487717bb1a87cfbffcb85e3afd50be6159e3d65b6c11e8e1e7f` |
| Sticky | `firmware-sticky-v1.7.0-rc.2.bin` | 6,113,664 bytes | `e74ebece2593f7a37e63083d752d18c2a03cf765067c3f88dca89660cca03b5c` |

## Included improvements

- Academic notes, highlights, clippings, bookmarks, and annotation tags are enabled in every device build.
- The X4 Pro uses the same INKademic academic workflow as X3/X4 and Sticky, including touch navigation, persistence, frontlight behavior, and the USB identity.
- Added date-aware academic filters and Markdown, JSON, and CSV exports with document identity, chapter, quote, stable layout metadata, and note context.
- Preserved touch navigation for footnotes and academic references, including table clippings and return-to-origin behavior.
- Avoided repeated recent-book and automatic statistics writes when their data is unchanged.
- Kept note writes durable and rejected oversized or newer unreadable note data instead of replacing it with an empty file.
- Separated pioarduino SDK cache decisions by chip and SDK configuration.
- Stopped the SD backend before deep sleep, preserved the X4 Pro frontlight policy across wake and Quick Lock, and hardened compressed EPUB reads against invalid or oversized storage results.
- Applied the new INKademic logo in the web companion pages and firmware web portal.

## Forks and references

The integration was based on CrossPoint Reader, the CrossInk academic work, and the FreeInk SDK. Compatible ideas were reviewed from [CrossXT](https://github.com/yazdipour/CrossXT), [CrossInk-Bookorbit](https://github.com/agosez/CrossInk-Bookorbit), [CrossNotes](https://github.com/HilbergK/CrossNotes), and [YACP](https://github.com/Sichroteph/YACP/f). The complete attribution and compatibility notes are in [fork lineage and references](./fork-lineage.md).

These projects are references for selected features and compatibility decisions; they are not wholesale runtime dependencies of INKademic.

## Validation

- X3/X4, Sticky, and X4 Pro production builds completed successfully with OTA-size checks.
- Simulator builds for X3/X4 and X4 Pro completed successfully and reported `INKademic version 1.7.0-rc.2`.
- The X4 Pro production image uses 94.8% of the OTA application partition, leaving 342,048 bytes free.
- Host-side syntax and Python validation passed for the build, release, and simulator helper scripts.

This is a release candidate. Physical X4 Pro validation should be completed using [the validation checklist](x4-pro-validation.md) before promoting it to a stable release.
