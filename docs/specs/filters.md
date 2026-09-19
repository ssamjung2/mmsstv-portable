# Decoder filter specifications

Parameters and **measured** responses of the filters the decoder uses
(`src/dsp_filters.cpp`, configured in `src/decoder.cpp`). The responses below
were measured on 2026-09-18 by driving each filter with a steady sine and
reading the settled output amplitude, at 48 kHz and 11025 Hz. The two rates
agree within about 1 dB (below 4 kHz); the 48 kHz figures are shown.

Where the filters sit in the pipeline is described in
[the decoder specification](decoder.md).

## Summary

| Filter | Type | Parameters | Used for |
| --- | --- | --- | --- |
| 2-tap average | FIR | `(x[n] + x[n−1]) / 2` | Input smoothing |
| HBPFS | Windowed-sinc FIR BPF | 400–2500 Hz, `24·fs/11025` taps, att 20 dB | Before an image starts |
| HBPF | Windowed-sinc FIR BPF | 1100–2600 Hz, same taps | During an image |
| iir11 / iir13 | `CIIRTANK` resonator | 1080 Hz / 1320 Hz, **80 Hz bandwidth** | VIS bit decision |
| iir12 | `CIIRTANK` | 1200 Hz, 100 Hz bandwidth | Sync / VIS start |
| iir19 | `CIIRTANK` | 1900 Hz, 100 Hz bandwidth | Leader; narrow-mode sync and FSK mark |
| iirfsk | `CIIRTANK` | 2100 Hz, 100 Hz bandwidth | Narrow-mode FSK space |
| lpf11…19, lpffsk | `CIIR` Butterworth | 50 Hz, 2nd order | Envelope smoothing after rectification |
| Hilbert FIR | `MakeHilbert` | `round(12·fs/11025)` taps (even, ≥ 6), 100 Hz … fs/2 − 100 Hz | FM demodulator |
| Demodulator LPF | `CIIR` Butterworth | 1800 Hz, 3rd order | Pixel signal smoothing |

## Band-pass filters

`MakeFilter(ffBPF, fcl, fch, att = 20, gain = 1)`, as MMSSTV's "wide"
setting. Because att < 21 dB, the Kaiser window is not applied (α = 0): the
taps are a plain truncated sinc, so the transition bands are wide and the
stop band is shallow. Group delay is `taps / 2` samples (≈ 1.08 ms at any
rate). The taps are normalised to unit DC gain of the low-pass prototype, so
the pass band ripples by about ±1 dB.

Measured gain (dB), 104 taps at 48 kHz:

| Hz | 50 | 200 | 400 | 700 | 1000 | 1100 | 1200 | 1500 | 1900 | 2300 | 2500 | 2600 | 3000 | 4000 | 6000 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HBPF 1100–2600 | −35.7 | −34.5 | −41.7 | −23.2 | −11.0 | −5.9 | −2.6 | +1.0 | −0.1 | +0.6 | −2.0 | −5.1 | −18.1 | −25.6 | −41.8 |
| HBPFS 400–2500 | −16.4 | −24.2 | −5.0 | +0.8 | +0.8 | +0.6 | +0.4 | +0.3 | +1.0 | −0.5 | −5.1 | −9.6 | −18.9 | −28.5 | −40.7 |

Worst gain between 4 kHz and fs/2: HBPF −25.6 dB, HBPFS −27.4 dB
(11025 Hz: −26.9 / −27.1 dB).

Consequences:

- Mains hum is only weakly rejected before sync (HBPFS: −16 dB at 50 Hz).
  The limiter and the narrow resonators do most of the work.
- HBPF attenuates the 1200 Hz sync by 2.6 dB, which the following AGC and
  limiter remove.
- MMSSTV's "middle" and "narrow" settings (64 and 96 taps at 11025 Hz,
  att 40/50 dB) are not implemented.

## Tone detectors (resonators)

`CIIRTANK::SetFreq(f0, fs, bw)`:
`b1 = 2·e^(−π·bw/fs)·cos(2π·f0/fs)`, `b2 = −e^(−2π·bw/fs)`,
`a0 = sin(2π·f0/fs) / ((fs/6)/bw)`. The third argument is the **bandwidth in
Hz** (Q = f0/bw: 13.5 for 1080/80, 16.5 for 1320/80, 12 for 1200/100).

Measured response relative to each filter's own peak (dB), 48 kHz:

| Detector | 1080 | 1100 | 1200 | 1300 | 1320 | 1500 | 1900 | 2100 | 2300 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| iir11 1080/80 | 0.0 | −1.0 | −10.5 | −15.8 | −16.6 | −22.0 | −29.0 | −31.5 | −33.5 |
| iir12 1200/100 | −7.9 | −6.6 | 0.0 | −7.3 | −8.7 | −16.7 | −25.1 | −27.8 | −30.1 |
| iir13 1320/80 | −14.9 | −14.2 | −9.6 | −0.9 | 0.0 | −13.8 | −24.9 | −28.0 | −30.5 |
| iir19 1900/100 | −22.2 | −22.1 | −21.2 | −20.1 | −19.9 | −17.2 | 0.0 | −12.7 | −19.0 |
| iirfsk 2100/100 | −23.8 | −23.7 | −23.1 | −22.3 | −22.1 | −20.3 | −11.9 | 0.0 | −12.7 |

For the transmitted VIS tones: at 1100 Hz, iir11 is 13.2 dB above iir13; at
1300 Hz, iir13 is 14.9 dB above iir11. The 20 Hz offset between the
transmitted tones (1100/1300) and the detector centres (1080/1320) costs
about 1 dB.

The peak gain `a0` is not normalised: the raw output level depends on
`bw` and `f0`, which is why the thresholds (`s_lvl` etc.) are empirical.

## Envelope low-pass

2nd-order Butterworth at 50 Hz (`MakeIIR(50, fs, 2, 0, 0)`, bilinear
transform). Measured: −0.3 dB at 25 Hz, −0.8 dB at 33 Hz (the VIS bit rate),
−3.0 dB at 50 Hz, −12.3 dB at 100 Hz, −24.1 dB at 200 Hz. Together with the
resonator this delays the envelope by about 9.5 ms, so a VIS bit sampled
24.5 ms after it starts (see the decoder document) reflects roughly the
middle of the bit.

## Level AGC and limiter

Not filters, but they set the level every detector sees: `CLVL` scales the
band-passed signal so its 100 ms peak is 16384 (gain limited to 512), then
`×32` with clamping at ±16384 hard-limits it. See
[the band-pass and AGC guide](bpf-and-agc.md).

## Possible improvements (not implemented)

- MMSSTV's middle/narrow band-pass settings, or a VIS-only narrow band-pass.
- AFC (retuning all detectors to a measured tone offset), as MMSSTV does via
  `g_dblToneOffset`.
- Correlating the whole VIS pattern instead of deciding bit by bit.
