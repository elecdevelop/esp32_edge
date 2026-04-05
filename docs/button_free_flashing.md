# Button-Free Flashing on ESP32-S3 Supermini

## Problem

The ESP32-S3 supermini has a single USB connector wired to the chip's native USB
peripheral (GPIO19/GPIO20). This USB port serves two purposes:

1. Flashing firmware
2. Real-time PCM data transfer to the host computer

The initial firmware used **TinyUSB** (USB OTG) for data transfer. When TinyUSB
is active, it takes over the USB pins and disables the ESP32-S3's built-in
**USB Serial/JTAG** peripheral — the one esptool relies on for automatic
reset into bootloader mode. This made button-free flashing impossible while
the firmware was running.

## Failed Approaches

### DTR/RTS two-transistor auto-reset circuit
Standard on boards with an external USB-UART bridge (CH340, CP210x, FTDI).
Not applicable here — the supermini has no external UART chip.

### Magic string (ESPBOOT) over TinyUSB CDC
The firmware listened for a `ESPBOOT` byte sequence, then wrote the
`RTC_CNTL_FORCE_DOWNLOAD_BOOT_M` flag to `RTC_CNTL_OPTION1_REG` and called
`esp_restart()` to reboot into the ROM download stub. The host-side script
(`flash.sh`) sent the magic string, then polled `lsusb` for VID `303a:1001`.

This approach was architecturally sound but unreliable in practice — the
TinyUSB CDC port and the auto-reset mechanism shared the same USB peripheral,
making the reset sequence race-prone and fragile.

## Solution: Switch to USB Serial/JTAG

The ESP32-S3 has a **built-in USB Serial/JTAG peripheral** that is completely
independent of TinyUSB/USB OTG. When the application uses USB Serial/JTAG for
data transfer instead of TinyUSB, the peripheral handles auto-reset in hardware
— esptool's `--before default_reset` works transparently, with no magic
strings and no button presses.

**Data rate check:** 16 kHz × 16-bit = 32 KB/s. USB Serial/JTAG is Full Speed
USB (~400 KB/s practical), so the data stream uses less than 10% of available
bandwidth.

**Logging:** The ESP-IDF console was already configured to use UART0
(`CONFIG_ESP_CONSOLE_UART_DEFAULT=y`), so log messages do not pollute the USB
data stream.

## Changes Made

### `main/usb_cdc.c`
- Removed all TinyUSB includes and callbacks
- Replaced with `driver/usb_serial_jtag.h`
- `usb_cdc_init()` calls `usb_serial_jtag_driver_install()`
- `usb_cdc_wait_for_host()` blocks on `usb_serial_jtag_read_bytes()` waiting
  for a single start byte from the host
- `usb_cdc_write_samples()` uses `usb_serial_jtag_write_bytes()`

### `main/CMakeLists.txt`
- Added `REQUIRES esp_driver_usb_serial_jtag esp_timer`
  (`esp_timer` was previously pulled in transitively by TinyUSB)

### `main/idf_component.yml`
- Removed `espressif/esp_tinyusb` dependency

### `flash.sh`
- Reduced to a simple `esptool` call with `--before default_reset --after hard_reset`
- No VID:PID polling, no magic string, no port discovery logic

### `host/save_wav.py`
- Replaced DTR assertion with an explicit start byte (`\x01`) sent to the
  ESP32 before reading the stream

## One-Time Bootstrap

The very first flash of the new firmware required one manual BOOT+RESET because
the old TinyUSB firmware was still on the chip and could not auto-reset.
All subsequent flashes are fully button-free.

## Usage

```bash
# Flash firmware (no buttons needed)
bash flash.sh

# Capture a 5-second de-noised WAV sample
python3 host/save_wav.py
```
