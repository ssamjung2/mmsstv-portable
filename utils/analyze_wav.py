#!/usr/bin/env python3
"""
Analyze SSTV WAV file VIS section to see actual frequencies
"""
import sys
import struct
import wave
import numpy as np
from scipy import signal
from scipy.fft import fft, fftfreq

def analyze_wav_vis(filename):
    """Analyze VIS section of SSTV WAV"""
    with wave.open(filename, 'rb') as wav:
        framerate = wav.getframerate()
        n_channels = wav.getnchannels()
        sampwidth = wav.getsampwidth()
        n_frames = wav.getnframes()
        
        print(f"WAV Info: {framerate} Hz, {n_channels} ch, {sampwidth} bytes/sample, {n_frames/framerate:.1f} sec")
        
        # Read all audio
        audio_bytes = wav.readframes(n_frames)
        
    # Convert to numpy array
    if sampwidth == 2:
        audio = np.frombuffer(audio_bytes, dtype=np.int16)
    else:
        print(f"Unsupported sample width: {sampwidth}")
        return
        
    # Convert to mono if stereo
    if n_channels == 2:
        audio = audio.reshape(-1, 2).mean(axis=1)
    
    # Normalize to [-1, 1]
    audio = audio.astype(float) / 32768.0
    
    # VIS typically starts after ~300ms leader + ~10ms break = ~310ms
    # Analyze first 500ms to capture leader + VIS
    analysis_samples = int(0.5 * framerate)
    segment = audio[:analysis_samples]
    
    # Compute spectrogram
    f, t, Sxx = signal.spectrogram(segment, framerate, nperseg=512, noverlap=256)
    
    # Find dominant frequencies in 1000-1500 Hz range (VIS band)
    vis_band_idx = (f >= 1000) & (f <= 1500)
    vis_freqs = f[vis_band_idx]
    vis_power = Sxx[vis_band_idx, :]
    
    # For each time slice, find peak frequency
    print(f"\nTime-frequency analysis (first 500ms):")
    print(f"{'Time(ms)':>8} {'Peak Freq':>10} {'Power':>10}")
    print("-" * 30)
    
    for i in range(min(20, len(t))):  # First 20 time slices
        peak_idx = np.argmax(vis_power[:, i])
        peak_freq = vis_freqs[peak_idx]
        peak_power = vis_power[peak_idx, i]
        print(f"{t[i]*1000:8.1f} {peak_freq:10.1f} {peak_power:10.4e}")
    
    # Analyze specific time windows for VIS data bits
    # Leader: 0-300ms (should be ~1900 Hz)
    # Break: 300-310ms (should be ~1200 Hz)
    # Start bit: 310-340ms (should be ~1200 Hz)
    # Data bit 0: 340-370ms (should be 1100 or 1300 Hz)
    # Data bit 1: 370-400ms
    # etc.
    
    print(f"\n\nAnalyzing specific VIS segments:")
    segments = [
        ("Leader (0-300ms)", 0.0, 0.3, [1900]),
        ("Break (300-310ms)", 0.3, 0.31, [1200]),
        ("Start bit (310-340ms)", 0.31, 0.34, [1200]),
        ("Data bit 0 (340-370ms)", 0.34, 0.37, [1100, 1300]),
        ("Data bit 1 (370-400ms)", 0.37, 0.40, [1100, 1300]),
        ("Data bit 2 (400-430ms)", 0.40, 0.43, [1100, 1300]),
        ("Data bit 3 (430-460ms)", 0.43, 0.46, [1100, 1300]),
    ]
    
    for name, t_start, t_end, expected_freqs in segments:
        start_idx = int(t_start * framerate)
        end_idx = int(t_end * framerate)
        seg = audio[start_idx:end_idx]
        
        # FFT to find dominant frequency
        N = len(seg)
        yf = fft(seg)
        xf = fftfreq(N, 1/framerate)[:N//2]
        power = 2.0/N * np.abs(yf[:N//2])
        
        # Find peaks in expected frequency range
        range_idx = (xf >= 1000) & (xf <= 2000)
        peak_idx = np.argmax(power[range_idx])
        peak_freq = xf[range_idx][peak_idx]
        peak_power = power[range_idx][peak_idx]
        
        # Check energy at expected frequencies
        expected_str = ", ".join([f"{freq} Hz" for freq in expected_freqs])
        print(f"\n{name} (expect {expected_str}):")
        print(f"  Dominant: {peak_freq:.1f} Hz (power={peak_power:.4f})")
        
        for exp_freq in expected_freqs:
            # Find closest frequency bin
            closest_idx = np.argmin(np.abs(xf - exp_freq))
            energy = power[closest_idx]
            print(f"  Energy at {exp_freq} Hz: {energy:.4f}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_wav.py <sstv.wav>")
        sys.exit(1)
    
    analyze_wav_vis(sys.argv[1])
