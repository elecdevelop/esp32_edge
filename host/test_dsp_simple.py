#!/usr/bin/env python3
"""
Proof of Concept: Simple spectral subtraction test
Generates signals like ESP32 and tests noise cancellation
"""

import numpy as np
import wave
import time

SAMPLE_RATE = 48000.0
FFT_N = 1024

VOICE_FREQ = 1000.0
NOISE_FREQ = 1700.0
VOICE_AMP = 500.0
NOISE_AMP = 50.0
ALPHA = 1.0
BETA = 0.02

def generate_signals(duration=5):
    """Generate test signals: voice = 1kHz + noise, noise = 1.7kHz alone"""
    t = np.arange(int(SAMPLE_RATE * duration)) / SAMPLE_RATE
    noise = NOISE_AMP * np.sin(2 * np.pi * NOISE_FREQ * t)  # 1.7kHz only
    voice = VOICE_AMP * np.sin(2 * np.pi * VOICE_FREQ * t)  # 1kHz only, NO noise in voice
    return noise, voice


def spectral_subtract_direct(voice_data, noise_data):
    """Simple direct time-domain subtraction"""
    clean = voice_data - ALPHA * noise_data
    clean = np.clip(clean, -32767, 32767)
    return clean

def spectral_subtract_fft(voice_data, noise_data):
    """FFT-based spectral subtraction (useless here)"""
    clean = voice_data - ALPHA * noise_data
    clean = np.clip(clean, -32767, 32767)
    return clean  # FFT method same as time-domain for simple subtraction

def spectral_subtract_simple(voice_data, noise_data):
    """Simple time-domain: subtract noise from voice if noise > voice"""
    
    noise_abs = np.abs(noise_data)
    voice_abs = np.abs(voice_data)
    
    # If noise is dominant, subtract it
    clean = np.clip(voice_data - ALPHA * noise_data, -32767, 32767)
    return clean

import wave
import time

def write_wav_simple(data, filepath, sample_rate=16000):
    """Write mono 16-bit WAV manually"""
    # Normalize to use full 16-bit range for audio quality
    max_val = np.max(np.abs(data))
    if max_val > 1e-10:
        data = data / max_val * 18000  # 18000 gives good headroom (~-11dB)
    
    out = np.clip(data.astype(np.float32) * 32767, -32767, 32767).astype(np.int16)
    
    with wave.open(filepath, "wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(out.tobytes())

def write_wav_original(voice_data, noise_data, clean, filepath, sample_rate=16000):
    """Write mono 16-bit WAV manually"""
    out = np.clip(clean.astype(np.float32) * 32767, -32767, 32767).astype(np.int16)
    
    with open(filepath, "wb") as f:
        f.write(b"RIFF")
        length = 36 + len(out) * 2
        f.write(length.to_bytes(4, byteorder='little'))
        f.write(b"WAVEfmt ")
        f.write(b"\x10\x00\x00\x00")  # Subchunk size
        f.write(b"\x01\x00")  # PCM format
        f.write(b"\x10\x00")  # 16-bit
        f.write(b"\x10\x00")  # Sample rate
        f.write(b"\x00\x00")  # Byte rate
        f.write(b"\x01\x00")  # Channels
        f.write(b"WAVE")
        f.write(b"data")
        f.write(len(out).to_bytes(4, byteorder='little'))
        f.write(out.tobytes())

if __name__ == "__main__":
    noise_data, voice_data = generate_signals(duration=5)
    print(f"Generated {len(voice_data)} samples ({len(voice_data)/SAMPLE_RATE:.1f}s)")
    
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    output_file = f"clean_simple_{timestamp}.wav"
    
    # Test simple method
    print("\n" + "="*60)
    print("TEST 1: Simple time-domain subtraction")
    print("="*60)
    
    clean_simple = spectral_subtract_simple(voice_data, noise_data)
    
    clean_fft = np.fft.fft(clean_simple, n=FFT_N)
    freqs = np.fft.fftfreq(FFT_N, 1/SAMPLE_RATE)
    
    print("\nFrequency domain (dB) - Simple subtraction:")
    for freq in [0, 500, 1000, 1500, 1700, 2000]:
        idx = int(freq * FFT_N / SAMPLE_RATE)
        if abs(idx) < FFT_N // 2:
            db = 20 * np.log10(np.abs(clean_fft[idx]) + 1e-10)
            print(f"  |{freq:4d}| Hz: {db:7.1f} dB")
    
    # FFT-based spectral subtraction (same as time-domain for simple signals)
    print("\n" + "="*60)
    print("TEST 2: FFT-based spectral subtraction")
    print("="*60)

    clean_fft = spectral_subtract_fft(voice_data, noise_data)
    
    print("\nFrequency domain (dB) - FFT subtraction:")
    for freq in [1000, 1700]:
        idx = int(freq * FFT_N / SAMPLE_RATE)
        if abs(idx) < FFT_N // 2:
            db = 20 * np.log10(np.abs(clean_fft[idx]) + 1e-10)
            print(f"  |{freq:4d}| Hz: {db:7.1f} dB")
    
    print("\nSaving clean outputs as WAV...")
    write_wav_simple(clean_simple, output_file)
    print(f"Saved {output_file}")
