# Implementation specifications

How *this library* works: the design of the encoder and decoder, the DSP they
use, and how faithfully each part follows MMSSTV. These documents are kept in
step with the code, and are the place to look before changing it.

For what the signal itself must contain, see
[the standards section](../standards/). For how to *use* the library, see
[the guide](../guide/).

## Encoder and decoder

| Document | Contents |
| --- | --- |
| [encoder.md](encoder.md) | Generation pipeline, segment queue, oscillator, code map, sample accounting, deliberate omissions |
| [decoder.md](decoder.md) | Front end, demodulator, VIS and N-VIS detection, image start, scan timing, colour conversion, sync re-lock and timing correction |
| [decoder-status.md](decoder-status.md) | What works, how it was verified, and the known limitations |
| [vis-code-table.md](vis-code-table.md) | The VIS code table and the test that checks every mode's code, bit pattern and parity |
| [vis-tone-frequencies.md](vis-tone-frequencies.md) | Why VIS is transmitted at 1100/1300 Hz but detected at 1080/1320 Hz |

## Signal processing

| Document | Contents |
| --- | --- |
| [filters.md](filters.md) | Every filter in the decoder, with measured responses |
| [bpf-and-agc.md](bpf-and-agc.md) | The band-pass filters, the CLVL level AGC and the limiter, as implemented |
| [dsp-primitives.md](dsp-primitives.md) | The DSP building blocks, their reference tests, and where the decoder uses them |
| [filter-verification.md](filter-verification.md) | Line-by-line comparison of `src/dsp_filters.cpp` against MMSSTV's `fir.cpp` |

## Provenance

| Document | Contents |
| --- | --- |
| [mmsstv-source-map.md](mmsstv-source-map.md) | Where each part of this library comes from in MMSSTV, with line numbers, what was not ported, and facts earlier documents got wrong |
