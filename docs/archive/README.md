# Archive

Session notes, progress reports and investigations from earlier stages of the
project. **These are historical records, not documentation.** Each one starts
with a banner listing what in it is now wrong, and several contradict each
other as well as the code.

What actually changed, and when, is summarised in
[CHANGELOG.md](../../CHANGELOG.md). Read these files only when you want the
reasoning or the raw measurements behind an entry there. Nothing else in the
documentation links into this directory for a fact.

| Document | Date | What it was |
| --- | --- | --- |
| [encoder-session-handoff.md](encoder-session-handoff.md) | 2026-01-28 | Hand-off note during the encoder work |
| [encoder-session-completion.md](encoder-session-completion.md) | 2026-01-30 | Encoder session wrap-up |
| [encoder-completion-summary.md](encoder-completion-summary.md) | 2026-01-30 | Summary of the finished encoder, with a phase timeline |
| [encoder-next-steps.md](encoder-next-steps.md) | 2026-01-30 | Short-term plan for validating the encoder with external software |
| [encoder-test-results.md](encoder-test-results.md) | early 2026 | An early encoder test run |
| [vco-timing-fix-draft.md](vco-timing-fix-draft.md) | 2026-01-30/31 | Draft report on the oscillator and Martin timing fixes; its Martin analysis is wrong |
| [timing-fixes-report.md](timing-fixes-report.md) | 2026-01-30/31 | Final report on the same fixes |
| [vis-test-implementation.md](vis-test-implementation.md) | early 2026 | How the VIS test suite was first built |
| [vis-decoder-analysis.md](vis-decoder-analysis.md) | early 2026 | Investigation into VIS decoding, and the 1080/1320 Hz confusion |
| [vis-decoder-validation-report.md](vis-decoder-validation-report.md) | early 2026 | VIS decoding tested against 13 recordings that are not in this repository |
| [test-modes-analysis.md](test-modes-analysis.md) | early 2026 | Why the files in `tests/test_modes/` failed to decode then. All 43 decode now |
| [decoder-prototype-progress.md](decoder-prototype-progress.md) | 2026-02-05 | The first decoder prototype, whose demodulator was later replaced |
| [dsp-test-results.md](dsp-test-results.md) | 2026-02-05 | DSP reference test run, 13–14 of 17 passing then; all 17 pass now |
| [progress-dashboard.md](progress-dashboard.md) | 2026-02-05 | Progress snapshot at "35–40% complete" |
| [vis-test-audio-samples.md](vis-test-audio-samples.md) | 2026-02-19 | Note on regenerating the VIS test audio |
| [bpf-agc-enablement.md](bpf-agc-enablement.md) | 2026-02-19 | Change summary for turning the band-pass filter and AGC back on |

These files are kept in git history regardless, so they can be deleted if the
directory stops earning its place.
