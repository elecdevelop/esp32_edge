# ESP32-S3 Super Mini — Intelligent PDM Noise Cancellation Project
*Summary of design discussion*

---

## 1. Hardware

### Microphone Module
- **MP34DT01** — PDM Digital MEMS Microphone
- Interface: PDM (not I2S, not analog)
- Two mics on a single PDM line via L/R select pin:
  - Mic 1 (Voice): L/R pin → GND
  - Mic 2 (Noise reference): L/R pin → 3.3V

### Wiring (per mic)
| MP34DT01 Pin | ESP32-S3 Super Mini |
|---|---|
| VDD | 3.3V |
| GND | GND |
| CLK | GPIO 41 (shared) |
| DOUT | GPIO 42 (shared) |
| L/R | GND (Mic1) / 3.3V (Mic2) |

### PDM Mic Limit on Super Mini
| I2S Peripheral | Mics |
|---|---|
| I2S0 PDM RX | Mic 1 (L) + Mic 2 (R) |
| I2S1 PDM RX | Mic 3 (L) + Mic 4 (R) |
| **Maximum** | **4 PDM mics** |

> The Super Mini exposes ~16 usable GPIOs. USB (GPIO 19/20), Flash (GPIO 26–32), and UART0 (GPIO 43/44) are reserved. Two PDM buses consume only 4 pins.

---

## 2. Audio Capture Specs

| Parameter | Value |
|---|---|
| Sample rate | 16 kHz |
| Resolution | 16-bit PCM (decimated from 1-bit PDM) |
| PDM clock | ~1.024 MHz (16kHz × 64 decimation) |
| Capture method | DMA — zero CPU cost |
| Block size | 256 samples = 16ms per block |

---

## 3. DSP Pipeline

### Overview
```
Mic 1 ──► FFT ──────────────────────► Spectral Sub ──► iFFT ──► Output
                                           ▲     │
Mic 2 ──► FFT ──► Noise Estimator ─────────┘     │
                                                 │ pre-iFFT spectrum
                                            ┌────▼─────┐
                                            │ AI Model │
                                            └────┬─────┘
                                                 │ parameter adjustments
                                            ┌────▼──────────────┐
                                            │  DSP Parameters   │
                                            │  mu, gain, floor  │
                                            └───────────────────┘
```

### Processing Steps per Block
1. DMA captures stereo PDM, I2S peripheral decimates to 16-bit PCM
2. FFT (256pt) on both Mic 1 and Mic 2
3. Spectral subtraction: subtract Mic 2 noise spectrum from Mic 1
4. LMS adaptive filter for residual time-domain cleanup
5. iFFT + overlap-add (50% overlap) → clean audio output

### FFTs Required
- FFT Mic 1 (voice + noise)
- FFT Mic 2 (noise reference)
- iFFT of cleaned spectrum
- **Total: 3× FFT per block**

### Library
Use **esp-dsp** (Espressif optimized, hardware-accelerated on ESP32-S3):
```c
#include "esp_dsp.h"
dsps_fft2r_init_fc32(NULL, 256);
dsps_fft2r_fc32(mic1_buffer, 256);
```

---

## 4. CPU & Memory Budget

### CPU (Single Core)
| Operation | CPU Load |
|---|---|
| PDM capture (DMA) | 0% |
| 3× FFT/iFFT (256pt) | ~10% |
| Spectral subtraction + LMS | ~8% |
| Overlap-add, VAD, SNR | ~4% |
| **DSP Total** | **~22%** |
| AI supervisor | ~3–5% |
| **Grand Total** | **~25–27%** |

### Dual-Core Split (ESP32-S3 has 2× Xtensa LX7 @ 240MHz)
| Core 0 | Core 1 |
|---|---|
| RTOS, WiFi/BT, USB audio | DSP pipeline |
| AI supervisor (~5%) | FFT + spectral sub + LMS (~22%) |
| **~20% load** | **~22% load** |
| **~80% headroom** | **~78% headroom** |

### Memory
| Buffer | Size |
|---|---|
| DMA buffers (double, stereo) | 2 KB |
| FFT workspace (256pt complex) | 2 KB |
| Noise spectrum estimate | 512 B |
| Overlap buffer | 1 KB |
| LMS weights + delay line | 512 B |
| AI model weights (MLP/LSTM) | ~8 KB |
| AI feature buffer | ~1 KB |
| ESP-IDF / RTOS overhead | ~50 KB |
| **Total** | **~65 KB / 512 KB** |

---

## 5. AI Supervisor Layer

### Role
The AI does **not** process audio directly. It watches the DSP pipeline's environment and output quality, then adjusts filter parameters in real time — acting as a closed-loop controller.

### Parameters the AI Controls
| Parameter | Range | Effect |
|---|---|---|
| LMS step size `mu` | 0.001–0.1 | Adaptation speed |
| Spectral subtraction gain | 0.0–1.0 | Aggressiveness |
| Noise floor adjustment | ±6 dB | Sensitivity |
| VAD threshold | adaptive | Voice/silence boundary |

### Feature Vector (AI Inputs)
```c
typedef struct {
    // Feedforward
    float noise_floor_db;
    float snr_estimate;
    float vad_confidence;
    float noise_class[4];      // fan/traffic/crowd/silence

    // Feedback (pre-iFFT tap)
    float residual_energy;
    float residual_spectral[8]; // sub-band residuals
    float output_snr;
    float param_history[4];
    float improvement_db;
} ai_feature_vector_t;
```

### Model Architecture
```
Feature vector (16–20 floats)
        │
   ┌────▼──────┐
   │ Small     │  Tiny LSTM (16 hidden units)
   │ LSTM/MLP  │  or MLP 32→16→8
   └────┬──────┘
        │
   Parameter adjustments (4–6 floats)
```
- Model size: **< 10 KB** (int8 quantized)
- Inference rate: **~50 Hz** (every block or every few blocks)
- Inference cost: **~3–5% CPU on Core 0**

### FreeRTOS Task Pinning
```c
// Core 1 — DSP (time critical)
xTaskCreatePinnedToCore(dsp_pipeline_task, "DSP",
    4096, NULL, configMAX_PRIORITIES - 1, &dsp_handle, 1);

// Core 0 — AI supervisor
xTaskCreatePinnedToCore(ai_supervisor_task, "AI",
    4096, NULL, 5, &ai_handle, 0);
```

---

## 6. Feedback Loop & Quality Estimation

### The Feedback Tap
Sits **between spectral subtraction and iFFT** — everything still in frequency domain. The AI sees the residual spectrum and can determine *where* (which frequency bands) cancellation failed.

### Quality Metrics (computed in FFT domain)

**Spectral Flatness** — noise detector (pure tone=0, white noise=1, voice~0.1–0.3)
```c
float spectral_flatness(float *magnitude, int bins);
```

**Voice Band Energy Ratio** — energy in 300Hz–3.4kHz vs total
```c
float voice_band_ratio(float *magnitude, int bins, int sample_rate);
```

**Harmonic Ratio** — detects pitch structure (F0 + harmonics)
```c
float harmonic_ratio(float *magnitude, int bins, float f0_hz, int sample_rate);
```

**Spectral Envelope Smoothness** — clean voice = smooth, noise = jagged
```c
float envelope_smoothness(float *magnitude, int bins);
```

### Composite Quality Score
Four metrics → small MLP (4→8→1) → **quality_score (0.0–1.0)**

Training target: **DNSMOS** labels generated offline on your dataset.

### What the Quality Score Catches
| Artifact | Detected By |
|---|---|
| Broadband noise residual | Spectral flatness ↑ |
| Musical noise (FFT artifacts) | Envelope smoothness ↓ |
| Over-subtraction (voice damaged) | Voice band ratio ↓ + harmonic ratio ↓ |
| Low-freq rumble | Voice band ratio ↓ |
| Tonal interference / hum | Flatness ↓, envelope roughness ↑ |
| Good clean voice | All metrics in target range |

### Stability Safeguards
```c
// Clamp max change per block
params.spectral_gain = CLAMP(
    last_params.spectral_gain + output[1] * MAX_DELTA,
    MIN_GAIN, MAX_GAIN);

// Exponential smoothing
params.mu_lms = 0.7f * last_params.mu_lms + 0.3f * output[0];
```

---

## 7. Toolchain

| Stage | Tool |
|---|---|
| Audio capture + pipeline | ESP-IDF + ESP-ADF |
| FFT / DSP | esp-dsp library |
| Feature extraction / training | Python + librosa + TensorFlow/Keras |
| Quality labels | DNSMOS (offline) |
| Quantization | TFLite post-training int8 |
| On-device inference | TFLite Micro |
| Edge Impulse (optional) | Faster prototyping for classifier |

---

## 8. Development Roadmap

| Phase | Goal |
|---|---|
| 1 | Validate hardware — confirm stereo PDM capture, record raw audio |
| 2 | DSP pipeline — FFT/spectral subtraction with static parameters |
| 3 | Feature extraction — SNR, noise floor, spectral shape logging |
| 4 | AI model — collect dataset, train MLP/LSTM, quantize to int8 |
| 5 | Integration — close the loop, AI outputs feed DSP parameters |

---

## 9. Expected Performance

| Noise Type | Expected Result |
|---|---|
| Stationary noise (fan, HVAC) | ✅ Excellent — LMS handles well, AI maintains |
| Slowly varying (street, traffic) | ✅ Good — AI detects transitions early |
| Tonal noise (AC hum, machinery) | ✅ AI tunes spectral subtraction specifically |
| Music / complex noise | ⚠️ Meaningfully better than fixed DSP alone |
| Sudden impulse noise | ⚠️ Brief breakthrough before adaptation |
| Loop latency | ~16ms (one block delay) |

---

## 10. Future Expansion Possibilities

- **Beamforming** with 3–4 mics (I2S1 still free)
- **Wake word detection** via ESP-SR running concurrently
- **USB audio class** — present as a USB microphone to a PC
- **Bluetooth audio** output — wireless clean audio stream
- **Higher sample rate** — 32kHz for wider voice bandwidth (headroom exists)
- **Wiener filter** on top of spectral subtraction for cleaner output
- **Acoustic camera** with 8–16 mic array (requires external PDM aggregator or stronger SoC)
