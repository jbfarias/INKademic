---
title: Installation
nav_order: 2
---

# Installation

## Supported Devices

- Xteink X3, X4
- Xteink X4 Pro
- Seeed Studio Sticky

## Web Installation via USB

#### For new installs and updates.

1. Navigate to [https://inky.crossink.dev/#flash-tools](https://inky.crossink.dev/#flash-tools) and select your device model.
2. The latest version will be automatically selected, but if you ever want to revert to an earlier build, you can select it from the dropdown.
3. Choose the firmware option you want to install.
4. Click on the "Flash Firmware" button

X4 Pro uses the ESP32-S3 firmware option. Keep the reader connected during the
download-mode and flashing steps shown by Inky.

## USB Drive

On X4 Pro, choose `Home > File Transfer > USB Drive` to expose the SD card to
your computer. Eject the drive from the computer before disconnecting it; the
reader restarts to Home when the drive is safely ejected or the cable is
removed.

## SD Card Firmware Update

#### For installing newer versions of INKademic. Can be used by USB locked devices.

1. Follow the same steps from the Web Installation method above. There will be an option to download the firmware instead of USB flashing.
2. Place the downloaded `firmware-*.bin` file on your SD card. You can place this file anywhere.
3. Go to `Settings > System > SD Card Firmware Update` and navigate to the `.bin` file and update.

## USB Locked Devices

If your device has USB data transfer disabled:

1. Navigate to [https://inky.crossink.dev/#flash-tools](https://inky.crossink.dev/#flash-tools) and check the box for "I have a locked device" at the top.
2. The latest version will be automatically selected, but if you ever want to revert to an earlier build, you can select it from the dropdown.
3. Choose the firmware option you want to download.
4. Click on the "Download update.bin" button and follow the instructions.

### X4 Pro recovery when USB is not recognized

If an X4 Pro is stuck in the old firmware or does not expose its normal USB
interface, start the ESP32-S3 ROM download mode before flashing:

1. Disconnect the USB cable.
2. Press and hold the **left side button** (the boot/GPIO0 button).
3. Connect a known-good data USB-C cable directly to the computer. If needed,
   press and release Reset while continuing to hold the left button.
4. Release the left button when macOS shows `/dev/cu.usbmodem*` (or Linux shows
   a new `/dev/ttyACM*` device).
5. Use the web installer, selecting **X4 Pro / ESP32-S3**, or use the command
   line below.

If no serial device appears even in ROM mode, try another data-capable cable,
USB port, and computer without a hub. Do not hold the left button during a
normal reboot after flashing, or the reader will enter download mode again.

## Command Line

These instructions are for macOS and Linux. Windows users should use the web installer.

Install `esptool`:

```sh
pip3 install esptool
```

Download the `firmware-*.bin` file from the [INKademic releases page](https://github.com/jbfarias/INKademic/releases), then connect your device with USB-C.

Find the device port:

```sh
# Linux
dmesg | grep tty

# macOS
ls /dev/cu.*
```

Flash the firmware:

```sh
# X3/X4 and Sticky — ESP32-C3
# Linux
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin

# macOS
esptool.py --chip esp32c3 --port /dev/cu.usbmodem2101 --baud 921600 write_flash 0x10000 /path/to/firmware.bin

# X4 Pro — ESP32-S3 (use the port discovered in ROM download mode)
# Linux
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware-x4-pro.bin

# macOS
esptool.py --chip esp32s3 --port /dev/cu.usbmodem2101 --baud 921600 write_flash 0x10000 /path/to/firmware-x4-pro.bin
```

The X4 Pro command must use `--chip esp32s3`; it is not an ESP32-C3 image.
Do not erase the entire flash as a first recovery step.

Replace the port and firmware path with your actual values.
