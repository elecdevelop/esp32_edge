#!/usr/bin/env python3
"""
Receive PCM from ESP32 (sequential raw then denoised) and save as WAV files.

Usage:  python3 save_wav.py [PORT] [OUTPUT]

ESP32 sends: [ALL raw] [ALL denoised]
2 seconds @ 16 kHz = 32 000 samples per channel = 64 KB each.
Total: 128 KB.
"""
import sys
import wave
import serial
import os
from datetime import datetime

PORT         = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
OUTDIR       = "wave_samples"
os.makedirs(OUTDIR, exist_ok=True)
timestamp    = datetime.now().strftime("%Y%m%d_%H%M%S")
OUTPUT       = os.path.join(OUTDIR, f"output_{timestamp}.wav")
SAMPLE_RATE   = 16000
RECORD_SECONDS = 2
TOTAL_SAMPLES  = SAMPLE_RATE * RECORD_SECONDS  # 32,000
RAW_BYTES      = TOTAL_SAMPLES * 2              # 64,000 bytes per channel
TOTAL_BYTES    = RAW_BYTES * 2                  # 128,000 bytes

outdir = os.path.dirname(OUTPUT) or "."
raw_path   = os.path.join(outdir, f"raw_{os.path.basename(OUTPUT)}")
denoised_path = os.path.join(outdir, f"denoised_{os.path.basename(OUTPUT)}")

print(f"Port   : {PORT}")
print(f"Raw output       : {raw_path}")
print(f"Denoised output  : {denoised_path}")
print(f"Expect : {TOTAL_BYTES} bytes  ({RECORD_SECONDS:.1f} s @ {SAMPLE_RATE} Hz)")
print(f"         ({TOTAL_SAMPLES} raw + {TOTAL_SAMPLES} denoised samples)")

# Send a start byte to trigger the ESP32, then read the stream.
with serial.Serial(PORT, baudrate=115200, timeout=10) as ser:
    ser.reset_input_buffer()
    ser.write(b'\x01')      # start signal — wakes ESP32 from wait_for_host()

    print("Receiving PCM data:")
    buf = bytearray()
    while len(buf) < TOTAL_BYTES:
        chunk = ser.read(min(512, TOTAL_BYTES - len(buf)))
        if not chunk:
            print(f"\nTimeout after {len(buf)} / {TOTAL_BYTES} bytes.")
            sys.exit(1)
        buf += chunk
        pct = 100 * len(buf) // TOTAL_BYTES
        print(f"\r  {len(buf):>7} / {TOTAL_BYTES} bytes  ({pct:>3}%)", end="", flush=True)

print(f"\rReceived {len(buf)} bytes.                  ")

# ESP32 sends all raw first, then all denoised (sequential, not interleaved)
raw_data     = buf[:RAW_BYTES]
denoised_data = buf[RAW_BYTES:]

for label, data in [("Raw (voice+noise)", raw_data), ("Denoised", denoised_data)]:
    path = raw_path if label == "Raw (voice+noise)" else denoised_path
    with wave.open(path, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(bytes(data))
    print(f"Saved {path}  ({len(data)} bytes = {len(data) // 2} samples)")
