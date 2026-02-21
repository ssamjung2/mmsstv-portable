#!/usr/bin/env python3
import wave
import numpy as np

# Read the WAV file
with wave.open("tests/audio/alt_color_bars_320x240 - Martin1.wav", 'rb') as f:
    sample_rate = f.getframerate()
    n_frames = f.getnframes()
    print(f"Sample rate: {sample_rate} Hz")
    print(f"Total frames: {n_frames}")
    
    # Read a chunk from the middle (skip some silence)
    f.setpos(int(sample_rate * 0.5))  # Skip 0.5 seconds
    frames = f.readframes(int(sample_rate * 1.0))  # Read 1 second
    
    # Convert to numpy array
    audio = np.frombuffer(frames, dtype=np.int16)
    print(f"Audio chunk shape: {audio.shape}")
    
    # Apply FFT to get frequency content
    freqs = np.fft.rfftfreq(len(audio), 1.0 / sample_rate)
    fft = np.abs(np.fft.rfft(audio))
    
    # Find peaks around 1100 Hz and 1300 Hz
    mark_idx = np.argmin(np.abs(freqs - 1100))
    space_idx = np.argmin(np.abs(freqs - 1300))
    sync_idx = np.argmin(np.abs(freqs - 1900))
    
    print(f"1100 Hz (mark) energy: {fft[mark_idx]:.0f}")
    print(f"1300 Hz (space) energy: {fft[space_idx]:.0f}")
    print(f"1900 Hz (sync) energy: {fft[sync_idx]:.0f}")
    
    # Find top 10 peaks
    top_peaks = np.argsort(fft)[-15:][::-1]
    print("\nTop frequency peaks (< 3kHz):")
    for idx in top_peaks:
        if freqs[idx] < 3000:  # Only up to 3kHz
            print(f"  {freqs[idx]:.0f} Hz: {fft[idx]:.0f}")
