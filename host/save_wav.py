#!/usr/bin/env python3
"""
Receive de-noised PCM from ESP32 and save as a WAV file.

Usage:  python3 save_wav.py [PORT] [OUTPUT]
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
TOTAL_SAMPLES = 80_000
TOTAL_BYTES   = TOTAL_SAMPLES * 2

print(f"Port   : {PORT}")
print(f"Output : {OUTPUT}")
print(f"Expect : {TOTAL_BYTES} bytes  ({TOTAL_SAMPLES / SAMPLE_RATE:.1f} s @ {SAMPLE_RATE} Hz)")

# Send a start byte to trigger the ESP32, then read the stream.
# timeout=10 gives plenty of margin over the 5 s real-time stream.
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

with wave.open(OUTPUT, "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(SAMPLE_RATE)
    wav.writeframes(bytes(buf))

print(f"Saved {OUTPUT}")
