# Band-pass filter, AGC and limiter

The decoder's front end ahead of the tone detectors and the FM demodulator
(`decoder_process_sample()` in `src/decoder.cpp`). All stages are enabled;
the AGC can be bypassed with `sstv_decoder_set_agc_mode(dec, SSTV_AGC_OFF)`.
Measured filter responses are in
[the filter specifications](filters.md).

```text
x ─► clip ±24576 ─► (x[n]+x[n-1])/2 ─► BPF ─► CLVL AGC ─┬─► Hilbert FM demodulator
                                                        └─► ×32, clamp ±16384 ─► tone detectors
```

## Band-pass filter

Port of MMSSTV `CSSTVDEM::CalcBPF`, "wide" setting (`m_bpf = 1`):

```cpp
dec->bpftap = (int)(24.0 * sample_rate / 11025.0);          // 104 at 48 kHz, 24 at 11025 Hz
MakeFilter(hbpf,  bpftap, kFfBPF, sample_rate, 1100.0, 2600.0, 20.0, 1.0);
MakeFilter(hbpfs, bpftap, kFfBPF, sample_rate,  400.0, 2500.0, 20.0, 1.0);
```

- MMSSTV's low edge for HBPF is `m_SyncRestart ? 1100 : 1200`; `m_SyncRestart`
  defaults to 1, so 1100 Hz.
- att = 20 dB is below the Kaiser threshold (21 dB), so no window is applied:
  stop band ≈ −25…−28 dB, 50 Hz hum ≈ −16 dB (HBPFS).
- Filter selection, as MMSSTV (`m_Sync || m_SyncMode >= 3`): HBPFS (wide)
  while hunting for VIS/N-VIS, HBPF once an image is being decoded. Both share
  one delay line (`CFIR2`).
- Linear phase, group delay `bpftap / 2` samples (1.08 ms).

## Level AGC (`CLVL`)

Port of MMSSTV `CLVL` (`sstv.h`) in fast mode (`m_agcfast = 1`, as MMSSTV
sets it for the demodulator):

```cpp
void level_agc_do(level_agc_t *lvl, double d) {        // every sample
    lvl->m_Cur = d;
    if (d < 0.0) d = -d;
    if (lvl->m_Max < d) lvl->m_Max = d;
    lvl->m_Cnt++;
}

void level_agc_fix(level_agc_t *lvl) {                  // acts every 100 ms
    if (lvl->m_Cnt < lvl->m_CntMax) return;             // m_CntMax = fs × 0.1
    ...                                                 // slow-mode bookkeeping
    lvl->m_CurMax = lvl->m_Max;
    lvl->m_agc = (lvl->m_CurMax > 32) ? 16384.0 / lvl->m_CurMax
                                      : 16384.0 / 32.0;
    lvl->m_Max = 0.0;
}

double level_agc_apply(level_agc_t *lvl, double d) { return d * lvl->m_agc; }
```

- Every 100 ms the gain is set so that the previous 100 ms peak maps to 16384.
- Peaks at or below 32 get the maximum gain, 512.
- The gain starts at 1.0 and changes in steps, not smoothly.
- The AGC does not improve SNR; it only fixes the operating level.

Input scale: samples are expected on the 16-bit PCM scale. With the AGC on,
the absolute input level matters very little (up to the 512× gain limit).

## Limiter

```cpp
d = ad * 32.0;
if (d >  16384.0) d =  16384.0;
if (d < -16384.0) d = -16384.0;
```

After the AGC the signal peaks at 16384, so `×32` drives almost every sample
into the clamp. This is a hard limiter, as in FM receivers: the tone
detectors see a constant-amplitude signal whose zero crossings carry the
frequency. The detector thresholds (`s_lvl` = 2400 etc.) are calibrated for
this level.

The Hilbert demodulator uses the AGC output `ad`, not the limited signal.

## Debug taps

`sstv_decoder_enable_debug_wav()` writes the signal after the 2-tap average,
after the band-pass, and after the AGC. Its "final" output is the AGC output
× 2, not the limiter output (which would be a clipped square wave). See
[the debug WAV guide](../guide/debug-wav.md).
