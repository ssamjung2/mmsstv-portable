# MMSSTV source map

Where each part of this library comes from in the original MMSSTV source, and
how faithfully it was ported. The MMSSTV source (LGPL) is expected at
`../mmsstv/`; its files are Shift-JIS encoded, so convert before searching:

```bash
iconv -c -f CP932 -t UTF-8 ../mmsstv/sstv.cpp > /tmp/sstv.cpp
```

Line numbers refer to that MMSSTV tree. Together with the SSTV Handbook
(`docs/sstv-handbook.pdf`), MMSSTV is the reference for this project.

## Transmitter

| MMSSTV | Location | Port | Status |
| --- | --- | --- | --- |
| Preamble `TMmsstv::OutHEAD` | `Main.cpp:6816` | `write_preamble` (`encoder.cpp`) | Ported (VOX setting 0: tone sequence) |
| VIS / 16-bit VIS / AVT header / narrow FSK header (TX start-up) | `Main.cpp:6936–7125` | `write_vis_header`, `vis_build_tones` (`vis.cpp`) | Ported |
| `CSSTVMOD::WriteFSK` | `sstv.cpp:2942` | `write_fsk` | Ported |
| Scan-line writers `LineR24` … `LineMC` | `Main.cpp:6088–6400` | `write_line_*` | Ported |
| Line dispatch (timing arguments per mode) | `Main.cpp:6606–6740` | `generate_next_line_segments` | Ported |
| `CSSTVMOD::Do`, VCO set-up `SetFreeFreq(1100)`, `SetGain(1200)` | `sstv.cpp:2777`, `2854` | `VCO` (`vco.cpp`), `freq_to_vco_input` | Ported (no TX band-pass, no per-colour gain) |
| `ColorToFreq`, `ColorToFreqNarrow`, `GetRY` | `ComLib.cpp:3491`, `3497`, `3650` | `color_to_freq`, `color_to_freq_narrow`, `get_ry` | Ported |
| CW ID, FSK callsign ID, MMV clips | `Main.cpp` | – | Not ported |

## Mode timing

| MMSSTV | Location | Port | Status |
| --- | --- | --- | --- |
| Mode enum `smR36 … smMC180` | `sstv.h:450` | `sstv_mode_t` (same order) | Ported |
| `CSSTVSET::SetSampFreq` (`m_KS`, `m_OF`, `m_OFP`, `m_SG`…, `m_KSS`) | `sstv.cpp:655` | `compute_mode_timing` (encoder), `SCAN_TIMING` (decoder) | Ported |
| `CSSTVSET::GetTiming` (line length, ms) | `sstv.cpp:1188` | `get_line_ms` (encoder), `tw_ms` (decoder) | Ported |
| `CSSTVSET::GetBitmapSize` / `GetPictureSize` | `sstv.cpp:607` | `mode_table` (`modes.cpp`) | Ported; Robot/AVT use 320×240 (MMSSTV: 320×256 bitmap, 240 picture lines) |
| Narrow constants `NARROW_SYNC/LOW/HIGH` | `sstv.h:440` | `encoder.cpp`, `hill_set_width` | Ported |

## Receiver

| MMSSTV | Location | Port | Status |
| --- | --- | --- | --- |
| Front end in `CSSTVDEM::Do`: 2-tap average, band-pass, `CLVL` AGC, ×32 limiter, tone detectors | `sstv.cpp:1819` | `decoder_process_sample` | Ported; input is clipped at ±24576 (MMSSTV only flags it) |
| Detector set-up (1080/1200/1320/1900 Hz, 2100 Hz FSK, 50 Hz LPFs) | `sstv.cpp:1446–1455` | `sstv_decoder_create` | Ported |
| `CSSTVDEM::CalcBPF` | `sstv.cpp:1522` | HBPF 1100–2600 Hz, HBPFS 400–2500 Hz | "Wide" setting only; `m_SyncRestart = 1` (`sstv.cpp:1486`) gives the 1100 Hz edge |
| `CLVL` | `sstv.h:223` | `level_agc_*` | Ported (fast mode) |
| VIS sync modes 0, 1, 2, 9 | `sstv.cpp:1889–2125` | VIS state machine | Ported with looser acceptance (see [decoder status](decoder-status.md)) |
| Post-VIS start (mode 3), AVT header lock (modes 4–8) | `sstv.cpp:2126–2245` | Fixed lead times in `decoder_allocate_image_buffer` | AVT header not decoded; start computed from the first VIS |
| `CSSTVDEM::DecodeFSK` | `sstv.cpp:2378` | `decoder_decode_nvis` | N-VIS path ported; callsign path (0x2A) not |
| `CHILL` Hilbert demodulator, `CHILL::SetWidth` | `sstv.cpp:3022` | `hill_do`, `hill_set_width` | Ported; PLL and zero-crossing demodulators not |
| `CSYNCINT` sync-interval trackers, AFC (`SyncFreq`, `g_dblToneOffset`) | `sstv.cpp` | `sync_tracker_*` (only init/inc used) | Not ported |
| Picture drawing `TMmsstv::DrawSSTVNormal`: `m_SyncPos` per-line sync search, channel mapping `x = ps × W / m_KSS`, Robot 36 `m_DSEL` | `Main.cpp:3715–3900` | `decoder_process_image_sample`, `lnd_flush_line` | Ported (per-line re-lock instead of MMSSTV's buffered redraw) |
| `YCtoRGB` | `ComLib.cpp:3475` | `yc_to_rgb` | Ported |

## DSP primitives (`fir.cpp`)

`CIIRTANK` (`fir.cpp:46`), `MakeFilter` (`:332`), `MakeHilbert` (`:432`),
`MakeIIR` (`:953`), `CIIR`, `CFIR2` and `DoFIR` are ported one-to-one in
`src/dsp_filters.cpp`; see
[the filter verification](filter-verification.md).
`CPLL`, `CFQC`, `CLMS` and `CFFT` are not ported.

## Facts that earlier documents got wrong

- **VIS tones**: MMSSTV transmits 1100 Hz = 1, 1300 Hz = 0 (`Main.cpp`,
  VIS loop). 1080/1320 Hz are only detector centres. See
[the VIS tone frequencies](vis-tone-frequencies.md).
- **Detector width**: `CIIRTANK::SetFreq(f, fs, 80)` means an 80 Hz
  bandwidth (Q ≈ 13.5), not Q = 80.
- **VIS length**: 910 ms (8 data bits including parity, no separate parity
  tone); 1150 ms for the 16-bit VIS. Not 640 or 940 ms.
- **Band-pass**: att = 20 dB gives a rectangular window (measured worst stop band about −25 to −28 dB),
  not 60 dB; HBPF starts at 1100 Hz, not 1080 Hz.
- **Limiter**: the ×32 scale after the AGC is a hard limiter, not a gain
  adjustment.
- **Martin timing**: `m_SG` is a receive offset (start of the second channel),
  not the red channel's scan time; all three Martin channels have equal length.
