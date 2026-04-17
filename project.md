# ESP32 Edge
the github repository is github.com/elecdevelop/esp32_edge

The goal is to have a standalone edge-AI device based on ESP32-S3

An ESP32-S3 supermini module is connected to this machine via USB.

This USB connections will be used to feed test data, and received data from the ESP32-S3 board in realtime manner as well.

## Workflow: Build, Flash, and Acquire Data

### Prerequisites
- ESP-IDF installed at `~/esp/esp-idf`
- ESP32-S3 board connected via USB (`/dev/ttyACM0` — USB Serial/JTAG, `303a:1001`)
- Python `pyserial` available in the IDF Python environment

### 1. Build
```bash
source ~/esp/esp-idf/export.sh
idf.py build
```

### 2. Flash
No button press needed — the USB Serial/JTAG peripheral handles auto-reset.
```bash
idf.py -p /dev/ttyACM0 flash
```

### 3. Acquire data (save as WAV)
```bash
python3 host/save_wav.py /dev/ttyACM0
```
This sends a start byte to the ESP32, receives 7 s of de-noised PCM (16-bit mono, 16 kHz), and saves it as a timestamped WAV file in `wave_samples/`, e.g. `wave_samples/output_20260416_173309.wav`.

### Notes
- Log output (`ESP_LOGI` etc.) goes to **UART0 (GPIO 1/3)**, not USB. The USB port carries binary audio data only.
- To monitor logs you need a USB-UART adapter on GPIO 1/3, or temporarily set `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in `sdkconfig.defaults`.

---

## Step 1:
As  it is demonstrated in the docs/esp32s3_pdm_noise_cancellation.md the goal is to received data from two digital PDM mics, and perform a variety of mathematical (such as spectral subtraction and LMS adaptive filter) conversions with a cognitive analysis on it. One application would be a AI equipped de-noising setup.

To nail this, for this first step of the project, I would like to have a firmware structure in which data from two channels  of PDM mics --one for the voice and and one for the ambient noise profile -- being acquired in realtime manner, then a 32-bit precision FFT on them, which will feed a proven spectral subtraction de-noising algorithm to be perform on them. The outcome of this de-noising algorithm, would be sent to an iFFT block, to be send out as a de-noised signal. 

To perform a realtime assess/tweak on the de-noising algorithm output, an AI model is considered. This AI model is supposed to act like a human user to access the success of the de-noising algorithm, and then perform a variety of tweaks on the de-noising algorithm settings to make its output as perfect as possible.

For this first step, there is no AI model involved. also, the digital PDM mics are not connected to the ESP32-S3 supermini board. Hence, I would like the firmware to perform on some test data; on the noise profile mic, we have a 1.7KHz square signal, while on the voice mic we are supposed have a mix of a 1Khz sine wave and the already mentioned 1.7KHz square signal. The sound sampling would be 16-bit and 16ks/s .

A five second long sample of the output will be sent to the computer via USB storage protocol, to be saved as a wave file.

### Step 1 Experiments

#### experiment/simple_sine_noise
Branch: `experiment/simple_sine_noise`

This experiment validates the signal generation and DSP pipeline using two pure sine waves instead of square waves, with the noise reference kept clean (no voice content).

**Signal configuration:**
- Noise mic: 1.7 kHz sine, amplitude ±5000
- Voice mic: 1 kHz sine (±10000) + 1.7 kHz sine (±5000) mixed together
- Sample rate: 16 kHz, 16-bit PCM

The key difference from the original Step 1 spec (which used a square wave for noise) is that both channels use sine waves. This makes the spectral content simpler and more predictable, giving a cleaner baseline to verify the de-noising algorithm before introducing real mic signals.

**Files:**
- `main/signal_gen.c` — updated signal generator (sine-only, separate voice/noise amplitudes)
- `test_signal_verify.c` — host-side C program to print and analyze the first 128 samples of each channel, confirming the generator produces the expected amplitudes and waveforms
- `main/test_basic.c` — early on-device test stub for hop timing and USB output (not wired into the main build)
- `bootloader/bootloader.bin`, `partition/partition_table.bin` — pre-built binaries for standalone flashing without a full build environment

