# VIS tone frequencies: 1100/1300 Hz vs 1080/1320 Hz

**Answer:** MMSSTV *transmits* VIS data bits at the standard frequencies,
**1100 Hz = 1 and 1300 Hz = 0**. The values 1080 Hz and 1320 Hz appear only as
the centres of MMSSTV's *receive* tone detectors. The two are compatible and
there is no MMSSTV-specific tone standard. This library does the same:
the encoder sends 1100/1300 Hz and the decoder's detectors sit at 1080/1320 Hz.

## Evidence

| Source | What it shows |
| --- | --- |
| MMSSTV `Main.cpp`, VIS transmit loop | `mp->Write(short(d & 0x0001 ? 1100 : 1300), 30);` for each of the 8 (or 16) bits, LSB first. |
| MMSSTV `sstv.cpp`, `CSSTVMOD` | VCO set with `SetFreeFreq(1100)`, `SetGain(2300 - 1100)`; the transmitter can produce any tone from 1100 Hz up. |
| MMSSTV `sstv.cpp`, `CSSTVDEM` | `m_iir11.SetFreq(1080, SampFreq, 80.0)`, `m_iir13.SetFreq(1320, SampFreq, 80.0)`; bit = `d11 > d13`. |
| MMSSTV `fir.cpp`, `CIIRTANK::SetFreq(f, smp, bw)` | The third argument is a bandwidth in Hz: pole radius `exp(−π·bw/smp)`. |
| SSTV Handbook §3.6.2 | "The frequency 1300 Hz means the state of logical zero and 1100 Hz logical one." |

## Why 1080/1320 Hz detectors work with 1100/1300 Hz tones

The detectors are resonators with an **80 Hz** bandwidth, i.e. Q ≈ 13.5
(1080/80) and ≈ 16.5 (1320/80), not Q = 80. A 1100 Hz tone is 20 Hz from
the 1080 Hz centre and 220 Hz from the 1320 Hz centre, so `d11` is clearly
larger than `d13`; likewise for 1300 Hz. The decision only compares the two
detectors, so the 20 Hz offset costs very little. (MMSSTV does not document
why it chose 1080/1320 Hz.)

Test evidence in this repository:

- `tests/test_vis_decode.c` synthesises VIS at 1080/1320 Hz and
  `tests/test_roundtrip.cpp` encodes it at 1100/1300 Hz; the decoder
  identifies both.
- The recordings in `tests/audio/` decode correctly.

## History

Earlier documents in this folder (for example
[MMSSTV_ARCHITECTURE_ANALYSIS.md](MMSSTV_ARCHITECTURE_ANALYSIS.md) before it
was rewritten, [DECODER_STATUS.md](DECODER_STATUS.md) before 2026-09-18, and
several VIS reports) concluded that MMSSTV transmits at 1080/1320 Hz and that
the detectors have Q = 80 (13–17 Hz bandwidth). Both were wrong. Following
that conclusion, the encoder was changed to transmit 1080/1320 Hz; it was
changed back to 1100/1300 Hz on 2026-09-18.
