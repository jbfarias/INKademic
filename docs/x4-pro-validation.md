---
title: X4 Pro Validation
nav_order: 16
---

# X4 Pro Validation Matrix

INKademic has a dedicated X4 Pro firmware and simulator profile. The
simulator is useful for UI and input regressions, but it does not replace a
physical X4 Pro for display-controller, battery-gauge, frontlight, USB and
sleep/wake validation.

## Automated simulator checks

Run the smoke test for the X3/X4 profile and the X4 Pro profile:

```sh
python3 scripts/run_simulator_smoke_test.py --env simulator --page-turns 3
python3 scripts/run_simulator_smoke_test.py --env x4-pro-simulator --page-turns 3
```

The X4 Pro scenario exercises touch page turns, the frontlight panel, the
capacitive Home key, reader-menu access with touch disabled, menu tabs and
reader-options scrolling.

The next physical validation pass must also cover the fixes currently tracked
for the next release candidate: the **Restore frontlight on wake** setting must
be visible under Display, both side keys must navigate lists and dialogs as
previous/next, USB Drive must return to Home after the host ejects or the cable
is removed, and the INKademic book-and-pen logo must appear during boot and
sleep.

## Physical-device checklist

Before publishing a firmware build, verify the following on an actual X4 Pro:

1. Cold boot and wake from sleep render the INKademic logo and display with the correct
   orientation and without a mirrored image.
2. **Restore frontlight on wake** is present in Display settings and, when enabled,
   preserves the light's on/off state after sleep and Quick Lock.
3. Brightness can be reduced to the expected low level and warm/cool control
   remains responsive.
4. Touch page turns, both physical side keys, edge gestures, the capacitive Home key and touch-disabled
   reader mode do not trigger duplicate actions.
5. Power + Up, Quick Lock, Home-key shortcuts and USB Drive return to the
   reader without losing the current page or pending annotation.
6. Removing the USB cable after the computer mounts or ejects the SD card
   returns to Home without pressing Reset; a failed mount also returns cleanly.
7. Battery percentage changes after charging and after a normal reading
   session; the device does not reboot or drain unusually during sleep.
8. A large text EPUB, a table-heavy EPUB, an image-heavy EPUB and an EPUB
   with a long table of contents open or fall back safely without a reboot.

Record the firmware version, SDK revision, display-controller variant and test
book names with each physical run. INKademic must not claim full X4 Pro
hardware validation from simulator results alone.
