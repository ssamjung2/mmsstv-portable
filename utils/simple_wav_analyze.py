#!/usr/bin/env python3
"""
Simple WAV frequency analyzer without numpy
Uses Goertzel algorithm for specific frequencies
"""
import sys
import wave
import struct
import math

def goertzel(samples, target_freq, sample_rate):
    """
    Goertzel algorithm to detect energy at a specific frequency
    """
    k = int(0.5 + (len(samples) * target_freq / sample_rate))
    omega = (2.0 * math.pi * k) / len(samples)
    cosine = math.cos(omega)
    sine = math.sin(omega)
    coeff = 2.0 * cosine
    
    q0, q1, q2 = 0.0, 0.0, 0.0
    
    for sample in samples:
        q0 = coeff * q1 - q2 + sample
        q2 = q1
        q1 = q0
    
    real = q1 - q2 * cosine
    imag = q2 * sine
    magnitude = math.sqrt(real * real + imag * imag)
    
    return magnitude / len(samples)

def analyze_wav_segment(filename):
    """Analyze VIS timing in WAV file"""
    with wave.open(filename, 'rb') as wav:
        framerate = wav.getframerate()
        n_channels = wav.getnchannels()
        sampwidth = wav.getsampwidth()
        n_frames = wav.getnframes()
        
        print(f"WAV: {framerate} Hz, {n_channels} ch, {sampwidth} B/sample, {n_frames/framerate:.1f} sec\n")
        
        # Read first 600ms (covers leader + VIS)
        samples_to_read = int(0.6 * framerate)
        audio_bytes = wav.readframes(min(samples_to_read, n_frames))
    
    # Convert to samples
    if sampwidth == 2:
        fmt = 'h' * (len(audio_bytes) // 2)
        samples = struct.unpack(fmt, audio_bytes)
    else:
        print(f"Unsupported sample width: {sampwidth}")
        return
    
    # If stereo, take only left channel
    if n_channels == 2:
        samples = samples[::2]
    
    # Normalize
    samples = [s / 32768.0 for s in samples]
    
    # Analyze segments
    segments = [
        ("Leader (0-150ms)", 0.0, 0.15, [1900, 1200, 1100, 1300]),
        ("Leader (150-300ms)", 0.15, 0.30, [1900, 1200, 1100, 1300]),
        ("Break (300-310ms)", 0.30, 0.31, [1200, 1100, 1300, 1900]),
        ("Start bit (310-340ms)", 0.31, 0.34, [1200, 1100, 1300]),
        ("Data bit 0 (340-370ms)", 0.34, 0.37, [1100, 1300, 1200]),
        ("Data bit 1 (370-400ms)", 0.37, 0.40, [1100, 1300, 1200]),
        ("Data bit 2 (400-430ms)", 0.40, 0.43, [1100, 1300, 1200]),
        ("Data bit 3 (430-460ms)", 0.43, 0.46, [1100, 1300, 1200]),
        ("Data bit 4 (460-490ms)", 0.46, 0.49, [1100, 1300, 1200]),
        ("Data bit 5 (490-520ms)", 0.49, 0.52, [1100, 1300, 1200]),
        ("Data bit 6 (520-550ms)", 0.52, 0.55, [1100, 1300, 1200]),
        ("Data bit 7 (550-580ms)", 0.55, 0.58, [1100, 1300, 1200]),
    ]
    
    print("Expected Robot36 VIS (0x88 = 00010001 LSB-first):")
    print("  Bits: 0=1300Hz, 0=1300Hz, 0=1300Hz, 1=1100Hz, 0=1300Hz, 0=1300Hz, 0=1300Hz, 1=1100Hz\n")
    print(f"{'Segment':<25} {'1100Hz':>8} {'1300Hz':>8} {'1200Hz':>8} {'1900Hz':>8} {'Dominant':>10}")
    print("=" * 80)
    
    for name, t_start, t_end, freqs in segments:
        start_idx = int(t_start * framerate)
        end_idx = int(t_end * framerate)
        seg = samples[start_idx:end_idx]
        
        if len(seg) < 10:
            continue
        
        # Calculate energy at each frequency
        energies = {}
        for freq in freqs:
            energies[freq] = goertzel(seg, freq, framerate)
        
        # Find dominant
        dominant_freq = max(energies, key=energies.get)
        dominant_energy = energies[dominant_freq]
        
        # Print results
        e1100 = energies.get(1100, 0.0)
        e1300 = energies.get(1300, 0.0)
        e1200 = energies.get(1200, 0.0)
        e1900 = energies.get(1900, 0.0)
        
        print(f"{name:<25} {e1100:8.4f} {e1300:8.4f} {e1200:8.4f} {e1900:8.4f} {dominant_freq:7d} Hz")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 simple_wav_analyze.py <sstv.wav>")
        sys.exit(1)
    
    analyze_wav_segment(sys.argv[1])
