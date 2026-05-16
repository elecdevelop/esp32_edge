# ESP32-S3 Noise Cancellation Project - Status

## Project Overview

**Goal:** Edge-AI noise cancellation on ESP32-S3 using two PDM microphone channels (voice + noise reference)

**Current Phase:** Step 1 - Test with synthetic signals (no physical mics connected yet)

---

## Current Algorithm: Time-Domain Subtraction

### Why This Approach?

The project shifted from FFT spectral subtraction to time-domain subtraction because:

1. **Test Signal Characteristics:** Synthetic noise is a coherent 1.7 kHz sine wave present in both channels with identical phase
2. **Perfect Cancellation:** Time-domain subtraction `clean = voice - noise` cancels coherent noise exactly
3. **Simplicity:** No FFT overhead, minimal memory usage

### How It Works

```
Voice channel (Mic 1):  1 kHz sine + 1.7 kHz noise
Noise channel (Mic 2):  1.7 kHz noise (reference only)

Output = Voice - Noise = 1 kHz sine (noise removed)
```

At 1.7 kHz: both channels have same amplitude → subtraction = 0
At 1.0 kHz: noise channel is 0 → subtraction = original signal

### Current Parameters

```c
#define ALPHA  1.0f   // Full subtraction strength
#define BETA   0.02f  // Spectral floor (kept for future FFT use)
```

---

## Known Limitations of Time-Domain Subtraction

| Scenario | Works? | Why |
|----------|--------|-----|
| Coherent noise (same phase) | ✅ Yes | Perfect cancellation |
| Incoherent noise (different phase) | ❌ No | Phase mismatch |
| Broadband noise | ❌ No | Only cancels exact match |
| Reverberant environment | ❌ No | Reflections cause phase shifts |

**Note:** For real-world use, a hybrid approach (LMS adaptive filter + FFT spectral subtraction) is recommended.

---

## Hardware Configuration

### ESP32-S3 Super Mini

- **Chip:** ESP32-S3 (QFN56, dual core, 240 MHz)
- **USB:** USB Serial/JTAG (GPIO 19/20)
- **Flash:** 4 MB
- **PSRAM:** 2 MB
- **MAC:** d0:cf:13:07:bc:8c

### Planned Microphones

- **MP34DT01** PDM mics (2x)
- Voice mic + noise reference mic
- **Status:** Not yet connected (using synthetic signals)

---

## Audio Parameters

| Parameter | Value |
|-----------|-------|
| Sample Rate | 16,000 Hz |
| Bit Depth | 16-bit |
| Capture Duration | 2 seconds |
| Total Samples | 32,000 per channel |
| Hop Size | 128 samples (8 ms) |
| Test Signal - Voice | 1 kHz sine ±10,000 |
| Test Signal - Noise | 1.7 kHz sine ±5,000 |

---

## Data Flow

### ESP32 Firmware

1. **Wait** for host trigger byte (`\x01`)
2. **Reset** DSP pipeline and signal generator
3. **Capture** 2 seconds of audio (buffer both raw and denoised)
4. **Transmit** raw samples first (32,000 samples = 64 KB)
5. **Transmit** denoised samples second (32,000 samples = 64 KB)
6. **Total:** 128 KB per capture

### Host Software (Python)

1. **Send** trigger byte to ESP32
2. **Receive** 128,000 bytes total
3. **Split** into raw (first 64 KB) and denoised (last 64 KB)
4. **Save** as WAV files

---

## Build & Flash Instructions

### ESP-IDF Configuration

```bash
export IDF_PATH=/home/meysam/.espressif/v6.0.1/esp-idf
export ESP_IDF_VERSION=6.0.1
export IDF_PYTHON_ENV_PATH=/home/meysam/.espressif/python_env/idf6.0_py3.12_env
export PATH="/home/meysam/.espressif/tools/ninja/1.12.1:$PATH"
```

### Build and Flash

```bash
# Build firmware
idf.py build

# Flash to ESP32 (port may vary)
idf.py -p /dev/ttyACM0 flash

# Monitor serial output
idf.py -p /dev/ttyACM0 monitor
```

### Capture Wave Files

```bash
# From project root
python3 host/save_wav.py /dev/ttyACM0
```

---

## File Structure

```
esp32_edge/
├── CMakeLists.txt              # Project build config
├── main/
│   ├── main.c                  # Application entry point
│   ├── dsp_pipeline.c/h        # Noise cancellation algorithm
│   ├── signal_gen.c/h          # Synthetic test signal generator
│   ├── usb_cdc.c/h             # USB Serial/JTAG communication
│   └── CMakeLists.txt          # Component build config
├── host/
│   ├── save_wav.py             # Host-side WAV capture script
│   └── test_dsp_simple.py      # Python reference implementation
├── wave_samples/               # Captured WAV files
└── docs/
    └── esp32s3_pdm_noise_cancellation.md  # Hardware design docs
```

---

## Memory Usage

| Component | Size |
|-----------|------|
| `raw_buf` (32,000 samples) | 64 KB |
| `denoised_buf` (32,000 samples) | 64 KB |
| DSP pipeline buffers | ~1 KB |
| USB driver buffers | ~5 KB |
| FreeRTOS stacks | ~30 KB |
| **Total** | **~164 KB** |
| **Available SRAM** | **512 KB** |
| **Margin** | **~348 KB** |

---

## Next Steps (Future Phases)

### Phase 2: Hybrid Noise Cancellation
- **Stage 1:** LMS adaptive filter for coherent noise
- **Stage 2:** FFT spectral subtraction for incoherent/broadband noise
- More robust for real-world environments

### Phase 3: Real PDM Microphones
- Connect MP34DT01 mics via I2S PDM peripheral
- Implement proper PDM-to-PCM conversion
- Test with actual ambient noise

### Phase 4: AI Enhancement
- On-device ML model for adaptive noise classification
- Real-time parameter adjustment based on noise type

---

## Key Lessons Learned

1. **FFT spectral subtraction assumes incoherent noise** - works poorly for coherent single-tone noise
2. **Time-domain subtraction is perfect for coherent noise** - but limited to similar scenarios
3. **Interleaved vs sequential transmission** - ESP32 sends interleaved, host must deinterleave OR firmware must buffer and send sequentially
4. **Memory constraints** - 2-second capture with dual buffering fits, longer captures require host-side deinterleaving
5. **Signal generator phase reset** - critical for deterministic test results across captures

---

*Last updated: 2026-05-16*
