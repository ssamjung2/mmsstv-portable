# Development plan and requirements

What the project set out to build, and what remains. The plan documents here
were written before or during the work; they are kept because they record the
requirements and the reasoning, not because they describe today's code. Each
carries a banner listing what in it is now out of date.

For what the code does today, see [the specifications](../specs/) and
[the changelog](../../CHANGELOG.md).

## Requirements, and where they stand

From the original porting analysis and the encoder plan (January 2026):

| Requirement | Status |
| --- | --- |
| Portable C/C++ SSTV library for macOS, Linux and Raspberry Pi class machines | Met on macOS; portable C and C++ with no dependencies, not yet CI-tested elsewhere |
| Support the full MMSSTV mode set, including specialty modes | Met: all 43 modes encode and decode |
| A small public C API that is easy to embed | Met for the core; convenience helpers (WAV decode, C++ wrapper, callbacks) not built |
| Example utilities for encoding, decoding, batch work and debugging | Met, without option flags |
| Portable, testable, maintainable, built with CMake | Met |
| Automated tests with real audio fixtures | Met: 9 CTest tests, including recorded-audio and round-trip suites |
| Documentation that makes the library usable without reading the internals | Met: see [the guide](../guide/) |

Explicitly out of scope for a first release: a GUI application, noise
mitigation beyond the ported DSP, hardware integration, and commercial
packaging.

## The plans

| Document | Written | What it covers |
| --- | --- | --- |
| [porting-analysis.md](porting-analysis.md) | 2026-01-28 | First analysis of MMSSTV's architecture and a full porting plan, including the parts deliberately left out |
| [encoder-plan.md](encoder-plan.md) | 2026-01-28 | The reduced encoder-only scope that the project actually started from, with phase-by-phase tasks |
| [decoder-plan.md](decoder-plan.md) | 2026-02-05 | Plan for adding the receiver: DSP primitives, demodulator, VIS, image assembly |
| [validation-plan.md](validation-plan.md) | 2026-01-30 | How the encoder was to be validated against external decoders |
| [plan-review.md](plan-review.md) | 2026-02-21 | Mid-project review, scope for a decoder CLI, and a mapping of MMSSTV's UI settings to library options |
| [production-roadmap.md](production-roadmap.md) | 2026-09-18 | The current roadmap to a first release, with a status table of what is done and what is open |

## Open work

The short version: the decoder has no automatic frequency correction and does
not read the AVT digital header; the API has no WAV helper, C++ wrapper or
callbacks; the command-line tools take no option flags; there is no CI script
and no pkg-config file for the decoder. Details and priorities are in
[production-roadmap.md](production-roadmap.md), and the decoder's specific
gaps are in [decoder-status.md](../specs/decoder-status.md).
