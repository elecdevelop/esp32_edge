#!/usr/bin/env python3
"""
Proof of Concept: Complete DSP pipeline simulation
Simulates ESP32 test: 1kHz voice + 1.7kHz noise -> should output 1kHz only
"""

import numpy as np

SAMPLE_RATE = 16000.0
FFT_N = 256
HOP_SIZE = 64

VOICE_FREQ = 1000.0
NOISE_FREQ = 1700.0
VOICE_AMP = 10000.0
NOISE_AMP = 5000.0
ALPHA = 1.0
BETA = 0.02

def generate_signals(duration=5):
    """Generate test signals: noise mic and voice mic"""
    t = np.arange(int(SAMPLE_RATE * duration)) / SAMPLE_RATE
    noise = NOISE_AMP * np.sin(2 * np.pi * NOISE_FREQ * t)
    voice = VOICE_AMP * np.sin(2 * np.pi * VOICE_FREQ * t) + NOISE_AMP * np.sin(2 * np.pi * NOISE_FREQ * t)
    return noise.astype(np.int16), voice.astype(np.int16)

def test_overlap_add(voice_data, noise_data):
    """Simplified overlap-add spectral subtraction - follows C code exactly"""
    
    # Initialize Hann window
    s_hann = 0.5 * (1 - np.cos(2 * np.pi * np.arange(FFT_N) / FFT_N))
    s_overlap = np.zeros(HOP_SIZE)
    s_prev_voice = np.zeros(HOP_SIZE, dtype=np.int16)
    s_prev_noise = np.zeros(HOP_SIZE, dtype=np.int16)
    
    output = np.zeros(0, dtype=np.int16)
    out_idx = 0
    
    # Process in hops
    HOP = HOP_SIZE // 2
    
    while out_idx < len(voice_data) - HOP:
        # Total frame size: prev + new = 64 + 64 = 128
        voice_frame = np.zeros(FFT_N, dtype=np.float32)
        noise_frame = np.zeros(FFT_N, dtype=np.float32)
        
        # Fill first half with previous frame
        voice_frame[:HOP_SIZE] = voice_data[out_idx - HOP_SIZE:out_idx].astype(np.float32)
        voice_frame[:HOP_SIZE] *= s_hann[:HOP_SIZE]
        
        noise_frame[:HOP_SIZE] = noise_data[out_idx - HOP_SIZE:out_idx].astype(np.float32)
        noise_frame[:HOP_SIZE] *= s_hann[:HOP_SIZE]
        
        # Fill second half with current hop
        voice_frame[HOP_SIZE:FFT_N] = voice_data[out_idx:out_idx + HOP_SIZE].astype(np.float32)
        voice_frame[HOP_SIZE:FFT_N] *= s_hann[HOP_SIZE:FFT_N]
        
        noise_frame[HOP_SIZE:FFT_N] = noise_data[out_idx:out_idx + HOP_SIZE].astype(np.float32)
        noise_frame[HOP_SIZE:FFT_N] *= s_hann[HOP_SIZE:FFT_N]
        
        # Forward FFT
        voice_fft = np.fft.rfft(voice_frame)
        noise_fft = np.fft.rfft(noise_frame)
        
        # Spectral subtraction
        voice_mag2 = np.abs(voice_fft) ** 2
        noise_mag2 = np.abs(noise_fft) ** 2
        clean_mag2 = voice_mag2 - ALPHA * noise_mag2
        clean_mag2 = np.maximum(clean_mag2, BETA * voice_mag2)
        
        scale = np.sqrt(clean_mag2 / (voice_mag2 + 1e-10))
        voice_fft_scaled = voice_fft * scale
        
        # Inverse FFT
        iFFT_result = np.fft.irfft(voice_fft_scaled, n=FFT_N)
        
        # Overlap-add output
        current_out_samples = min(HOP, len(output) - out_idx + HOP)
        for i in range(min(HOP, len(output) - out_idx)):
            val = iFFT_result[i] + s_overlap[i]
            output = np.append(output, np.clip(val, -32767, 32767).astype(np.int16))
        
        # Update overlap buffer
        s_overlap = iFFT_result[HOP_SIZE - HOP:HOP_SIZE]
        
        # Update previous frame
        s_prev_voice[:HOP_SIZE] = voice_data[out_idx - HOP_SIZE:out_idx]
        s_prev_noise[:HOP_SIZE] = noise_data[out_idx - HOP_SIZE:out_idx]
        
        out_idx += HOP
    
    return output

if __name__ == "__main__":
    try:
        noise_data, voice_data = generate_signals(duration=5)
        print(f"Generated {len(voice_data)} samples ({len(voice_data)/SAMPLE_RATE:.1f}s)")
        
        result = test_overlap_add(voice_data, noise_data)
        
        # FFT analysis
        result_fft = np.fft.rfft(result, n=1024)
        freqs = np.fft.rfftfreq(1024, SAMPLE_RATE)
        
        print("\nFrequency domain analysis (dB):")
        for freq in [0, 1000, 1700, 2000]:
            idx = int(freq * 2 / SAMPLE_RATE * 1024)
            if idx < len(freqs):
                db = 20 * np.log10(np.abs(result_fft[idx]) + 1e-10)
                print(f"  {freq:4d} Hz: {db:7.1f} dB")
        
        print(f"\nOutput array length: {len(result)}")
    except Exception as e:
        print(f"Error: {e}")
