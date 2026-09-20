# Feature backlog

The full scope, decomposed far enough to start work. Milestones M0 to M2 are
broken into stories with acceptance criteria; later milestones are epics,
decomposed when we reach them — specifying a Flutter screen in detail today
would be guessing.

Story format: an outcome an operator or developer can observe, its acceptance
criteria, the requirements it satisfies
([requirements](application-requirements.md)) and how it is tested
([test strategy](test-strategy.md)). Sizes are S, M or L.

Each epic carries the tier from
[ADR-0012](../decisions/0012-risk-tiered-methodology.md), which sets how it is
tested: tier 0 and 1 are test-first with the strictest gates, tier 3 is
characterization-tested and explicitly not test-first.

| Epic | Milestone | Tier | Theme |
| --- | --- | --- | --- |
| [A. Foundations](#a-foundations) | M0 | 1 | Build, CI, skeleton, simulation |
| [B. Audio](#b-audio) | M1 | 2 | Devices, levels, calibration |
| [C. Receiving](#c-receiving) | M1 | 2, 3 | Session, decode, waterfall, quality |
| [D. Picture library](#d-picture-library) | M1 | 2 | Storage, metadata, search, retention |
| [E. Control surface](#e-control-surface) | M1 | 1 | API, CLI, events, service |
| [F. Transmitting](#f-transmitting) | M2 | **0** | Encode, keying, watchdog, CW ID |
| [G. Rig control](#g-rig-control) | M2 | 2 | hamlib, presets, stamping |
| [H. Pictures and templates](#h-pictures-and-templates) | M2 | 2, 4 | Formats, fitting, template rendering |
| [I. Logging](#i-logging) | M2 | 1 | Contacts, RSV, ADIF |
| [J. Remote access](#j-remote-access) | M2 | 1 | Pairing, discovery, tokens |
| [K. Pi front end](#k-later-epics) | M3 | 5 | ImGui, touch — **under review, see readiness review** |
| [L. Linux front end](#k-later-epics) | M4 | 5 | Qt/QML, desktop |
| [M. Apple front end](#k-later-epics) | M5 | 5 | Flutter, VOX, App Store |
| [N. Release readiness](#k-later-epics) | M6 | — | i18n, a11y, packaging, beta |
| [O. Regulation and on-air safety](#o-regulation-and-on-air-safety) | M2 | **0** | Band plan, identification, duty cycle, automatic control |
| [P. Unattended and repeater](#p-unattended-and-repeater) | M2–M3 | 2 | Occupancy, beacon, replay, reporting |
| [Q. Data portability](#q-data-portability) | M1–M2 | 2 | Backup, restore, export |
| [R. Receive aids](#r-receive-aids) | M3 | 3 | Notch, noise reduction, AFC, demodulator presets |
| [S. Satellite and ISS](#s-satellite-and-iss) | M3 | 2, 3 | Doppler tolerance, pass recording |
| [T. Distribution](#t-distribution) | M6 | 1 | Update channels per platform |

---

## Spikes, before M0 closes

Three decisions were deliberately not taken on paper. Each is a time-boxed
experiment that ends in an ADR moving from Proposed to Accepted (or being
replaced). **A spike produces a decision and a throwaway prototype, not
production code.**

**SP-1 · Audio backend (2 days)** → [ADR-0004](../decisions/0004-audio-backend.md)
Questions: does miniaudio behave on ALSA, CoreAudio and iOS, including device
loss and route changes? Can the callback stay allocation-free? What does a
44.1 kHz-only dongle do? How large is it on an iOS build?
*Exit:* capture and playback running on macOS and the Pi 500, a measured
callback jitter figure, and a decision recorded. Fall back: PortAudio on the
desktop, native CoreAudio on Apple platforms.

**SP-2 · API framing (1 day)** → [ADR-0005](../decisions/0005-api-protocol.md)
Questions: is JSON-RPC plus length-prefixed binary frames comfortable from C,
Dart and a shell script? What is the cost of a waterfall stream at 20 columns
per second on a Pi Zero 2 W? Does one connection carrying both planes keep
ordering simple?
*Exit:* a throwaway daemon streaming synthetic waterfall data to a C client
and a shell one-liner, with CPU measured on the Zero.

**SP-3 · Storage, and what a picture file is called (1 day)** → [ADR-0008](../decisions/0008-storage.md)
Questions: SQLite in WAL mode on an SD card through a power cut — does it
survive? How fast is a 10,000-row query on a Zero 2 W? And the open
sub-decision: **content-addressed filenames or human-readable ones?** Readable
names (`2026-09-19_1423_martin1_G0ABC.png`) make the folder browsable in
Files and Finder; content addressing dedupes and verifies integrity. A hybrid
is possible: readable names on disk, hash in the database.
*Exit:* a decision on naming, a power-cut test result, and a query timing.

## A. Foundations

**A1 · Repository and build skeleton (M)**
Core library, daemon and CLI build on all three platforms with CMake presets.
*Accepts:* `cmake --preset dev` then one command produces `pocketsstvd` and `sstv`;
`pocketsstvd --version` prints component and API versions. *Tests:* build matrix.

**A2 · CI pipeline (M)**
Pull-request pipeline under 15 minutes across arm64, x86-64 and macOS, with
lint, format, sanitizers, licence and SBOM checks.
*Accepts:* a deliberately broken pull request fails on each gate.
*Satisfies:* `N-8`. *See:* [devops](devops-and-delivery.md#pipeline).

**A3 · Simulation harness (L)**
Fake audio device, loopback audio, fake rig, loopback keying, virtual clock,
fault injector.
*Accepts:* the whole stack runs with `--audio fake --rig fake` on a machine
with no sound card; a recording can be fed in faster than real time.
*Tests:* used by every later test. **Blocks A4, C1, F2.**

**A4 · API skeleton and schema (M)**
`hello`, `system.info`, `events.subscribe`, framing, version negotiation,
error envelope, generated C client.
*Accepts:* a client built against 1.0 connects; a client claiming 2.0 is
rejected with `version_unsupported` naming both versions.
*Satisfies:* `R-CLI-5`. *Tests:* contract.

**A5 · Configuration and storage bootstrap (M)**
TOML load and save, `config.describe`, database creation, integrity check,
migration runner, and station identity (callsign, grid, operator) entered once
and reused by templates, the log and ADIF export.
*Accepts:* unknown keys survive a round trip with a warning; a corrupt
database opens read-only and raises a fault.
*Satisfies:* `R-CFG-1`, `R-CFG-2`. *Tests:* unit, upgrade.

**A6 · Structured logging and journal (S)**
Levels, platform log integration, a station journal of on-air actions.
*Satisfies:* `R-OPS-1`.

**A7 · Fixture manifest and generated corpora (M)**
Put the existing assets to work and stop committing what we can generate.
*Accepts:* every fixture in `tests/audio/` has a manifest entry (origin, date,
rate, expected mode and result, licence, checksum); `tests/test_modes/` is
generated by a CI fixture step rather than assumed to exist;
`tests/decoded_images/` is written to the build directory and untracked; the
impaired, rate-varied and adversarial corpora are generated from the existing
clean recordings at fixed seeds.
*See:* [test data](test-strategy.md#test-data-what-we-have).

**A8 · Virtual device lab (M)** · tier 1
Containers on the Pi 500 providing `snd-aloop`, `gpio-sim`, `tty0tty` and a
hamlib dummy rig, plus the device conformance suite that runs the same
assertions against fake, virtual and real backends.
*Accepts:* the full suite runs on the self-hosted arm64 runner with no
physical audio or radio attached; a deliberately broken fake fails the
conformance suite. *Spike first:* module availability on the runners,
`tty0tty` on Raspberry Pi OS, and whether containerised audio matches
bare-metal latency.
*See:* [ADR-0013](../decisions/0013-test-environments.md).

**A9 · Off-air recording corpus (S)** · tier 3
Capture real SSTV from public WebSDR and KiwiSDR receivers on the calling
frequencies: fading, drift, interference and stations we did not encode.
*Accepts:* at least 20 recordings with provenance and expected results,
decoded with recorded quality floors. *Why:* it fills the `R-INT-2` gap
today, with no hardware and no transmitting.

---

## B. Audio

**B1 · Device enumeration and selection (M)**
*Accepts:* devices list with stable ids; selection persists across restart and
replugging; changing a device while receiving is rejected with
`unsupported_in_state`. *Satisfies:* `R-AUD-1`, `R-AUD-7`.

**B2 · Capture at native rate with resampling (M)**
*Accepts:* a 44.1 kHz-only interface decodes a reference recording with the
same quality as 48 kHz, and every rate the card offers from 8 kHz upward
works without quality surprises. *Satisfies:* `R-AUD-2`, `R-AUD-9`.
*Tests:* integration across the rate-varied corpus.

**B3 · Levels, gain and clipping (S)**
*Accepts:* `audio.level` events at 10 Hz; clipping counted and surfaced.
*Satisfies:* `R-AUD-3`.

**B4 · Device loss and recovery (M)**
*Accepts:* unplugging the interface mid-picture emits `audio.deviceLost`,
stores the partial picture, and resumes automatically on replug.
*Satisfies:* `R-AUD-4`. *Tests:* hardware-in-the-loop and fault injection.

**B5 · Clock calibration (M)**
*Accepts:* calibration reports ppm for RX and TX independently and persists
it; a deliberately offset recording decodes straight afterwards.
*Satisfies:* `R-AUD-5`.

---

## C. Receiving

**C1 · Session state machine (L)**
Implements [session behaviour](session-behaviour.md) with the fake audio
device.
*Accepts:* every transition in that document is exercised; illegal transitions
are refused with `unsupported_in_state`. *Satisfies:* `R-RX-2`, `R-RX-8`.
*Tests:* unit, 100% coverage of this module.

**C2 · Decode to a picture (L)**
*Accepts:* the `tests/audio/` recordings produce pictures matching their
reference images, including the padded ones whose signal starts ~0.45 s in and
ends with a stray tone; lines are persisted as they arrive; killing the
process mid-picture leaves a recoverable partial. *Satisfies:* `R-RX-1`, `R-RX-4`,
`N-3`. *Tests:* integration, power-loss injection.

**C3 · Manual start and mode override (S)**
*Accepts:* starting mid-picture in a chosen mode produces a usable picture.
*Satisfies:* `R-RX-3`.

**C4 · Waterfall and spectrum (M)**
FFT size, window and rate specified and configurable; markers are a client
concern, the data is not.
*Accepts:* `waterfall.column` at the configured rate; a slow subscriber sees
`dropped` rather than stalling the decoder. *Satisfies:* `R-RX-5`.
*Tests:* contract back-pressure.

**C5 · Slant and phase control (M)**
*Accepts:* automatic correction on by default; an operator correction applies
to the current picture and can be saved as the station default.
*Satisfies:* `R-RX-6`, `R-RX-7`.

**C6 · Quality summary (S)**
*Accepts:* every picture carries the quality JSON in
[the data model](data-model.md#database). *Satisfies:* `R-RX-9`.

**C7 · Decode from a file (S)**
*Accepts:* `pocketsstv decode recording.wav` produces the same picture and metadata
as live decoding of the same audio, verified against the `tests/audio/`
corpus and its reference images. *Satisfies:* `R-RX-11`, `R-CLI-3`.

---

## D. Picture library

**D1 · Store pictures with metadata (M)**
*Accepts:* content-addressed files, database row, deduplication on identical
content. *Satisfies:* `R-LIB-1`.

**D2 · Query and paging (M)**
*Accepts:* filters by date, callsign, mode and band return in under 100 ms
over 10,000 pictures on a Pi. *Satisfies:* `R-LIB-2`.

**D3 · Retention, budget and disk-full (M)**
*Accepts:* below the reserve, auto-save stops, a fault is raised, receiving
continues, and the database is never corrupted. *Satisfies:* `R-LIB-4`,
`R-LIB-6`. *Tests:* fault injection.

**D4 · Repair from disk (S)**
*Accepts:* deleting the database and running `library.repair` restores every
picture's row from the files. *Satisfies:* `R-LIB-4`.

**D5 · Outgoing gallery (S)**
*Accepts:* pictures and templates prepared for transmission are stored,
browsable and reusable, and survive restart. *Satisfies:* `R-LIB-3`.

**D6 · Export and delete with undo (S)**
*Accepts:* deletion is reversible for 10 seconds. *Satisfies:* `R-LIB-4`.

---

## E. Control surface

**E1 · Method and event dispatch (L)**
All M1 methods and events from
[the API specification](api-specification.md).
*Accepts:* the conformance suite passes; every error carries `kind` and
`remedy`. *Satisfies:* `R-CLI-5`.

**E2 · Multi-client and transmit lock (M)**
*Accepts:* two clients connect; the second is refused the transmit lock with
`tx_locked`; a disconnect releases the lease.
*Satisfies:* `R-CLI-5`. *See:* [ADR-0009](../decisions/0009-transmit-arbitration.md).

**E3 · CLI (L)**
`monitor`, `status`, `pictures`, `decode`, `config`, `dev feed`, with
`--json` everywhere.
*Accepts:* every command has machine-readable output and a non-zero exit on
failure; `--json` output is schema-validated. *Satisfies:* `R-CLI-2`,
`R-CLI-4`.

**E4 · Service integration (S)**
*Accepts:* the systemd unit starts at boot, restarts on failure, logs to the
journal, and releases keying on stop. *Satisfies:* `R-CLI-7`, `R-CLI-1`.

**E5 · Diagnostics bundle (S)**
*Accepts:* `pocketsstv diagnostics` produces a redacted archive; a crash leaves a
local report. *Satisfies:* `R-OPS-2`, `R-OPS-3`.

---

## F. Transmitting

**F1 · Playback and encode (M)**
*Accepts:* loopback audio decodes what we transmit, for every mode.
*Satisfies:* `R-TX-1`, `R-TX-4`. *Tests:* integration over loopback.

**F2 · Keying backends (L)** · tier 0
Serial RTS/DTR, CAT, CM108, GPIO, VOX, each declaring whether it can confirm
the keyed state (`confirmed`) or only assume it (`assumed`).
*Accepts:* every backend asserts and releases, observed by the loopback keying
interface; an `assumed` backend does not fault for lack of readback, and says
so in `rig.status`; the optocoupler sense line upgrades a serial backend to
`confirmed`. Verified across the radio matrix, with the FT-990 as the legacy
case. *Satisfies:* `R-RIG-1`, `R-RIG-5`. *Tests:* hardware-in-the-loop.

**F3 · Watchdog and fault state (M)**
**The highest-risk story in the project.**
*Accepts:* an induced failure to release enters `Fault`, alarms every client,
retries release, and survives a daemon restart with the line released before
anything else opens. *Satisfies:* `R-RIG-5`, `R-TX-2`. *Tests:*
hardware-in-the-loop, `SIGKILL` during transmission, 100% coverage.

**F4 · Timing and sequencing (S)** · tier 0
Lead-in and tail delays, abort path.
*Accepts:* configured delays are honoured within 10 ms; abort unkeys within
200 ms; **with the KXPA100 in line, RF never appears before the amplifier's
relays are settled** — measured, not assumed, because hot-switching damages
the amplifier. *Satisfies:* `R-TX-3`, `R-RIG-6`.

**F5 · CW identifier (S)**
*Accepts:* sent after the picture, decodable by a CW reader at the configured
speed. *Satisfies:* `R-TX-5`.

**F6 · Tune tone and repeater sequences (S)**
*Accepts:* tone at the configured frequency and duration; repeater tone
sequences sent before a transmission. *Satisfies:* `R-TX-6`, `R-TX-13`.

**F7 · Drive calibration (M)**
*Accepts:* a guided flow sets output level to a target peak with clipping
detection, and the setting persists per device.
*Satisfies:* `R-TX-10`, `R-TX-11`. *Why:* over-driving is the most common
on-air fault in SSTV, and the original had no help for it.

**F8 · Unattended and scheduled transmission (M)**
*Accepts:* a beacon sends a chosen picture on an interval with per-cycle
limits, and stops on any fault. *Satisfies:* `R-TX-12`.

---

## G. Rig control

**G1 · hamlib integration (M)** — connect, read frequency and mode, report
errors without blocking audio. *Satisfies:* `R-RIG-2`.
**G2 · Set frequency and mode (S)** — *Satisfies:* `R-RIG-3`.
**G3 · Network rigctld (S)** — *Satisfies:* `R-RIG-4`.
**G4 · Presets and manual frequency (S)** — band presets and manual entry when
no rig is connected, so pictures are still stamped. *Satisfies:* `R-RIG-7`.
**G5 · Stamping (S)** — frequency and rig mode recorded on every picture and
contact. *Satisfies:* `R-RIG-2`, `R-LIB-1`.

---

## H. Pictures and templates

**H1 · Decode and encode formats (M)** — PNG, JPEG, WebP, BMP in; PNG and
JPEG out; EXIF orientation honoured; hostile files rejected safely.
*Satisfies:* `R-IMG-1`, `R-IMG-3`. *Tests:* fuzzing.
**H2 · Geometry fitting (M)** — fit, fill, stretch, with a crop rectangle and
the 16-line header band handled. *Satisfies:* `R-IMG-2`, `R-IMG-9`.
**H3 · Template document and renderer (L)** — the schema in
[the data model](data-model.md#template-documents), rendered in the core.
*Accepts:* golden images match on all three platforms; long callsigns obey
`overflow`; CJK text shapes correctly. *Satisfies:* `R-IMG-4`…`R-IMG-7`.
**H4 · Field binding (S)** — QSO context into template fields; unbound fields
render empty. *Satisfies:* `R-IMG-6`.
**H5 · Adjustments (S)** — crop, rotate, brightness, contrast, sharpen;
originals never modified. *Satisfies:* `R-IMG-8`.

---

## I. Logging

**I1 · Contacts with RSV (M)** — *Accepts:* RSV strings stored and exported
unchanged. *Satisfies:* `R-LOG-1`, `R-LOG-5`.
**I2 · ADIF import and export (M)** — *Accepts:* round-trip preserves every
field, including ones we do not model. *Satisfies:* `R-LOG-2`. *Tests:*
interoperability.
**I3 · Context pre-fill (S)** — *Satisfies:* `R-LOG-3`.
**I4 · Pictures linked both ways (S)** — *Satisfies:* `R-LIB-5`.

---

## J. Remote access

**J1 · Tokens and TLS (M)** — off by default; tokens named, listed, revocable.
*Satisfies:* `R-CLI-6`, `N-6`.
**J2 · Pairing (S)** — `pocketsstv pair` prints a QR code carrying host, port,
fingerprint and token. *Satisfies:* `R-NET-2`.
**J3 · Discovery (S)** — opt-in mDNS. *Satisfies:* `R-NET-1`.

---

## O. Regulation and on-air safety

Tier 0: this is in the path that keys a transmitter.
See [ADR-0014](../decisions/0014-regulatory-guardrails.md).

**O1 · Band plan and privileges (M)** — *Accepts:* transmitting outside the
configured privileges is refused, with a single deliberate override that is
logged; defaults ship per region and are editable. *Satisfies:* `R-REG-1`.
**O2 · Automatic identification (M)** — *Accepts:* the callsign is sent at the
configured interval and at the end of a sequence; unattended modes refuse to
start without it. *Satisfies:* `R-REG-2`, `R-TX-5`.
**O3 · Automatic control limits (M)** — supervisory timeout, maximum session
length, immediate stop. *Satisfies:* `R-REG-3`.
**O4 · Duty-cycle guard (S)** — *Accepts:* a four-minute transmission at a
power the radio cannot sustain warns before keying, with per-radio limits.
*Satisfies:* `R-REG-4`. *Why:* PD290 at 100 W will cook an FT-990.
**O5 · Remote transmit permission (S)** — separate from remote access, off by
default. *Satisfies:* `R-SYS-3`.
**O6 · Nothing transmits by itself (S)** — *Accepts:* a received picture is
never retransmitted, and no unattended transmission occurs, unless repeater or
beacon mode is explicitly enabled; enabling either is logged.
*Satisfies:* `R-REG-5`.

## P. Unattended and repeater

Ported from MMSSTV's repeater mode (`Repeater.txt`), which is the Raspberry Pi
use case.

**P1 · Occupancy detector (M)** — an auto-correlator reporting whether the
channel is busy. *Accepts:* transmission is postponed while another station is
on frequency, with the threshold visible and tunable.
*Satisfies:* `R-RX-17`, `R-REP-4`.
**P2 · Beacon (M)** — interval, channel-clear silence time, mode, picture and
template rotation. *Satisfies:* `R-REP-2`, `R-REP-5`.
**P3 · Unattended service (S)** — runs headless, survives restart, reports
what it did and why it skipped transmissions. *Satisfies:* `R-REP-1`,
`R-REP-6`.
**P4 · Tone-access replay (M)** — answer an access tone with an identifier,
receive, and retransmit. *Satisfies:* `R-REP-3`. *Note:* the full repeater;
schedule after P1–P3 prove themselves.

## Q. Data portability

**Q1 · Backup archive (M)** — pictures, log, templates and settings in one
archive, on demand and scheduled; the backup is verified readable.
*Satisfies:* `R-DAT-1`, `R-LOG-7`. *Why:* the station runs on an SD card.
**Q2 · Restore (M)** — onto a different machine and a different platform.
*Accepts:* a restore on a fresh install reproduces the library, log and
settings exactly. *Satisfies:* `R-DAT-2`.
**Q3 · Export (S)** — open formats with metadata, with personal data
removable. *Satisfies:* `R-DAT-3`, `R-DAT-4`.

## R. Receive aids

Tier 3: characterization-tested against recordings, never test-first.

**R1 · Notch filter (S)** — placed by the operator on the spectrum.
*Satisfies:* `R-RX-13`.
**R2 · Noise reduction and auto-notch (M)** — `src/SpectralSubtractionDNR.cpp`
exists and is unused; either wire it in or replace it. *Satisfies:* `R-RX-14`.
**R3 · Automatic frequency control (M)** — MMSSTV's AFC was never ported.
*Accepts:* a signal 100 Hz off frequency decodes without operator retuning.
*Satisfies:* `R-RX-15`. **Blocks S1.**
**R4 · Demodulator presets (S)** — including a low-CPU option for the Pi Zero.
*Satisfies:* `R-RX-16`.

## S. Satellite and ISS

**S1 · Doppler tolerance (M)** — *Accepts:* a recording with a drifting
carrier, as an ISS pass produces, decodes without colour shift.
*Satisfies:* `R-SAT-1`. *Depends on:* R3.
**S2 · Pass recording (S)** — record audio for a whole pass and decode it
repeatedly afterwards. *Satisfies:* `R-SAT-2`, `R-RX-10`.
**S3 · Scheduled capture (S)** — start and stop recording around a pass
schedule. *Satisfies:* `R-SAT-3`.

## T. Distribution

**T1 · Update channels (M)** — package repository, Flatpak remote, Homebrew
tap, App Store; the operator is told what changed. *Satisfies:* `R-DST-1`,
`R-DST-2`.
**T2 · Coexistence (S)** — share the rig through a shared control service,
release devices when idle. *Satisfies:* `R-SYS-1`.

## K. Later epics

Decomposed when their milestone starts. Each carries the same bar: the
scripted QSO walkthrough, accessibility and localisation checks, and
packaging that installs on a clean image.

- **K. Pi front end (M3)** — Monitor, Compose, Library, Log, Station on an
  800×480 touchscreen; the fault alarm; works against a local or remote
  daemon. First-run setup wizard (`R-CFG-7`) lands here.
- **L. Linux front end (M4)** — the same screens for a desktop, template
  editor, drag and drop, clipboard, keyboard shortcuts, `.deb` and Flatpak.
- **M. Apple front end (M5)** — Flutter over the embedded core, photo library
  and share sheet, VOX operation, notifications (`R-UX-1`), network rigctld,
  App Store build and relinking instructions.
- **N. Release readiness (M6)** — translator workflow with one non-English
  locale, accessibility pass per front end, operator documentation,
  reproducible signed releases, public beta.

## Cross-cutting, every story

These are not separate stories; they are part of Definition of Done:
translatable strings, errors with remedies, no telemetry, tests at the right
level, documentation in the same pull request, and nothing that can key a
transmitter without going through the watchdog.

## Dependencies on the library track

| Need | Story | Status |
| --- | --- | --- |
| Decode from a buffer or file without re-implementing WAV parsing | C7 | Library gap, [production roadmap](production-roadmap.md) |
| Progress and completion callbacks instead of polling | C2, F1 | Library gap |
| AVT digital header decoding | C2 (AVT quality) | Known limitation |
| pkg-config for the decoder | A1 | Library gap |
