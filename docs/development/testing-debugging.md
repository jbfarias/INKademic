---
title: Testing & Debugging
parent: Development
nav_order: 4
---

# Testing and Debugging

CrossInk runs on real hardware, so debugging usually combines local build checks, simulator checks, and on-device logs.

## Local checks

Make sure `clang-format` 21+ is installed and available in `PATH` before running the formatting step.
If needed, see [Getting Started](./getting-started.md).

```sh
./bin/clang-format-fix
pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high
pio run -e simulator
pio run -e default
```

`pio run` without `-e` builds the X3/X4 and Sticky firmware targets from `platformio.ini`. Use it for a comprehensive build check, but prefer explicit environments while iterating.

## Flash and monitor

Flash firmware:

```sh
pio run -e default --target upload
```

Open serial monitor:

```sh
pio device monitor
```

Optional enhanced monitor:

```sh
python3 -m pip install pyserial colorama matplotlib
python3 scripts/debugging_monitor.py
```

## Diagnostic SD build

For private hardware testing, build the X3/X4 diagnostic variant instead of
the public/default firmware:

```sh
pio run -e diagnostic
pio run -e diagnostic --target upload
pio device monitor
```

The artifact is `.pio/build/diagnostic/firmware-x3-x4-diagnostic.bin`. After
the test session, copy these files from the SD card:

- `/.crosspoint/diagnostics/diagnostic.log` — UTC-timestamped `LOG_*` application events;
- `/.crosspoint/diagnostics/diagnostic.previous.log` — the previous rotated log;
- `/.crosspoint/diagnostics/last_crash_report.txt` — the panic report from the
  most recent captured panic reboot;
- `/crash_report.txt` — the normal CrossInk crash report.

The logger uses a bounded RAM queue and writes from the main loop, so it does
not perform SD I/O inside `LOG_*` calls. It is best-effort: a sudden power loss
or a failure before the SD card is mounted can still prevent the newest lines
from being saved. Do not use this build for public distribution or collect
personal book contents when sharing its logs.

## Useful bug report contents

- Firmware version and build environment
- Exact steps to reproduce
- Expected vs actual behavior
- Serial logs from boot through failure
- Whether issue reproduces after clearing the affected book cache or using **Clear Reading Cache**

## Common troubleshooting references

- [Common Issues](../troubleshooting.md)
