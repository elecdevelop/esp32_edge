#!/usr/bin/env python3
"""
Proof of Concept: Complete DSP pipeline simulation
Simulates ESP32 test: 1kHz voice + 1.7kHz noise -> should output 1kHz only
"""

import numpy as np

SAMPLE_RATE = 16000.0
FFT_N = 256
HOP_SIZE = 64
HOP = HOP_SIZE // 2

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

def hamming_window(n):
    """Hamming window function"""
    return 0.54 - 0.46 * np.cos(2 * np.pi * np.arange(n) / (n - 1))

def test_overlap_add(voice_data, noise_data):
    """Full overlap-add spectral subtraction"""
    
    # Initialize Hann window (matches C code)
    s_hann = 0.5 * (1 - np.cos(2 * np.pi * np.arange(FFT_N) / FFT_N))
    s_overlap = np.zeros(HOP_SIZE)
    s_prev_voice = np.zeros(HOP_SIZE, dtype=np.int16)
    s_prev_noise = np.zeros(HOP_SIZE, dtype=np.int16)
    
    output = np.zeros(len(voice_data), dtype=np.int16)
    out_idx = 0
    
    # Process in hops
    while out_idx < len(voice_data):
        # Build overlapping frame: [prev | new] with windowing
        window_size = HOP_SIZE + HOP_SIZE
        
        n_prev = min(HOP_SIZE, out_idx)
        voice_frame = np.zeros(window_size, dtype=np.int16)
        noise_frame = np.zeros(window_size, dtype=np.int16)
        
        if n_prev > 0:
            voice_frame[:n_prev] = s_prev_voice[:n_prev]
            noise_frame[:n_prep] = s_prev_noise[:n_prev]
        
        if out_idx + HOP_SIZE <= len(voice_data):
            voice_frame[n_prev:] = voice_data[out_idx:out_idx + HOP_SIZE]
            noise_frame[n_prev:] = noise_data[out_idx:out_idx + HOP_SIZE]
        else:
            # Pad with zeros if near end
            voice_frame[n_prev:] = voice_data[out_idx:]
            noise_frame[n_prev:] = noise_data[out_idx:]
        
        # Forward FFT
        voice_fft = np.fft.rfft(voice_frame * s_hann[:window_size], n=FFT_N)
        noise_fft = np.fft.rfft(noise_frame * s_hann[:window_size], n=FFT_N)
        
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
        if out_idx + HOP_SIZE <= len(voice_data):
            output[out_idx:out_idx + HOP_SIZE] = np.clip(
                iFFT_result[:HOP_SIZE] + s_overlap, -32767, 32767
            ).astype(np.int16)
        else:
            output[out_idx:] = np.clip(
                iFFT_result[:len(output) - out_idx] + s_overlap, -32767, 32767
            ).astype(np.int16)
        
        # Update overlap buffer
        s_overlap = iFFT_result[HOP_SIZE:]
        
        # Update previous frame
        n_new = min(HOP_SIZE, len(voice_data) - out_idx)
        s_prev_voice[HOP_SIZE - n_new:] = s_prev_voice[:n_new]
        s_prev_voice[:n_new] = voice_data[out_idx:out_idx + n_new]
        s_prev_noise[HOP_SIZE - n_new:] = s_prev_noise[:n_new]
        s_prev_noise[:n_new] = noise_data[out_idx:out_idx + n_new]
        
        out_idx += HOP
    
    return output

if __name__ == "__main__":
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
    
    print(f"\nOutput saved test_output.npy for inspection")
