#!/bin/bash
# Button-free flash for ESP32-S3 supermini.
# Uses the built-in USB Serial/JTAG auto-reset — no buttons needed.

set -e

source ~/esp/esp-idf/export.sh 2>/dev/null

PORT=${1:-/dev/ttyACM0}

python3 -m esptool \
    --chip esp32s3 \
    -p "$PORT" \
    --before default_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio \
    --flash_size 4MB \
    --flash_freq 80m \
    0x0     build/bootloader/bootloader.bin \
    0x8000  build/partition_table/partition-table.bin \
    0x10000 build/esp32_edge_step1.bin

echo ""
echo "Done — firmware booting."
