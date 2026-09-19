# Production-Ready SSTV Library Plan

## Status (2026-09-18)

| Workstream | Done | Open |
| --- | --- | --- |
| A. Decoder quality | All 43 modes decode (round-trip MAE ≤ 16); per-line sync re-lock fixed; timing correction reworked as an integral term (straight images at +1000 ppm); Scottie sync tracking enabled; Robot 36 colour; narrow modes auto-detected (N-VIS); `finish()` and `reset()` behave correctly; `get_state()` reports timing error and correction. | Residual horizontal offsets of a few pixels in some modes; AVT digital header not decoded; no AFC. See [decoder status](../specs/decoder-status.md). |
| B. API | create / free / reset / feed / finish / get_image / get_state; mode hint; timing-correction controls. | WAV/buffer decode helper, C++ wrapper, progress/completion callbacks. |
| C. CLI | `decode_wav`, `decode_wav_debug`, `evaluate_decoded_images` (see [the project README](../../README.md)). | Option flags (`--mode-hint`, `--timing-gain`, `--agc-mode`, `--json-summary`, output formats), batch decode. |
| D. Testing | `test_roundtrip` (all modes, clock mismatch, decoder reuse) plus the recorded-audio suites; see [the test suite](../../tests/README.md). | Single-command CI script; malformed-input and performance tests. |
| E. Packaging and docs | CMake install of both libraries and headers; reference docs rewritten to match the code. | pkg-config file covers the encoder only; release artefacts. |

## 1. Purpose

This document turns the original porting and RX decoder work into a concrete production roadmap for the mmsstv-portable library. It preserves the original objectives of the project while adding the refinements needed for a reliable, user-friendly, and maintainable release.

## 2. Original Project Goals (Preserved)

The original plan targeted the following outcomes:

- Build a portable SSTV encoder/decoder library for macOS/Linux-style environments.
- Support the full SSTV mode set, including common and specialty modes.
- Provide a public C API that is simple to use from applications and scripts.
- Deliver example utilities for encoding, decoding, batch processing, and debugging.
- Keep the implementation portable, testable, and maintainable with CMake.
- Validate behavior with automated tests and real audio fixtures.
- Produce documentation and examples that make the library usable without reverse-engineering the internals.

## 3. Product Vision

The library should be able to:

- Decode SSTV audio recordings into images with predictable quality.
- Support one-command decoding for common workflows.
- Expose tuning controls for advanced users without complicating the default path.
- Be easy to embed in tools, scripts, and GUI applications.
- Ship with clear examples, reproducible tests, and documented behavior.

## 4. Delivery Scope

### In Scope

- Decoder quality improvements for the remaining outlier modes.
- Public API expansion for initialization, tuning, and completion.
- CLI utilities for decode, batch decode, and image evaluation.
- Automated regression coverage with real fixtures.
- Production packaging, installation, and documentation.

### Out of Scope for v1.0

- Full GUI application.
- Advanced noise mitigation beyond the existing DSP path.
- External hardware integration.
- Commercial distribution and app-store packaging.

## 5. Release Goals

### Functional Goals

- Decode common SSTV recordings successfully with minimal manual intervention.
- Preserve existing encoder functionality and keep it test-covered.
- Support all major mode families with reliable image output.
- Provide deterministic behavior for repeated runs on the same input.

### Quality Goals

- No regressions in the existing encoder and decoder test suite.
- Clear error handling and diagnostics for unsupported or low-confidence inputs.
- Stable memory behavior and predictable resource cleanup.
- Reasonable performance for real-time or near-real-time processing.

### Usability Goals

- One-command decode for a WAV file.
- Straightforward integration into C/C++ applications.
- Helpful defaults that work for most users.
- Optional advanced controls for expert tuning.

## 6. Workstreams and Planned Deliverables

### Workstream A — Decoder Quality and Stability

Objective: Improve decode fidelity for the remaining weak modes and prevent regressions.

Planned steps:

1. Profile the current timing correction path.
   - Identify the modes that still show horizontal drift or image skew.
   - Compare decoded output against reference fixtures.

2. Tune mode-specific timing behavior.
   - Add per-mode or per-family calibration where the current generic mapping is insufficient.
   - Preserve the current good performance for already-stable modes.

3. Harden the decoder lifecycle.
   - Ensure reset, finish, and end-of-stream handling behave consistently.
   - Avoid partial or corrupted images when the stream ends unexpectedly.

4. Add diagnostic output.
   - Expose timing-error and correction stats through the state object.
   - Provide optional log verbosity for debugging.

Deliverables:

- Improved image quality for the remaining outlier modes.
- Stable end-of-stream finalization.
- Better debug visibility for timing and sync behavior.

Acceptance criteria:

- The decoder meets or improves the current timing regression thresholds.
- No regressions in the existing regression suite.

### Workstream B — Public API and Integration Experience

Objective: Make the decoder easy to embed and use from other software.

Planned steps:

1. Keep the core C API stable and documented.
   - Provide create/free/reset/feed/finish/get_image/get_state operations.
   - Keep the API small and predictable.

2. Add convenience helpers.
   - Add a high-level function to decode a WAV file directly from disk.
   - Add a helper to decode a buffer and return an image object.

3. Add a lightweight C++ wrapper.
   - Simple RAII wrapper for safe ownership.
   - Optional helper methods for common operations.

4. Support metadata and callbacks.
   - Report detected mode, line progress, current status, and timing diagnostics.
   - Allow optional callbacks for progress and completion events.

Deliverables:

- Simpler integration for host applications.
- Less boilerplate for common decode workflow.
- Better introspection for debugging and UI integration.

### Workstream C — CLI and Utility Tools

Objective: Make the library useful directly from the terminal and scripting environments.

Planned steps:

1. Extend decode utilities.
   - Accept input WAV, output image path, and optional output format.
   - Support batch mode for many input files.

2. Add convenience options.
   - `--mode-hint`
   - `--timing-gain`
   - `--agc-mode`
   - `--no-timing-correction`
   - `--json-summary`
   - `--output-format png|ppm|pgm|jpeg`

3. Add analysis utilities.
   - Compare decoded output against reference images.
   - Generate a simple quality report for a directory of decoded images.

4. Improve developer ergonomics.
   - Add shell completion examples and documented usage patterns.

Deliverables:

- A practical CLI for decode, batch decode, and evaluation.
- Easier adoption by hobbyists and researchers.

### Workstream D — Testing, Validation, and Regression Coverage

Objective: Ensure the library remains reliable over time.

Planned steps:

1. Expand fixture coverage.
   - Add more real audio samples and reference images.
   - Cover the most sensitive modes in regression tests.

2. Add integration tests.
   - Round-trip checks for the encoder and decoder where appropriate.
   - Batch decode validation over multiple modes.

3. Add performance and stability checks.
   - Validate memory usage and handling of malformed input.
   - Measure decode throughput for typical workloads.

4. Add CI-friendly test run scripts.
   - Ensure the project can be tested with a single command in CI.

Deliverables:

- A stronger regression suite.
- Better confidence for future changes.
- Easier validation across platforms.

### Workstream E — Packaging, Documentation, and Distribution

Objective: Make the project production-ready for real-world use.

Planned steps:

1. Improve installation support.
   - Add install rules for headers, binaries, and docs.
   - Ensure pkg-config metadata is correct.

2. Improve documentation.
   - Add quick-start, API reference, CLI reference, and troubleshooting sections.
   - Include example code for C and C++ usage.

3. Create release artifacts.
   - Source tarball, build instructions, and release notes.
   - Document known limitations and supported platforms.

4. Prepare a polished README.
   - Emphasize intended usage scenarios and expected performance.

Deliverables:

- An installable and documented library.
- A cleaner first-run experience for new users.

## 7. Detailed Feature Specifications

### Decoder Features

- Decode SSTV audio from PCM buffers or WAV files.
- Detect mode automatically from VIS headers when available.
- Support optional mode hints for faster or more stable decoding.
- Apply real-time timing correction with adjustable gain.
- Expose timing-error and correction diagnostics.
- Finalize decoding cleanly at end-of-stream.
- Produce RGB24 or grayscale image buffers.

### CLI Features

- Input: WAV file path.
- Output: image path and output format.
- Options for timing correction, AGC mode, and verbosity.
- Batch mode for processing multiple files.
- Summary output in human-readable or JSON format.

### API Features

- Thread-safe enough for single-decoder use in applications.
- Clear status codes and error handling.
- Optional callbacks for progress and completion.
- Stable state access for UI display or logging.

### Quality and Debug Features

- Verbose logging levels.
- Optional debug WAV generation for internal signal inspection.
- Metadata on detected mode, line counts, and timing stats.
- A comparison utility for evaluating decode quality against references.

## 8. Production-Quality Refinements

The following refinements are recommended before calling the project production-ready:

1. Robust input validation.
   - Reject unsupported sample rates or malformed WAV files clearly.

2. Better error reporting.
   - Return descriptive status values and diagnostics rather than silent failures.

3. Cleaner defaults.
   - Default settings should work well for common recordings without requiring advanced tuning.

4. Configurability.
   - Keep advanced tuning controls but make them optional and documented.

5. Cross-platform consistency.
   - Validate on macOS and Linux and ensure build scripts remain portable.

6. Memory safety.
   - Review allocation paths and ensure cleanup is consistent for all failure cases.

7. Documentation quality.
   - Provide examples for common tasks such as decoding a file, decoding a buffer, and batch processing.

8. Benchmarks and profiling.
   - Track decode speed and memory consumption to guide future optimization.

## 9. Suggested Milestones

### Milestone 1 — Decoder Reliability

- Improve the remaining outlier modes.
- Stabilize end-of-stream handling.
- Keep regression tests green.

### Milestone 2 — Usability Pass

- Add a direct WAV-to-image decoder helper.
- Extend the CLI with practical flags and batch mode.
- Improve user-facing error messages.

### Milestone 3 — Release Readiness

- Finalize docs, install rules, and examples.
- Validate on a broader matrix of real audio samples.
- Prepare release notes and a polished README.

## 10. Recommended Release Criteria

The project should be considered ready for a first public release when:

- The automated test suite passes consistently.
- The decoder handles the common mode set reliably.
- The CLI can decode files with a single command.
- Documentation covers setup, usage, and troubleshooting.
- The project builds cleanly on the target platforms.

## 11. Intended Usage Scenarios

This library is intended for:

- Decoding SSTV recordings captured from amateur radio activity.
- Batch processing many audio files into images.
- Integrating SSTV decode into hobbyist tools and scripts.
- Debugging timing, sync, and VIS behavior for research and tuning.
- Serving as a portable reference implementation for future decoder work.

## 12. Recommended Next Actions

1. Complete the remaining timing tuning pass for the worst outlier modes.
2. Add the missing fixture coverage for the remaining real-world cases.
3. Add a first-class decode helper for WAV files from the public API.
4. Extend the CLI with practical batch and output options.
5. Finalize docs and packaging for the first release candidate.
