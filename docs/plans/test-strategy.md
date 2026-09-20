# Test strategy

What we test, how, where it runs, and what blocks a release. This project
keys real transmitters and runs unattended for weeks, so the bar is higher
than "the unit tests pass".

Three principles:

1. **Determinism.** No test depends on a radio, the ionosphere, wall-clock
   timing or network access. Signals come from recordings and generators;
   time is injectable.
2. **Tests are the specification's teeth.** Every **Must** requirement in
   [the requirements](application-requirements.md) has a test or a written
   manual procedure. A requirement nobody can test is a requirement nobody
   can implement.
3. **The dangerous paths get the most attention.** Keying, unkeying and the
   watchdog are tested harder than anything else in the system.

## Levels

| Level | Covers | Runs | Budget |
| --- | --- | --- | --- |
| Unit | Pure logic: state machine, parsing, geometry, config, ADIF | Every push, all platforms | < 60 s |
| Property-based | DSP and timing invariants over generated inputs | Every push | < 3 min |
| Golden image | Template and picture rendering, pixel-compared | Every push, all platforms | < 60 s |
| Contract | Every client against the published API schema | Every push | < 60 s |
| Integration | Recording in → picture, metadata and log out, through the daemon | Every push | < 5 min |
| Interoperability | Our audio decoded by other software, and theirs by us | Nightly | < 20 min |
| Performance | CPU, memory and latency budgets on real hardware | Nightly on a Pi | < 15 min |
| Fuzzing | Image decoders, API inputs, ADIF, template documents | Nightly, continuous corpus | 1 h |
| Upgrade | Old database and config to current | Every push | < 60 s |
| Soak | Multi-day unattended operation | Weekly and before release | 72 h |
| Hardware-in-the-loop | Keying, audio devices, rig control | Nightly on the bench Pi | < 20 min |
| Accessibility | Platform checkers, focus order, labels | Every push (front ends) | < 5 min |
| Localisation | Pseudo-locale, long strings, CJK, RTL layout | Every push (front ends) | < 5 min |
| Usability | Scripted QSO walkthrough per front end | Before release, manual | — |
| Field | Real contacts by real operators | Beta, before 1.0 | — |

## Simulation, so nothing waits for hardware

The single most important investment for velocity: the whole stack must run
with no sound card, no radio and no interface. Delivered in **M0**, before the
features that need it.

| Double | Behaviour |
| --- | --- |
| **Fake audio device** | Plays a chosen WAV into the capture path, captures playback to a buffer, runs faster than real time under a virtual clock |
| **Loopback audio** | Playback wired to capture, so the encoder feeds the decoder without a cable |
| **Fake rig** | Implements the rig interface with scriptable frequency, mode, PTT and induced faults (timeouts, refusals, disconnects) |
| **Loopback keying** | Reports the asserted state back to the test, so unkeying is *observed*, not assumed. Also exists in hardware on the bench |
| **Virtual clock** | Controls monotonic and wall-clock time, including jumps, for timeout and `clock_quality` tests |
| **Fault injector** | Disk full, database corruption, device removal, slow client, partial write, process kill |
| **API mock server** | Replays scripted sessions so front-end work never waits for the daemon |

Every double is configuration, not a build flag: the shipping binary can run
against them, which means a bug reported in the field can be reproduced with
the operator's own recording.

## What each level actually asserts

### Unit and property-based

- The [session state machine](session-behaviour.md): every transition, every
  guard, every timeout, including the illegal ones (transmit while in
  `Fault`).
- Property tests over the DSP, which is where subtle bugs hide: encode →
  decode round-trips for random images at random sample rates and clock
  offsets, asserting error stays under a bound; timing correction converges;
  no sample count drifts beyond ±1.
- ADIF: any field in, same field out.
- Config: unknown keys survive a load/save cycle.

### Golden image

Template rendering must be identical on every platform
([ADR-0007](../decisions/0007-template-rendering.md)), so a set of templates —
long callsigns, CJK text, alpha, colour bars, the 16-line header band — is
rendered and compared against committed PNGs, per platform, with a small
tolerance for font rasterisation. A diff image is attached to the failure.

### Contract

The C and Dart clients are generated from the schema, and a conformance suite
runs every method and event against the daemon: argument validation, error
`kind`s, version negotiation, and back-pressure (a deliberately slow
subscriber must see `dropped` counts, and a slow *fault* subscriber must be
disconnected).

### Integration

The end-to-end test that matters most: feed a recording to the daemon, assert
the stored picture's pixels, its metadata, the quality summary and the log
entry. The inputs are the existing `tests/audio/` recordings played through
the fake audio device — the tight ones for a clean baseline, the padded ones
because they start with silence and end with a stray tone, which is what a
real capture looks like.

Variants cover partial pictures, mid-picture restarts, power-loss recovery
from the work-in-progress file, and a full transmit cycle through loopback
audio with keying observed.

### Interoperability

Credibility depends on working with what operators already run.

- **They decode us**: generated audio for every mode is decoded by MMSSTV
  (under Wine), QSSTV and at least one mobile application; the result is
  compared to the source image. Run nightly, tracked per mode.
- **We decode them**: a corpus of recordings made by other software and off
  the air, including deliberately poor ones — weak, slanted, QRM, clipped —
  with expected modes and quality floors. We have none of these today: every
  recording in the repository is clean and, as far as anyone recorded, ours.
  Building this corpus is an M2 deliverable and needs operators' permission.
- **ADIF round-trip** against exports from mainstream loggers.

Failures here are release-blocking for the modes on the common calling
frequencies, and tracked for the rest.

### Virtual devices, and the hardware that remains

Most of what looks like it needs hardware does not
([ADR-0013](../decisions/0013-test-environments.md)). Kernel virtual devices
exercise the real driver paths in CI:

| Real thing | Stand-in | What it proves |
| --- | --- | --- |
| Sound card | `snd-aloop` | Real ALSA enumeration, open, rates, xruns — playback on one subdevice arrives as capture on the other |
| Serial PTT | `tty0tty` | Actual RTS and DTR assertion. Plain `socat` PTYs have **no** modem-control lines and cannot test keying |
| Pi GPIO keying | `gpio-sim` | Real libgpiod paths, with the line readable back |
| Radio CAT | hamlib dummy backend and rig simulators | CAT, CAT-PTT, timeouts, error handling |
| CM108 HID | `dummy_hcd` with raw-gadget, or `usbip` from one real dongle | Weakest substitute; one physical dongle is simpler |

One **device conformance suite** runs identical assertions against fake,
virtual and real backends, so a fake that has drifted from reality fails in
CI rather than on the bench.

What still needs physical hardware, nightly on the Pi and weekly on the
bench: real USB audio behaviour (clock drift, xruns under contention), the
electrical keying circuit, radio CAT quirks, VOX, thermals on a loaded Pi,
and Apple audio-session behaviour.

### The radios, and what each one is for

Eight radios spanning five Yaesu generations, Elecraft, and a
Kenwood-protocol clone, with power from a fraction of a watt to 100 W. That is
an unusually good hamlib matrix, so each gets a job rather than being
"a radio":

| Radio | Power | CAT | Audio | Role |
| --- | --- | --- | --- | --- |
| **QMX** | ~5 W | USB, Kenwood TS-480 emulation | **Built-in USB codec** | The automation rig: one cable gives audio and CAT, safe on a dummy load. Lives attached to the Pi 500 for nightly L3 |
| **FT-817** | 5 W | 8-pin mini-DIN, TTL levels (needs a converter) | DATA jack via an isolated interface | Portable and VOX rig; the acoustic-coupling target for the iOS front end |
| **FT-897** | 100 W (20 W QRP) | Same family as the 817 | DATA jack via an interface | The receiving end of RF loopback tests |
| **FT-990** | 100 W | Older Yaesu binary protocol, level converter | Packet jack via an interface | **The legacy canary.** Exercises hamlib paths modern radios skip, and probably cannot report PTT state — see below |
| **FTDX3000** | 100 W | USB (virtual COM) | Verify whether USB audio is exposed; otherwise DATA/ACC | The real station for on-air beta QSOs |
| **FTX-1F** | — | USB-C; verify hamlib support for a 2025 model | Likely USB audio, verify | Newest-generation check: proves we degrade gracefully when hamlib lags a new radio |
| **KX3** | 0.1–10 W, finely adjustable | Elecraft K3-compatible command set over serial (KXUSB cable) | Line-level jacks, needs an isolated interface | **Second manufacturer** in the CAT matrix, and the controlled-SNR reference — see below |
| **KX3 + KXPA100** | to 100 W | Amplifier on its own serial port | — | Amplifier sequencing: the one setup that makes the keying delays matter |

Three consequences for the test plan:

1. **Two radios make an RF loopback.** Transmit from the QMX or FT-817 into a
   dummy load, receive on the FT-897 or FTDX3000. That exercises the entire
   chain — ALC, filters, clock drift, real receiver AGC — without using the
   air, and it generates an interop corpus of genuine transceiver audio that
   no WebSDR recording can match for repeatability.
2. **The FT-990 is the most valuable radio here for finding bugs**, precisely
   because it is the oldest. Anything that assumes a modern CAT feature will
   fail on it first.
3. **The KX3 turns synthetic impairment into real RF.** Its power is
   adjustable down to a fraction of a watt, so transmitting into an attenuator
   and dummy load with another radio receiving gives **repeatable SNR sweeps
   with genuine propagation-free RF**: real receiver AGC, real filters, real
   clock drift, at a signal level we choose. That is a considerable upgrade on
   `test_hf_impairments`, which adds noise arithmetically. Keep both: the
   synthetic path stays deterministic and runs in CI, the RF path validates
   that the synthetic path is honest.
4. **The KXPA100 is the only setup here that can prove `R-RIG-6`.** Keying
   delays exist so an amplifier's relays are settled before RF appears;
   hot-switching an amplifier damages it. With the amplifier in line, lead-in
   and tail timing stop being configuration and become a hardware safety
   test — and it belongs in tier 0 with the rest of the keying path.
5. **Publish a tested-radio list** with hamlib model numbers, firmware
   versions and known quirks, as promised in
   [ADR-0003](../decisions/0003-hamlib.md). These eight are its first entries.

Unverified, and worth a bench hour before M2: whether the FTDX3000 exposes
USB audio on your firmware, whether your hamlib build knows the FTX-1F or
needs a compatible profile, and whether the QMX firmware transmits SSB audio
suitably. SSTV's constant-amplitude, frequency-varying tone should suit the
QMX transmitter better than voice does, but that is a prediction, not a
measurement.

### Performance

Budgets from [the requirements](application-requirements.md) are enforced, not
aspired to: decode CPU under 25% of one Pi 4 core, time to first waterfall
frame under 2 s, bounded memory over 72 hours, and end-to-end audio latency.
A regression over 10% fails the nightly build.

### Fuzzing

Image files, ADIF files, template documents and API messages are all attacker
-controlled in the realistic case (a picture arrives from a stranger; a phone
on the network talks to the daemon). Continuous fuzzing with a corpus, run
under sanitizers; any crash becomes a unit test.

### Upgrade and migration

Fixture databases and config files from every released version are migrated
forward and then exercised. A migration that loses a field fails the build.

### Soak

72 hours unattended on a Pi with the fake audio device replaying a long
recording loop: memory flat, no file descriptor growth, database consistent,
every picture accounted for, log rotation working, and one induced device
removal and clock jump along the way.

### Accessibility and localisation

Each front end runs its platform's accessibility checker; the manual
walkthrough includes a screen-reader pass. A pseudo-locale with expanded
strings catches clipped layouts before translators see them, and one CJK
locale is tested because the original community is largely Japanese.

### Usability and field testing

Before each release, one operator per front end performs the scripted
walkthrough: first run, set up audio, receive a picture, straighten it,
compose a reply with a template, transmit, log, export ADIF. Observations are
recorded; anything that needed the documentation is a defect in the interface.

Before 1.0, a beta with operators on the air, with a diagnostics bundle they
can attach to reports.

## Test data: what we have

The repository already holds the expensive part of a test corpus, and the
planned stack should build on it rather than start again. Catalogued in
[tests/README.md](../../tests/README.md#fixtures); summarised here for what it
means to the application.

| Asset | Size | What it is | Use in the application stack |
| --- | --- | --- | --- |
| `tests/audio/` 18 WAVs + 20 reference JPEGs | 83 MB, tracked | 22050 Hz mono, clean, 13 of 43 modes, each with the picture that was sent | The primary input for integration tests: fed through the **fake audio device** into the daemon, asserting the stored picture, metadata and quality summary |
| — of which 5 are *padded* recordings | | ~0.45 s of silence before the preamble, trailing tone afterwards | Session tests: hunting, header acquisition away from sample 0, and ignoring audio after the last line |
| `tests/test_modes/` 43 WAVs | 571 MB, **gitignored** | One per mode from our encoder | Mode coverage in nightly runs; regenerated, never committed |
| `tests/decoded_images/` | 18 MB, tracked | Output of a test run, not references | Should be untracked; see below |
| `tests/vis_codes.json` | small | Per-mode VIS reference data | Contract and unit tests for header handling |
| `tests/test_hf_impairments` | — | Adds noise, fading and hum to a clean recording | The generator for the impaired corpus we currently lack |

Three facts shape the plan:

1. **The fixtures are clean and tight.** Nothing here is weak, drifting,
   interfered with, or clipped, and only one sample rate (22050 Hz) is
   represented. Real HF is none of those things.
2. **Two thirds of the modes have no recorded audio.** PD, SC2, AVT, MP, ML,
   narrow, B/W, Robot 24, P5 and P7 exist only as round-trip through our own
   encoder, which cannot catch a shared misunderstanding between our encoder
   and our decoder.
3. **Nothing records provenance.** No fixture says what made it, when, or
   under what licence, so we cannot tell a third-party recording from one of
   our own — which is exactly the distinction interoperability testing needs.

### What we add, and when

| Corpus | Built from | Milestone |
| --- | --- | --- |
| **Impaired set** | The existing clean WAVs, passed through `test_hf_impairments` at fixed seeds: five noise levels, fading, tone interference, clipping | M1, for decode robustness and quality-summary thresholds |
| **Rate set** | The same recordings resampled to 8000, 11025, 44100 and 48000 Hz | M1, for `R-AUD-2` |
| **Mode completion set** | `generate_all_modes` output, checked in as checksums rather than audio, regenerated in CI | M1 |
| **Third-party set** | Recordings made by MMSSTV, QSSTV and a mobile application, plus genuine off-air captures | M2, for `R-INT-1…3`; needs permission and provenance |
| **Adversarial set** | Truncated, header-only, silent, wrong-rate, stereo, huge and malformed files | M1, for the fuzzing and error paths |
| **Picture set** | PNG, JPEG, WebP, BMP, EXIF-rotated, CMYK, 1×1, enormous, and deliberately corrupt | M2, for `R-IMG-1` and the image fuzzer |
| **Template set** | Templates with long callsigns, CJK text, alpha, colour bars, the header band | M2, as golden-image references |

### Fixture policy

- Every fixture has a manifest entry: what produced it, when, sample rate,
  expected mode, expected result, licence, and a checksum.
- Audio we can generate is **generated in CI, not committed**. Only recordings
  we cannot reproduce — third-party and off-air — earn space in the
  repository, and large ones go in a released fixture archive referenced by
  checksum.
- Reference images stay lossless (PNG) for new fixtures. The existing JPEG
  references are lossy, which is part of why `decode_images` tolerates an MAE
  of 60; new golden comparisons should not inherit that slack.
- Changing a fixture is a reviewed change with a stated reason. A "fixed"
  fixture can hide a real regression.
- Off-air recordings are included only with the transmitting operator's
  permission; callsigns in fixture pictures are ours or clearly fictional.

### Two repository problems to fix

- `tests/decoded_images/` is 18 MB of committed test **output**. It should be
  written to the build directory and removed from version control.
- `decode_modes` depends on `tests/test_modes/`, which is gitignored, so the
  test cannot run on a fresh clone. CI should generate it as a fixture step,
  and the test should say plainly how to create it when it is missing.

## Environments

| Layer | Where it runs | Cadence |
| --- | --- | --- |
| L0 fakes | macOS laptop, hosted runners | Every push |
| L1 virtual devices in containers | Pi 500, self-hosted arm64 runner | Every push |
| L2 real devices on real hardware | Pi Zero 2 W (headless target), Pi 500 | Nightly, soak weekly |
| L3 physical loopback and radio | Bench | Weekly, pre-release |
| L4 on the air | Operator stations | Beta |

The Pi Zero 2 W is the performance and memory reference, because it is the
weakest target and has 512 MB. The Pi 500 is the build and virtual-device
host. Both are arm64, so one binary serves both.

## Gates

| When | Must pass |
| --- | --- |
| Pull request | Unit, property, golden, contract, integration, upgrade, lint, format, static analysis, licence check, accessibility and localisation for touched front ends |
| Nightly | Everything above plus performance on a Pi, hardware-in-the-loop, interoperability, fuzzing |
| Weekly | Soak |
| Release | Nightly and weekly green, manual walkthrough per front end, migration from the previous release verified, packaging installed and launched on clean images |

Coverage targets: **90% of the core's logic**, and 100% of the keying and
state-machine code — not as a vanity metric, but because those are the paths
that key a transmitter. Front-end coverage is not a target; their behaviour is
covered by contract and walkthrough tests.

## Flaky tests

A test that fails intermittently is worse than no test, because it teaches the
team to ignore red. Policy: a flaky test is quarantined within one working
day, an issue is opened with the seed or recording that reproduces it, and
quarantine expires after two weeks — at which point it is fixed or deleted.
Retries are not a fix and are not configured in CI.

## Traceability

Each **Must** requirement names its test in the backlog
([feature backlog](feature-backlog.md)), and the requirement ID appears in the
test's name or description. A report generated in CI lists requirements with
no test; it must be empty before a release.

## Defects

| Severity | Definition | Response |
| --- | --- | --- |
| S1 | Keys the radio when it should not, fails to unkey, or destroys stored data | Stop work, fix, release; regression test mandatory |
| S2 | Data loss avoidable by the operator, decode failure on a common mode, crash | Fixed before the next release |
| S3 | Incorrect behaviour with a workaround | Scheduled |
| S4 | Cosmetic, or affects one front end only | Backlog |

Every S1 and S2 fix arrives with the test that would have caught it. No
exceptions: this is how the library's decoder regressions were finally pinned
down, and it is why `test_roundtrip` exists.
