# INKademic lineage and fork references

INKademic is an independent academic fork. This page records the projects that
form its foundation or informed its compatibility work, so contributors can
trace ideas and upstream attribution without confusing them with bundled
dependencies.

## Direct foundations

- [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
  provides the main reader, activity, networking, storage, and device-firmware
  architecture.
- [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) provides the shared
  display, input, storage, battery, UI, and board-support layers. INKademic
  currently pins the SDK as a submodule so X3/X4, X4 Pro, and Sticky builds use
  a reproducible hardware base.
- [CrossInk](https://github.com/uxjulia/CrossInk) is the principal fork
  reference for the typography, reading statistics, web companion, simulator
  workflow, and several reader refinements carried into the academic fork.

## Forks consulted during the academic integration

- [CrossXT](https://github.com/yazdipour/CrossXT) was reviewed for device and
  firmware-variant compatibility patterns.
- [CrossInk-Bookorbit](https://github.com/agosez/CrossInk-Bookorbit) was
  reviewed for synchronization and library-integration ideas. BookOrbit is not
  a runtime dependency of INKademic.
- [CrossNotes](https://github.com/HilbergK/CrossNotes) informed note and
  annotation compatibility checks. INKademic keeps its own document-safe
  stores and web export format while reading compatible legacy note data where
  supported.
- [YACP](https://github.com/Sichroteph/YACP/f) was reviewed as an additional
  reference for reader behavior and project-level improvements.

These projects remain separately licensed and maintained. INKademic does not
vendor their repositories wholesale: changes are adapted to the CrossPoint /
FreeInk architecture, tested across the supported device profiles, and
documented in this repository when they affect user-visible behavior.

## Attribution and compatibility

The CrossPoint Reader and CrossInk projects retain their original attribution.
Names of persisted files, export schemas, upstream services, and SDK headers
are kept where changing them would break existing books, annotations, device
settings, or builds. The product and user-facing project name is **INKademic**.
