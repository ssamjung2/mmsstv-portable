# Application requirements

What we are building on top of the encoder and decoder libraries: an SSTV
station application for Raspberry Pi, Linux, macOS and iOS, with the command
line kept as a first-class interface.

This document says *what* and *why*. The *how* is in
[the architecture](application-architecture.md), the *when* in
[the roadmap](application-roadmap.md), and what the original did in
[the feature inventory](mmsstv-feature-inventory.md).

Requirements are identified (`R-RX-1`) so the roadmap and the tests can refer
to them. Priority is **Must**, **Should** or **Later**.

## Who this is for

An amateur operator working SSTV on HF or VHF, typically on one of the calling
frequencies (14.230 MHz and friends). They tune in, watch a picture arrive
line by line, decide whether to answer, put together a picture carrying their
callsign and a signal report, key the transmitter, send it, and log the
contact. Sessions are long and conversational: pictures are exchanged over
many minutes, often with several stations taking turns.

Three deployment shapes matter:

- **Shack Raspberry Pi**, often headless or on a small touchscreen, left
  running. The priority platform.
- **Linux or macOS desktop**, wired to a radio through a USB interface.
- **iPhone or iPad**, held near the radio, using the speaker and microphone
  with VOX. No cables, no rig control.

## Product principles

1. **The radio is the point, not the software.** Default to working
   configuration; hide everything that can be derived or measured.
2. **Nothing is lost.** Every received picture is kept with the context that
   makes it meaningful: when, what frequency, what mode, what the signal
   looked like.
3. **Honest about the air.** Show signal quality plainly, including when a
   picture is arriving badly, rather than hiding it behind a progress bar.
4. **Scriptable.** Anything the GUI can do, the CLI and the API can do.
5. **Offline and private.** No account, no telemetry, no cloud dependency.
   Network features are opt-in.

## Functional requirements

### Receive

| ID | Requirement | Priority |
| --- | --- | --- |
| R-RX-1 | Decode all 43 modes the library supports, detecting the mode from VIS, 16-bit VIS or the narrow FSK header | Must |
| R-RX-2 | Start receiving automatically on a detected header, with an adjustable trigger sensitivity | Must |
| R-RX-3 | Start receiving manually in a chosen mode, mid-picture, for signals whose header was missed | Must |
| R-RX-4 | Display the picture as it arrives, line by line, with elapsed and remaining time | Must |
| R-RX-5 | Show a live spectrum and waterfall with markers at 1200, 1500, 1900 and 2300 Hz | Must |
| R-RX-6 | Correct slant automatically, and let the operator override it and save the correction as the station default | Must |
| R-RX-7 | Let the operator re-align the picture's horizontal position (phase) by direct manipulation | Should |
| R-RX-8 | Stop, restart and re-sync automatically on loss or re-acquisition of sync | Should |
| R-RX-9 | Record a per-picture quality summary (signal level, sync stability, timing error, dropped lines) | Should |
| R-RX-10 | Optionally record the received audio alongside the picture for later re-decoding | Later |
| R-RX-11 | Decode a recording from a file, producing the same picture and metadata as live decoding | Must |
| R-RX-12 | Decode the FSK identifier some stations send after a picture (45.45 baud Baudot, per `fskid.txt`) | Should |
| R-RX-13 | A notch filter the operator places on the spectrum, to remove a carrier from the passband | Should |
| R-RX-14 | Adaptive noise reduction and automatic notch, switchable | Should |
| R-RX-15 | Automatic frequency control, tolerating a signal that is off frequency or drifting | Should |
| R-RX-16 | Named demodulator presets, including a low-CPU option for constrained hardware | Later |
| R-RX-17 | An occupancy detector that reports whether the channel is busy, used to gate unattended transmission | Must |

### Transmit

| ID | Requirement | Priority |
| --- | --- | --- |
| R-TX-1 | Transmit any supported mode, showing exactly what will be sent before sending it | Must |
| R-TX-2 | Key the transmitter by the configured method, and unkey reliably on completion, abort, error or crash | Must |
| R-TX-3 | Abort a transmission in progress, leaving the radio in receive | Must |
| R-TX-4 | Show transmission progress with a line marker and remaining time | Must |
| R-TX-5 | Send a CW identifier after the picture, with configurable text, speed and tone | Must |
| R-TX-6 | Send a tune tone of configurable frequency and duration for repeater access | Should |
| R-TX-7 | Default the TX mode to the last received mode, with an option to pin it | Should |
| R-TX-8 | Send an FSK identifier | Later |
| R-TX-9 | Queue several pictures for sequential transmission | Later |
| R-TX-10 | Guide the operator to a correct transmit audio level, with peak metering, clipping detection and a persisted per-device setting | Must |
| R-TX-11 | Generate a test tone at a chosen level and duration for drive calibration | Must |
| R-TX-12 | Transmit unattended on a schedule or interval, stopping on any fault | Should |
| R-TX-13 | Send repeater access tone sequences before a transmission | Should |
| R-TX-14 | Send arbitrary CW messages with field substitution, not only an automatic identifier, with named presets | Should |
| R-TX-15 | Send the FSK identifier after a picture, optionally carrying a contest serial | Later |
| R-TX-16 | Reduce transmit power per colour channel (MMSSTV's VariSSTV), to lower the thermal load on the transmitter | Later |

### Pictures and templates

| ID | Requirement | Priority |
| --- | --- | --- |
| R-IMG-1 | Read PNG, JPEG, WebP and BMP; honour EXIF orientation | Must |
| R-IMG-2 | Fit any source picture to the target mode's geometry with a chosen strategy (fit, fill, stretch) and a visible crop control | Must |
| R-IMG-3 | Write received pictures as PNG or JPEG with quality settings, and embed capture metadata | Must |
| R-IMG-4 | Overlay a template on the TX picture with real alpha compositing | Must |
| R-IMG-5 | Template items: text, image, line, rectangle, filled rectangle, colour bar, each positioned and styled | Must |
| R-IMG-6 | Template text supports fields bound to the QSO: their call, my call, report, date, time, band, mode, free text | Must |
| R-IMG-7 | Render templates identically on every platform | Must |
| R-IMG-8 | Basic adjustments: crop, rotate, brightness, contrast, sharpen | Should |
| R-IMG-9 | Handle the 16-line header band automatically from the mode's geometry | Should |
| R-IMG-10 | HEIC input on Apple platforms | Later |

### Picture library

| ID | Requirement | Priority |
| --- | --- | --- |
| R-LIB-1 | Keep every received picture with its metadata: timestamp, mode, frequency, quality, and the station if known | Must |
| R-LIB-2 | Browse, search and filter by date, callsign, mode and frequency | Must |
| R-LIB-3 | Keep a gallery of outgoing pictures and templates for reuse | Must |
| R-LIB-4 | Export and delete pictures, singly and in bulk, with a storage budget and a retention policy | Should |
| R-LIB-5 | Link pictures to log entries in both directions | Should |
| R-LIB-6 | Enforce a storage budget and behave safely when the disk fills: stop saving, warn, keep receiving, never corrupt stored data | Must |

### Logging

| ID | Requirement | Priority |
| --- | --- | --- |
| R-LOG-1 | Record contacts with the fields ADIF requires, plus the pictures exchanged | Must |
| R-LOG-2 | Import and export ADIF, as the primary interchange format | Must |
| R-LOG-3 | Pre-fill the log from the current QSO context (call, band, mode, report) | Should |
| R-LOG-4 | Look up prefix, country and grid from a maintained country file | Later |
| R-LOG-5 | Record SSTV reports as RSV (readability, strength, video), not RST, and carry them through ADIF unchanged | Must |
| R-LOG-6 | Export the log in operator-defined text formats built from field macros, as well as ADIF | Later |
| R-LOG-7 | Back up the log and library on a schedule, and verify the backup is readable | Must |

### Radio control and keying

| ID | Requirement | Priority |
| --- | --- | --- |
| R-RIG-1 | Key by serial RTS or DTR, by CAT through hamlib, by CM108-style USB audio GPIO, by Raspberry Pi GPIO, or by VOX | Must |
| R-RIG-2 | Read frequency and mode from the radio, and stamp received and transmitted pictures with them | Should |
| R-RIG-3 | Set frequency and mode from the application | Should |
| R-RIG-4 | Talk to a `rigctld` over the network, so an iPhone or a laptop can use a radio attached to another machine | Should |
| R-RIG-5 | Fail safe: if keying cannot be released, stop audio, warn loudly and keep warning | Must |
| R-RIG-6 | A configurable delay between keying and audio, and between audio ending and unkeying | Must |
| R-RIG-7 | Band and frequency presets, and manual frequency entry, so pictures are stamped even with no rig connected | Should |

### Audio

| ID | Requirement | Priority |
| --- | --- | --- |
| R-AUD-1 | Enumerate input and output devices and let the operator choose them independently | Must |
| R-AUD-2 | Work at the device's native sample rate, without requiring 48 kHz | Must |
| R-AUD-3 | Show input level with clipping indication, and provide gain control | Must |
| R-AUD-4 | Survive device disconnection and reconnection without restarting | Should |
| R-AUD-5 | Calibrate the sound-card clock for TX and RX independently, from measurement | Should |
| R-AUD-6 | On iOS, manage the audio session for speaker and microphone use with VOX, including interruption handling | Must |
| R-AUD-7 | Identify devices stably across replugging and reordering, and support different devices for input and output | Must |
| R-AUD-8 | Select which channel of a stereo input carries the signal | Should |
| R-AUD-9 | Support the sample rates a sound card offers, from 8 kHz upward, without quality surprises | Must |

### Headless operation, CLI and API

| ID | Requirement | Priority |
| --- | --- | --- |
| R-CLI-1 | Run the station without any GUI, as a service on a Raspberry Pi | Must |
| R-CLI-2 | A command-line client covering receive, transmit, status, configuration and log access | Must |
| R-CLI-3 | Keep the existing file-based tools (`encode_wav`, `decode_wav`, `list_modes`) working | Must |
| R-CLI-4 | Machine-readable output (JSON) from every command | Must |
| R-CLI-5 | A documented, versioned local API that the GUIs and third-party tools use | Must |
| R-CLI-6 | Opt-in network access to that API, authenticated and encrypted | Should |
| R-CLI-7 | Run as a systemd service with sensible defaults and log to the journal | Should |

### Settings and presentation

| ID | Requirement | Priority |
| --- | --- | --- |
| R-CFG-1 | One documented settings file per platform convention, with every key explained | Must |
| R-CFG-2 | Station identity (callsign, grid, operator) entered once and used everywhere | Must |
| R-CFG-3 | Named station profiles for different radios or locations | Should |
| R-CFG-4 | Translatable interface, with a translator workflow that does not require a developer | Should |
| R-CFG-5 | Meet the platform's accessibility conventions: focus order, screen-reader labels, contrast, text scaling | Should |
| R-CFG-6 | Dark and light themes following the system setting | Should |
| R-CFG-7 | A first-run setup that gets an operator from install to receiving: callsign, audio devices, levels, and an optional keying test | Must |

### Regulation and on-air safety

An application that can set frequency and transmit unattended has to help the
operator stay legal and stay out of trouble. None of this was in the first
draft of these requirements.

| ID | Requirement | Priority |
| --- | --- | --- |
| R-REG-1 | Know the operator's licence privileges and warn, or refuse, before transmitting outside them; never transmit outside the configured band plan without a deliberate override | Must |
| R-REG-2 | Identify the station automatically at the interval the operator's regulator requires, and at the end of a transmission sequence | Must |
| R-REG-3 | Apply the extra constraints that unattended and automatically controlled operation carry: a supervisory timeout, a maximum session length, and an immediate stop control | Must |
| R-REG-4 | Warn before a transmission whose length and power would exceed a configured duty-cycle limit for the radio or amplifier in use | Should |
| R-REG-5 | Never transmit anything the operator did not choose: no automatic retransmission of a received picture unless repeater mode is explicitly enabled | Must |

### Unattended and repeater operation

Ported from MMSSTV's repeater mode, which is specified in `Repeater.txt`.

| ID | Requirement | Priority |
| --- | --- | --- |
| R-REP-1 | Run unattended: receive, store, and optionally transmit, with no interface attached | Must |
| R-REP-2 | Transmit a beacon on an interval, only after the channel has been clear for a configured time | Should |
| R-REP-3 | Answer an access tone with an identifier, then receive and retransmit the picture that follows | Later |
| R-REP-4 | Gate every unattended transmission on the occupancy detector, and postpone rather than transmit over another station | Must |
| R-REP-5 | Rotate through a list of pictures and templates for repeated transmissions | Later |
| R-REP-6 | Report unattended activity: what was received, sent, postponed and why | Should |

### Data portability

| ID | Requirement | Priority |
| --- | --- | --- |
| R-DAT-1 | Back up everything — pictures, log, templates, settings — to a single archive, on demand and on a schedule | Must |
| R-DAT-2 | Restore that archive onto a different machine, including a different platform | Must |
| R-DAT-3 | Export a picture set or the whole library in open formats, with metadata, so another tool can use it | Should |
| R-DAT-4 | Delete an operator's personal data (callsigns, names, locations) from exports on request | Should |

### Satellite and ISS operation

The commonest way newcomers meet SSTV is an ISS event, and it is the hardest
signal we will be asked to decode.

| ID | Requirement | Priority |
| --- | --- | --- |
| R-SAT-1 | Decode signals whose frequency drifts during a pass, as Doppler shift causes, without the operator retuning | Should |
| R-SAT-2 | Record a whole pass to audio and decode it afterwards, repeatedly, without loss | Should |
| R-SAT-3 | Take a pass schedule as input and start and stop unattended recording around it | Later |

### Coexistence, distribution and remote safety

| ID | Requirement | Priority |
| --- | --- | --- |
| R-SYS-1 | Share the radio with other software: use a shared rig-control service rather than claiming the port exclusively, and release devices when idle | Should |
| R-SYS-2 | Run more than one station on one machine — two radios, two profiles — without collision | Later |
| R-SYS-3 | Remote transmit is a separate permission from remote access, and is **off by default** | Must |
| R-DST-1 | Publish an update channel per platform: a package repository for Debian and Raspberry Pi OS, a Flatpak remote, a Homebrew tap, and the App Store | Must |
| R-DST-2 | Tell the operator that an update exists, and what changed, without phoning home for it more than they asked | Should |

### Interoperability

The application is only credible if it works with what operators already run.

| ID | Requirement | Priority |
| --- | --- | --- |
| R-INT-1 | Audio we transmit decodes correctly in MMSSTV, QSSTV and mainstream mobile applications, for every mode | Must |
| R-INT-2 | We decode recordings produced by those applications and off the air, including weak, slanted and interfered signals | Must |
| R-INT-3 | ADIF exported by us imports into mainstream loggers, and theirs imports into us without losing fields | Must |

### Time, operations and network

| ID | Requirement | Priority |
| --- | --- | --- |
| R-TIME-1 | Record the trustworthiness of every timestamp, and correct earlier records once the clock is synchronised. A Raspberry Pi with no real-time clock must not write 1970 into the log | Must |
| R-OPS-1 | Keep a station journal of on-air actions the operator can read afterwards | Must |
| R-OPS-2 | Produce a diagnostics bundle for problem reports, redacted by default | Must |
| R-OPS-3 | Write a local crash report and offer it on next launch. Nothing is ever sent automatically | Must |
| R-UX-1 | Notify the operator when a picture completes or a fault occurs while the interface is not in front of them | Should |
| R-NET-1 | Advertise the daemon on the local network, off by default | Later |
| R-NET-2 | Pair a remote client by scanning a code carrying host, port, fingerprint and token | Should |

## Non-functional requirements

| ID | Requirement |
| --- | --- |
| N-1 | Decode in real time on a Raspberry Pi Zero 2 W, the weakest supported target, using less than 50% of one core with the waterfall running |
| N-1b | The daemon's resident memory stays under 150 MB on a 512 MB Pi Zero 2 W, including one decoded picture |
| N-2 | Start to first waterfall frame in under 2 seconds on a Pi Zero 2 W |
| N-3 | A picture must never be lost by a crash: received lines are persisted as they arrive |
| N-4 | Memory stays bounded during multi-day unattended operation |
| N-5 | No network access unless the operator enables it; no telemetry, ever |
| N-6 | The local API socket is restricted to the local user by default |
| N-7 | Packaged natively per platform: Debian packages for Pi and Ubuntu, Flatpak for other Linux, signed app bundle for macOS, App Store build for iOS |
| N-8 | Every release is reproducible from a tagged source tree by a documented command |
| N-9 | Licensing stays coherent: LGPL core, per-front-end licences documented, App Store distribution legally reviewed |
| N-10 | Documentation and code comments are written for a person reading them years later, in the style the existing docs use |
| N-11 | A command from a local client takes effect within 150 ms; event delivery to a local subscriber is within 50 ms of the underlying change |
| N-12 | A library of 10,000 pictures queries in under 100 ms on a Raspberry Pi 4 |
| N-13 | Thirty days of unattended operation without restart, with flat memory and no file-descriptor growth |
| N-14 | High-rate event streams degrade by dropping frames to slow clients, never by delaying the decoder |

## Platform matrix

| | Raspberry Pi | Linux desktop | macOS | iOS |
| --- | --- | --- | --- | --- |
| Priority | 1 | 3 | 4 | 4 |
| Front end | Dear ImGui + SDL3 | Qt 6 / QML | Flutter | Flutter |
| Core | Daemon (`pocketsstvd`) | Daemon | Embedded or daemon | Embedded in-process |
| Keying | GPIO, serial, CAT, CM108 | Serial, CAT, CM108 | Serial, CAT, VOX | VOX only |
| Rig control | hamlib, local or network | hamlib | hamlib | Network `rigctld` only |
| Audio | ALSA or PipeWire | PipeWire or ALSA | CoreAudio | CoreAudio, AVAudioSession |
| Primary use | Unattended station, touchscreen | Desktop operating | Desktop operating | Portable, acoustic coupling |

Rationale for three front ends rather than one is in
[ADR-0002](../decisions/0002-three-front-ends.md).

## Out of scope

- Modes other than the 43 MMSSTV modes; no digital voice, no FT8, no RTTY.
- Being a general image editor. We crop, rotate and adjust; anything more is
  another application's job.
- Cloud galleries, image sharing services and online callsign lookups in the
  first release.
- Contest logging beyond what ADIF carries.
- Windows. Not excluded on principle, but not a target; nothing in the design
  should prevent it later.

## Terms

| Term | Meaning |
| --- | --- |
| VIS | The tone-coded mode identifier at the start of a transmission |
| RSV | The SSTV signal report: readability, strength, video quality. Not RST |
| PTT | Press to talk: the signal that keys the transmitter |
| VOX | Voice-operated keying: the radio keys itself when it hears audio |
| CAT | Computer aided transceiver: the radio's control protocol |
| Slant | Image skew caused by a sample-clock difference between stations |

## How we will know it works

- Every **Must** requirement has an automated test, or a documented manual
  procedure where hardware is involved.
- The decode path is already covered by the library's regression suite; the
  application adds end-to-end tests that run a recording through the daemon
  and compare the stored picture and metadata.
- Keying is tested against a loopback interface that reports the keyed state,
  so `R-RIG-5` is proven rather than assumed.
- Each front end ships with a scripted walkthrough of the primary QSO flow,
  run before release on real hardware.
- Traceability is checked in CI: a report lists **Must** requirements with no
  test, and must be empty before a release.

The full approach is in [the test strategy](test-strategy.md), and the
stories that deliver these requirements are in
[the feature backlog](feature-backlog.md).
