# Readiness review

An adversarial review of the application plan, asking one question: **could a
team start coding from these documents without inventing the answers
themselves?**

Reviewed on 2026-09-19: the requirements, architecture, UX design, roadmap and
engineering practices, plus the decision records. The verdict was no —
six gaps were outright blocking. This document records what was missing, where
it is addressed now, and what is still open, so the same review can be run
again later.

## Blocking gaps

Things a team would have had to invent, differently, in three front ends.

| # | Gap | Now addressed in |
| --- | --- | --- |
| B1 | The API was named and justified but never specified: no methods, no events, no error model, no framing, no subscription semantics | [api-specification.md](api-specification.md) |
| B2 | No data model at all: no database schema, no picture metadata, no configuration keys, no template document format | [data-model.md](data-model.md) |
| B3 | "The session state machine" was referenced three times and never defined — no states, transitions, guards or timeouts | [session-behaviour.md](session-behaviour.md) |
| B4 | Multi-client behaviour undefined. **Two clients could key the radio at once**, which is an on-air fault, not a UX wrinkle | [session-behaviour.md](session-behaviour.md#multi-client-rules), [ADR-0009](../decisions/0009-transmit-arbitration.md) |
| B5 | No failure taxonomy: device loss, disk full, corrupt database, rig disconnect, client death, daemon restart mid-transmission all undefined | [session-behaviour.md](session-behaviour.md#failure-taxonomy) |
| B6 | No version or compatibility policy across five independently released components | [ADR-0010](../decisions/0010-versioning-and-compatibility.md), [devops-and-delivery.md](devops-and-delivery.md#versioning-and-compatibility) |

## Feature gaps

Requirements missing from a plan that claimed to cover full scope. The first
three are the ones that would have embarrassed us on the air.

| # | Gap | Now |
| --- | --- | --- |
| F1 | SSTV reports are **RSV**, not RST. The log and ADIF mapping had it wrong | `R-LOG-5`, [data-model.md](data-model.md#what-adif-maps-to) |
| F2 | No transmit drive calibration, though over-driving is the most common SSTV fault and the original had no help either | `R-TX-10`, `R-TX-11`, story F7 |
| F3 | No interoperability requirement: nothing said our transmissions must decode in MMSSTV or QSSTV | `R-INT-1…3`, [test strategy](test-strategy.md#interoperability) |
| F4 | **A Raspberry Pi has no real-time clock.** An unattended station with no network would have written 1970 into every log entry | `R-TIME-1`, [session-behaviour.md](session-behaviour.md#time) |
| F5 | Repeater tones and FSK identifier receive were "keep" in the inventory, then vanished from the requirements | `R-TX-13`, `R-RX-12` |
| F6 | No way to decode a recording from a file, despite the CLI being a priority | `R-RX-11`, story C7 |
| F7 | No unattended or scheduled transmission, the obvious Pi use case | `R-TX-12` |
| F8 | No storage budget or disk-full behaviour for a station that runs for months | `R-LIB-6`, story D3 |
| F9 | No stable device identity; unplugging a USB interface would have silently changed devices | `R-AUD-7` |
| F10 | No frequency presets or manual entry, so a station with no rig could not stamp pictures | `R-RIG-7` |
| F11 | No first-run setup, the step where most operators give up | `R-CFG-7`, [ui-ux-design.md](ui-ux-design.md#first-run-r-cfg-7) |
| F12 | No notifications, no diagnostics bundle, no crash reporting path — in a project that collects no telemetry | `R-UX-1`, `R-OPS-1…3` |
| F13 | No remote pairing or discovery, though remote control was a headline benefit | `R-NET-1`, `R-NET-2` |
| F14 | Waterfall was a **Must** requirement with no specification: no FFT size, window or rate, and it drives event bandwidth | [architecture](application-architecture.md#threading) budgets table |

## Engineering and testing gaps

| # | Gap | Now |
| --- | --- | --- |
| E1 | No test strategy worth the name: five rows in a table, no levels, no gates, no data policy, no coverage or flaky policy | [test-strategy.md](test-strategy.md) |
| E2 | No test doubles. Every developer and every CI job would have needed a sound card, a radio and a keying interface | [ADR-0011](../decisions/0011-simulation-first-development.md), story A3 |
| E3 | No Definition of Ready or Done, no backlog structure, no iteration cadence — "agile practices" asserted but not defined | [devops-and-delivery.md](devops-and-delivery.md#working-agreement), [feature-backlog.md](feature-backlog.md) |
| E4 | Missing test types entirely: property-based, golden image, fuzzing, upgrade, soak, interoperability, accessibility, localisation | [test-strategy.md](test-strategy.md#levels) |
| E5 | No traceability from requirements to tests | Backlog stories name requirements; CI reports untested **Must** requirements |
| E6 | No developer onboarding path: nothing said how to run the thing locally | [devops-and-delivery.md](devops-and-delivery.md#development-environment) |
| E7 | No defect severity definitions or response expectations | [test-strategy.md](test-strategy.md#defects) |
| E8 | Crash handling unaddressed in a no-telemetry project | [devops-and-delivery.md](devops-and-delivery.md#when-it-breaks-in-the-field) |
| E9 | Packaging named but not specified: no systemd hardening, no entitlements, no notarisation, no rollback | [devops-and-delivery.md](devops-and-delivery.md#packaging) |
| E10 | No concurrency budgets: no buffer sizes, no latency targets, no event rates | [architecture](application-architecture.md#threading) |
| E11 | Platform lifecycles ignored: iOS suspension and audio interruption would have corrupted transmissions | [architecture](application-architecture.md#platform-lifecycles) |

## Second audit: back to the original software (2026-09-19)

The first inventory of MMSSTV used the manual and the form names. A second
pass over the version history (`EUPDATE.TXT`) and the auxiliary
specifications (`Repeater.txt`, `fskid.txt`, `mode.txt`) found features the
plan had missed entirely, and corrected a claim I had made about the original.

| Found | Consequence |
| --- | --- |
| **A complete SSTV repeater mode**: tone access, identifier answer, replay, beacon with channel-clear checking, template rotation, supervision counters | The Pi use case was already solved in 2001. Became requirements `R-REP-1…6` and [epic P](feature-backlog.md#p-unattended-and-repeater) |
| **An occupancy detector** gating every unattended transmission | `R-RX-17`; the discipline that makes a beacon a good neighbour |
| **Three demodulators**, one of them low-CPU | `R-RX-16`; relevant to a Pi Zero |
| **Notch, adaptive noise reduction, AFC** | `R-RX-13…15`. AFC was never ported and blocks satellite work |
| **CW keyboard** with macros and presets, not just an identifier | `R-TX-14` |
| **FSK identifier**, fully specified in `fskid.txt` | `R-RX-12`, `R-TX-15` |
| **VariSSTV**: per-colour transmit power shaping to protect the transmitter | `R-TX-16`, and support for the duty-cycle guard |
| **Radio command presets**, custom log export, log backup | `R-RIG-7`, `R-LOG-6`, `R-LOG-7` |
| Sample rates from 8 kHz, stereo channel selection, level-converter calibration | `R-AUD-8`, `R-AUD-9` |

**A claim I got wrong.** The first inventory said MMSSTV had "no headless or
remote operation" and "no automation surface". The first half was too strong:
it has no *remote* operation, but it runs unattended perfectly well as a
repeater. The inventory is corrected.

Together with the ten gaps found by reviewing the plan against itself —
regulation, satellites, backup, distribution, coexistence, duty cycle, remote
keying, receive aids, multi-radio, and the missing definition of 1.0 — the
requirement count went from 98 to 134.

## Before the repository can be public

Two assets are committed whose redistribution rights are not established. Both
were found while preparing to publish, and both are already in git history, so
removing them from the working tree is not sufficient.

**Decisions taken 2026-09-19:** remove the handbook and cite it, rewriting
history before publication; regenerate the fixtures with known provenance
(story A7). The table below records what was found.

| Asset | What it is | Evidence | Options |
| --- | --- | --- | --- |
| `docs/standards/sstv-handbook.pdf` and `sstv-handbook/` | **"Image Communication on Short Waves" by Martin Bruchanov, OK2MNM** (www.sstv-handbook.com), 196 pages, PDF produced 2011-02-01. No licence statement anywhere in the file; the `main.tex` is a traced reconstruction of the PDF, which is arguably a derivative work | Title and author recovered from the document's own text; PDF metadata is generic (`texput.dvi`, Ghostscript) | Ask the author for permission; or remove and cite with a link, keeping a local untracked copy; either way the history needs rewriting before publication |
| `tests/audio/*.wav`, `*.jpg` | 18 recordings and 20 reference images, present since the first commit | No EXIF author, software or copyright fields; no WAV metadata chunks; no provenance recorded anywhere | Establish origin if you remember it; otherwise regenerate: the "tight" recordings come from our own encoder, and the "padded" properties (lead-in silence, trailing tone) can be synthesised, so no test value is lost |

Removing files from a published history requires a rewrite (`git filter-repo`)
before the repository becomes public. With one contributor and no forks that
is cheap now and expensive later. The repository's `.git` is already 1.0 GB,
much of it these assets plus executables committed and later deleted, so a
rewrite would also make cloning tolerable.

A third, smaller point: citing the handbook is unaffected either way.
References to it in the documentation can become a citation and a link, which
is the normal scholarly practice and costs the reader nothing.

## Still open

Deliberately, with the reason:

| Open question | Why it is still open | When it must close |
| --- | --- | --- |
| SP-2's Raspberry Pi Zero 2 W measurement | Deferred 2026-09-20: the Zero is not set up. The transport decision is made and does not depend on it; the figure confirms headroom on the weakest target. Harness kept in `spikes/sp2-api-framing/` | Before M1 closes, since `N-1` and `N-11` are measured there |
| Audio backend, API details, template renderer, storage shapes are **Proposed**, not Accepted ([ADRs 0004, 0005, 0007, 0008](../decisions/)) | They deserve a spike and your decision rather than my assertion | 0004, 0005, 0008 before M0 ends; 0007 before M2 starts |
| Template rasteriser and text shaping choice | Hinges on iOS build size and CJK quality; needs measurement | Spike before M2 |
| App Store legal review | Needs a lawyer, not an architect | Before M5 ends |
| Hardware bench inventory and budget | Mostly replaced by virtual devices ([ADR-0013](../decisions/0013-test-environments.md)); the remaining bench needs an audio interface, a serial adapter and a keying-sense circuit | Before M2's keying stories |
| **Three front ends or two?** The Pi 500 is desktop-class with a keyboard, so Qt/QML could serve it and Linux both, removing the ImGui front end and roughly a milestone of solo work | Deferred deliberately on 2026-09-19 | **Before M3 starts.** If it stays three, [ADR-0002](../decisions/0002-three-front-ends.md) stands; if it becomes two, a new ADR supersedes it |
| ~~Product name~~ | **Answered 2026-09-19**: PocketSSTV, with `pocketsstvd` and `pocketsstv` as the binaries. Availability on package registries is unverified and should be checked before first publication | Before packaging |
| ~~What radio and interface are available~~ | **Answered 2026-09-19**: QMX, FT-817, FT-897, FT-990, FTDX3000, FTX-1F, KX3 with KXPA100. Roles assigned in [the test strategy](test-strategy.md#the-radios-and-what-each-one-is-for). Still needed: an isolated audio interface for the line-level radios, and the callsign for on-air work | Audio interface before M2 |
| Whether the Pi front end ships as its own package or with the daemon | Packaging detail, no design consequence | M3 |

## Ready to start?

M0 can begin. The checklist for saying "yes" to M1:

- [ ] ADRs 0004, 0005 and 0008 moved from Proposed to Accepted
- [ ] API schema published and the C client generating
- [ ] Simulation harness running the stack with no hardware
- [ ] CI green on arm64, x86-64 and macOS inside the 15-minute budget
- [ ] Database and config bootstrap with a migration test
- [ ] Backlog epics B to E decomposed with acceptance criteria
- [ ] Hardware bench ordered, so M2's keying work is not blocked

## How to re-run this review

Ask of each document: could someone who was not in the room implement this
without asking a question whose answer changes the shape of the code? Then
check three things specifically, because they are where this plan failed the
first time: **interfaces** (is the contract written down?), **failure**
(what happens when it breaks?), and **the radio** (can this key a
transmitter, and what stops it?).
