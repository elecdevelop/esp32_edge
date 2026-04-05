#!/bin/bash
# One-command flash for ESP32-S3 — no buttons required.
# Works whether the firmware (303a:4001) or ROM bootloader (303a:1001) is running.

set -e
source ~/esp/esp-idf/export.sh 2>/dev/null

PORT=${1:-/dev/ttyACM0}

# ---- If the firmware is running, send the magic reset command ------------
if lsusb | grep -q "303a:4001"; then
    echo "Firmware running — sending reset command..."
    python3 -c "
import serial, time
with serial.Serial('$PORT', baudrate=115200, timeout=1) as s:
    s.write(b'ESPBOOT')
    s.flush()
    time.sleep(0.1)
"
    # Wait up to 4 s for ROM bootloader to enumerate
    echo -n "Waiting for ROM bootloader"
    for i in $(seq 1 20); do
        sleep 0.2
        if lsusb | grep -q "303a:1001"; then echo " OK"; break; fi
        echo -n "."
    done
    lsusb | grep -q "303a:1001" || { echo; echo "ERROR: ROM bootloader did not appear"; exit 1; }
fi

# ---- Flash (device is already in download mode) -------------------------
python3 -m esptool \
    --chip esp32s3 \
    -p "$PORT" \
    --before no_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio \
    --flash_size 4MB \
    --flash_freq 80m \
    0x0      build/bootloader/bootloader.bin \
    0x8000   build/partition_table/partition-table.bin \
    0x10000  build/esp32_edge_step1.bin

echo ""
echo "Done — firmware booting."
