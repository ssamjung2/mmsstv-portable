#!/usr/bin/env python3
"""
Quick script to analyze VIS code frequencies in a WAV file.
"""
import wave
import struct
import numpy as np
from scipy import signal

def analyze_vis(wav_path):
    """Analyze VIS code in WAV file."""
    with wave.open(wav_path, 'rb') as wav:
        fs = wav.getframerate()
        nchannels = wav.getnchannels()
        nframes = wav.getnframes()
        audio_bytes = wav.readframes(nframes)
        
   # Convert to numpy array 
    if nchannels == 1:
        audio = np.frombuffer(audio_bytes, dtype=np.int16).astype(np.float32)
    else:
        audio = np.frombuffer(audio_bytes, dtype=np.int16).astype(np.float32)
        audio = audio[::nchannels]  # Take left channel
        
    # Normalize
    audio /= 32768.0
    
    # Look at first 1 second (should contain VIS)
    duration = min(1.0, len(audio) / fs)
    samples = int(duration * fs)
    audio = audio[:samples]
    
    # Design bandpass filter for SSTV range
    sos = signal.butter(4, [1000, 2400], btype='band', fs=fs, output='sos')
    filtered = signal.sosfilt(sos, audio)
    
    # analyze in 30ms windows (VIS bit duration)
    window_samples = int(0.030 * fs)
    hop = window_samples // 2
    
    print(f"Sample rate: {fs} Hz")
    print(f"Window: {window_samples} samples = 30ms")
    print(f"\nTime(s)   Freq(Hz)  Energy")
    print("-" * 40)
    
    for i in range(0, len(filtered) - window_samples, hop):
        window = filtered[i:i+window_samples]
        # FFT
        fft = np.fft.rfft(window * np.hanning(len(window)))
        freqs = np.fft.rfftfreq(len(window), 1/fs)
        magnitudes = np.abs(fft)
        
        # Find peak in 1000-2400 Hz range
        mask = (freqs >= 1000) & (freqs <= 2400)
        if np.any(mask):
            peak_idx = np.argmax(magnitudes[mask])
            peak_freq = freqs[mask][peak_idx]
            peak_mag = magnitudes[mask][peak_idx]
            
            time_s = i / fs
            if peak_mag > 10:  # Threshold
                print(f"{time_s:6.3f}  {peak_freq:7.1f}  {peak_mag:8.1f}")

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: analyze_vis_wav.py <wav_file>")
        sys.exit(1)
    analyze_vis(sys.argv[1])
