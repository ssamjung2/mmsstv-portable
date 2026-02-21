#!/usr/bin/env python3
"""
Statistical noise and SNR analysis for SSTV DSP pipeline
Compares 'clean' and 'dnr' WAV files for RMS noise and SNR improvement
"""
import sys
import wave
import numpy as np

def load_wav_mono(filename):
    with wave.open(filename, 'rb') as wav:
        n_channels = wav.getnchannels()
        sampwidth = wav.getsampwidth()
        n_frames = wav.getnframes()
        audio_bytes = wav.readframes(n_frames)
        if sampwidth == 2:
            audio = np.frombuffer(audio_bytes, dtype=np.int16)
        else:
            raise ValueError(f"Unsupported sample width: {sampwidth}")
        if n_channels == 2:
            audio = audio.reshape(-1, 2).mean(axis=1)
        audio = audio.astype(float) / 32768.0
    return audio

def rms(x):
    return np.sqrt(np.mean(x**2))

def snr(signal, noise):
    signal_power = np.mean(signal**2)
    noise_power = np.mean(noise**2)
    return 10 * np.log10(signal_power / noise_power)

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 analyze_noise.py <clean_wav> <dnr_wav>")
        sys.exit(1)
    clean = load_wav_mono(sys.argv[1])
    dnr = load_wav_mono(sys.argv[2])
    # Use central region for analysis (avoid edges)
    N = min(len(clean), len(dnr))
    start = int(N * 0.25)
    end = int(N * 0.75)
    clean_seg = clean[start:end]
    dnr_seg = dnr[start:end]
    # RMS noise
    rms_clean = rms(clean_seg)
    rms_dnr = rms(dnr_seg)
    # Difference signal (residual noise removed by DNR)
    diff = clean_seg - dnr_seg
    rms_diff = rms(diff)
    # SNR improvement (treat clean as signal, diff as noise removed)
    snr_before = snr(clean_seg, diff)
    snr_after = snr(dnr_seg, diff)
    print(f"RMS (clean): {rms_clean:.6f}")
    print(f"RMS (dnr):   {rms_dnr:.6f}")
    print(f"RMS (diff):  {rms_diff:.6f}")
    print(f"SNR before DNR: {snr_before:.2f} dB")
    print(f"SNR after DNR:  {snr_after:.2f} dB")
    print(f"DNR RMS reduction: {100*(1 - rms_dnr/rms_clean):.2f}%")

if __name__ == "__main__":
    main()
