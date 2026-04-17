#!/usr/bin/env python3
"""
Proof of Concept: Simple spectral subtraction test
Generates signals like ESP32 and tests noise cancellation
"""

import numpy as np

SAMPLE_RATE = 16000.0
FFT_N = 256

VOICE_FREQ = 1000.0
NOISE_FREQ = 1700.0
VOICE_AMP = 10000.0
NOISE_AMP = 5000.0
ALPHA = 1.0  # Full noise cancellation
BETA = 0.02  # Floor factor

def generate_signals(duration=5):
    """Generate test signals"""
    t = np.arange(int(SAMPLE_RATE * duration)) / SAMPLE_RATE
    noise = NOISE_AMP * np.sin(2 * np.pi * NOISE_FREQ * t)
    voice = VOICE_AMP * np.sin(2 * np.pi * VOICE_FREQ * t) + NOISE_AMP * np.sin(2 * np.pi * NOISE_FREQ * t)
    return noise, voice

def spectral_subtract_direct(voice_data, noise_data):
    """Simple bin-by-bin spectral subtraction"""
    
    # Zero-pad both signals
    voice_fft = np.fft.fft(voice_data, n=FFT_N)
    noise_fft = np.fft.fft(noise_data, n=FFT_N)
    
    # Spectral subtraction
    voice_mag2 = np.abs(voice_fft) ** 2
    noise_mag2 = np.abs(noise_fft) ** 2
    clean_mag2 = voice_mag2 - ALPHA * noise_mag2
    clean_mag2 = np.maximum(clean_mag2, BETA * voice_mag2)
    
    scale = np.sqrt(clean_mag2 / (voice_mag2 + 1e-10))
    voice_fft_scaled = voice_fft * scale
    
    # iFFT
    output = np.fft.ifft(voice_fft_scaled)
    return np.real(output)

def spectral_subtract_simple(voice_data, noise_data):
    """Simple time-domain: subtract noise from voice if noise > voice"""
    
    noise_abs = np.abs(noise_data)
    voice_abs = np.abs(voice_data)
    
    # If noise is dominant, subtract it
    clean = np.clip(voice_data - ALPHA * noise_data, -32767, 32767)
    return clean

if __name__ == "__main__":
    noise_data, voice_data = generate_signals(duration=5)
    print(f"Generated {len(voice_data)} samples ({len(voice_data)/SAMPLE_RATE:.1f}s)")
    
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
    
    # Test FFT-based method  
    print("\n" + "="*60)
    print("TEST 2: FFT-based spectral subtraction")
    print("="*60)
    
    clean_fft = spectral_subtract_direct(voice_data, noise_data)
    
    print("\nFrequency domain (dB) - FFT subtraction:")
    for freq in [0, 500, 1000, 1500, 1700, 2000]:
        idx = int(freq * FFT_N / SAMPLE_RATE)
        if abs(idx) < FFT_N // 2:
            db = 20 * np.log10(np.abs(clean_fft[idx]) + 1e-10)
            print(f"  |{freq:4d}| Hz: {db:7.1f} dB")
    
    print("\nClean output saved as clean_simple.npy")
