# Documentation index

Last checked against the code: 2026-09-18.

The documents fall into two groups. **Reference documents** describe the code
as it is and are kept up to date. **Historical records** are plans, session
notes and reports from earlier stages of the project; each starts with a
banner listing what in it is now wrong. When a historical record disagrees
with a reference document or the code, the reference document is right.

The authorities behind the reference documents are the MMSSTV source
(`../mmsstv/`, see [MMSSTV_ARCHITECTURE_ANALYSIS.md](MMSSTV_ARCHITECTURE_ANALYSIS.md))
and the SSTV Handbook ([sstv-handbook.pdf](sstv-handbook.pdf); its LaTeX
source is in `sstv-handbook/`).

New to the project? Start with [START_HERE.md](START_HERE.md).

## Reference documents

### Overview

| Document | Contents |
| --- | --- |
| [README.md](README.md) | What the library does, the 43 modes, build options, encoder and decoder usage, command-line tools |
| [../tests/README.md](../tests/README.md) | Building and running the tests, what each test checks, latest results, fixtures |
| [PRODUCTION_READY_PLAN.md](PRODUCTION_READY_PLAN.md) | Roadmap to a first release, with a status table of what is done and what is open |

### Encoder

| Document | Contents |
| --- | --- |
| [ENCODER.md](ENCODER.md) | Encoder pipeline, preamble, VIS / 16-bit VIS / AVT / narrow (N-VIS) headers, pixel-to-tone mapping, line structure per mode family, sample accounting |
| [FREQUENCY_ANALYSIS.md](FREQUENCY_ANALYSIS.md) | Why VIS is sent at 1100/1300 Hz while the receiver's detectors sit at 1080/1320 Hz |
| [VIS_TEST_SUITE_REPORT.md](VIS_TEST_SUITE_REPORT.md) | The VIS code table test (`test_vis_codes`), VIS format, parity rules |

### Decoder

| Document | Contents |
| --- | --- |
| [DECODER_ARCHITECTURE_BASELINE.md](DECODER_ARCHITECTURE_BASELINE.md) | How the decoder works: front end, demodulator, VIS and N-VIS detection, image start, scan timing, colour conversion, sync re-lock and timing correction, API |
| [DECODER_STATUS.md](DECODER_STATUS.md) | What works, how it was verified, known limitations |
| [BPF_AGC_IMPLEMENTATION_GUIDE.md](BPF_AGC_IMPLEMENTATION_GUIDE.md) | Band-pass filters, CLVL AGC, limiter, debug taps |
| [DEBUG_WAV_GUIDE.md](DEBUG_WAV_GUIDE.md) | Writing intermediate signals to WAV files with `decode_wav_debug` |

### DSP and filters

| Document | Contents |
| --- | --- |
| [FILTER_SPECIFICATIONS.md](FILTER_SPECIFICATIONS.md) | Every filter the decoder uses, with measured responses |
| [FILTER_IMPLEMENTATION_VERIFICATION.md](FILTER_IMPLEMENTATION_VERIFICATION.md) | Line-by-line comparison of `src/dsp_filters.cpp` with MMSSTV's `fir.cpp` |
| [DSP_CONSOLIDATED_GUIDE.md](DSP_CONSOLIDATED_GUIDE.md) | The DSP primitives, their tests (`test_dsp_reference`) and where the decoder uses them |

### MMSSTV and SSTV background

| Document | Contents |
| --- | --- |
| [MMSSTV_ARCHITECTURE_ANALYSIS.md](MMSSTV_ARCHITECTURE_ANALYSIS.md) | Where each part of the library comes from in MMSSTV (with line numbers), what is not ported, facts earlier documents got wrong |
| [AVT.md](AVT.md) | SSTV Handbook text on the AVT modes, with a note on what this library implements |
| [Wraase SC-2.md](Wraase%20SC-2.md) | SSTV Handbook text on the Wraase SC-2 modes, with a note on how MMSSTV's timing differs |
| [sstv-handbook.pdf](sstv-handbook.pdf) | The SSTV Handbook |

### Testing under HF conditions

| Document | Contents |
| --- | --- |
| [HF_IMPAIRMENTS_TEST_GUIDE.md](HF_IMPAIRMENTS_TEST_GUIDE.md) | The HF impairment test program: what it simulates and the files it writes |
| [S_UNIT_DBM_REFERENCE.md](S_UNIT_DBM_REFERENCE.md) | S-meter units, dBm and the noise levels used by the HF test |

## Historical records

Not maintained. Read them for history only; the banner at the top of each
lists the corrections that matter.

| Document | Written | What it was |
| --- | --- | --- |
| [PORTING_ANALYSIS.md](PORTING_ANALYSIS.md) | 2026-01-28 | First analysis of MMSSTV and full porting plan |
| [ENCODING_ONLY_PLAN.md](ENCODING_ONLY_PLAN.md) | 2026-01-28 | Encoder-only porting plan with phase tracking |
| [SESSION_HANDOFF_SUMMARY.md](SESSION_HANDOFF_SUMMARY.md) | 2026-01-28 | Session hand-off during encoder work |
| [SESSION_COMPLETION.md](SESSION_COMPLETION.md) | 2026-01-30 | Encoder session wrap-up |
| [PROJECT_COMPLETION_SUMMARY.md](PROJECT_COMPLETION_SUMMARY.md) | 2026-01-30 | Encoder completion summary |
| [NEXT_STEPS.md](NEXT_STEPS.md) | 2026-01-30 | Plan for validating the encoder with external decoders |
| [PHASE_6_VALIDATION_PLAN.md](PHASE_6_VALIDATION_PLAN.md) | 2026-01-30 | Detailed external-validation plan |
| [TEST_RESULTS.md](TEST_RESULTS.md) | early 2026 | Early encoder test run |
| [VCO_TIMING_FIX.md](VCO_TIMING_FIX.md) | 2026-01-30/31 | Draft report on the VCO and Martin timing fixes |
| [SSTV_TIMING_FIXES_FINAL.md](SSTV_TIMING_FIXES_FINAL.md) | 2026-01-30/31 | Final report on the same fixes |
| [VIS_TEST_IMPLEMENTATION_SUMMARY.md](VIS_TEST_IMPLEMENTATION_SUMMARY.md) | early 2026 | How the VIS test suite was first built |
| [RX_FEATURES_PLAN.md](RX_FEATURES_PLAN.md) | 2026-02-05 | Plan for adding the decoder |
| [RX_DECODER_PROGRESS.md](RX_DECODER_PROGRESS.md) | 2026-02-05 | First decoder prototype |
| [PROGRESS_DASHBOARD.md](PROGRESS_DASHBOARD.md) | 2026-02-05 | Progress snapshot |
| [DSP_TEST_RESULTS_SUMMARY.md](DSP_TEST_RESULTS_SUMMARY.md) | 2026-02-05 | DSP test run (13 to 14 of 17 passing then; 17 now) |
| [VIS_DECODER_ANALYSIS.md](VIS_DECODER_ANALYSIS.md) | early 2026 | VIS decoder investigation |
| [VIS_DECODER_VALIDATION_REPORT.md](VIS_DECODER_VALIDATION_REPORT.md) | early 2026 | VIS decoder test against 13 recordings not in the repository |
| [TEST_MODES_ANALYSIS.md](TEST_MODES_ANALYSIS.md) | early 2026 | Why `tests/test_modes/` failed to decode then (all 43 decode now) |
| [VIS_TEST_AUDIO_SAMPLES.md](VIS_TEST_AUDIO_SAMPLES.md) | 2026-02-19 | VIS test note |
| [BPF_AGC_ENABLEMENT_SUMMARY.md](BPF_AGC_ENABLEMENT_SUMMARY.md) | 2026-02-19 | Change summary for turning the band-pass and AGC back on |
| [MASTER_PLAN_REVIEW.md](MASTER_PLAN_REVIEW.md) | 2026-02-21 | Plan review and CLI/UI settings wish list |
