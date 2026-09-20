# DevOps and delivery

How work flows from an idea to an operator's radio: the development
environment, the pipeline, versioning across five deliverables, packaging, and
the agile mechanics that keep the plan honest once coding starts.

Coding standards and licensing are in
[engineering practices](engineering-practices.md); what gets tested is in
[the test strategy](test-strategy.md).

## Working agreement

Scope is laid out fully in these plans, but delivery is incremental. The team
is **one developer plus an AI assistant**, which decides most of what follows:
ceremony that assumes a group is dropped, and automation takes the place of a
second pair of eyes.

- **Flow, not sprints.** Kanban with a work-in-progress limit of one, and a
  monthly beta release train. Timeboxed iterations fail on irregular solo
  time; a WIP limit does not.
- **A walking skeleton before depth.** The first slice through M1 is audio in
  → decode → store → API → CLI prints it, for one mode, end to end. Integration
  risk surfaces in week one instead of at the end of the milestone.
- **The backlog is the single queue** ([feature backlog](feature-backlog.md)),
  ordered, with acceptance criteria written before work starts.
- **Trunk-based development.** Short-lived branches, small reviewed pull
  requests, `main` always releasable.
- **Feature flags** for anything that spans iterations, so `main` stays
  shippable and half-built features stay dark.
- **Spikes are time-boxed** and produce a decision record, not code that
  quietly becomes production.
- **Documentation and tests land with the code**, never "after the milestone".
- **Rigor follows blast radius** ([ADR-0012](../decisions/0012-risk-tiered-methodology.md)):
  test-first for safety, contract and logic code; characterization tests for
  the DSP; approval tests for rendering.

### Who does what

| | Human | AI assistant |
| --- | --- | --- |
| Design decisions, ADRs | Decides | Drafts, argues both sides |
| Tier 0 code (keying, watchdog) | Writes or reviews, **signs off** | May draft; never the only reviewer |
| Tier 1–3 code and tests | Reviews | Drafts, tests, refactors |
| Documentation | Reviews | Maintains |
| Hardware verification | **Only the human can do this** | Cannot hear audio, see an LED, or watch a picture arrive |
| Release sign-off | Human | Prepares notes and checks gates |

That last row is the reason automation matters more here than on a larger
team: every claim the assistant makes about the physical station is inference
from logs and metrics, never observation.

### Definition of Ready

A story may start when it has: a clear operator-visible outcome, acceptance
criteria, the requirement IDs it satisfies, a test approach, and no unanswered
design question that would change its shape.

### Definition of Done

A story is done when **all** of these are true:

1. Acceptance criteria demonstrated, on the priority platform where relevant.
2. Tests at the right levels, passing in CI, including a regression test for
   any fixed defect.
3. Documentation updated in the same pull request — API schema, key reference,
   guide, or the relevant plan.
4. No new warnings, lint or static-analysis findings; sanitizers clean.
5. Errors carry a `kind` and a remedy; strings are translatable.
6. Telemetry-free, and no new network access without an explicit setting.
7. Reviewed. With one developer that means: the AI assistant reviews every
   change, and tier 0 or 1 work waits 24 hours for a second look by the author
   before merging. Anything that can key a transmitter additionally needs the
   human to have seen it work on hardware.
8. `CHANGELOG.md` updated when operator-visible.

## Development environment

Ten minutes from clone to running station, on any of the three desktops:

```bash
cmake --preset dev && cmake --build --preset dev      # builds core, daemon, CLI
./bin/pocketsstvd --config dev/station.toml --audio fake --rig fake &
./bin/pocketsstv monitor --follow
./bin/pocketsstv dev feed tests/audio/alt5_test_panel_martin1.wav
```

`--audio fake --rig fake` is the whole point: **no sound card, no radio, no
interface needed to develop or to run the tests**
([test doubles](test-strategy.md#simulation-so-nothing-waits-for-hardware)).
Front-end developers get the same with `pocketsstvd --replay session.jsonl`, which
serves a recorded session to the API so screens can be built against realistic
event streams.

Supporting pieces, all delivered in M0: CMake presets per platform, a
devcontainer for Linux work, a documented Pi cross-build sysroot, and
`docs/guide/` pages for each.

## Pipeline

| Stage | What runs | Where |
| --- | --- | --- |
| Pre-commit (local) | format, lint on changed files | macOS laptop |
| Pull request | Build core, daemon, CLI and touched front ends; unit, property, golden, contract, integration, upgrade tests; sanitizers; licence and SBOM check | Hosted x86-64 and macOS runners; arm64 on the Pi 500 |
| Pull request (L1) | The same suites against kernel virtual devices in containers | Pi 500 self-hosted runner |
| Merge to `main` | The above plus packaging artefacts | CI |
| Nightly | Performance and memory on the Pi Zero 2 W, mutation tests on tiers 0–1, interoperability, fuzzing, iOS build | Pi Zero + hosted |
| Weekly | 72-hour soak | Pi Zero 2 W |
| Release | Full matrix, manual walkthrough, signing, notarisation, publication | CI + human |

Rules: the pull-request stage stays under 15 minutes; no retries on failure;
arm64 runs natively on the Pi 500, never under emulation, because emulated
timing hides real-time bugs. The Pi 500 is a **disposable host**: its runner
and test environment are containers rebuilt from a script
([ADR-0013](../decisions/0013-test-environments.md)).

## Versioning and compatibility

Five things version independently, which is a trap unless it is written down
([ADR-0010](../decisions/0010-versioning-and-compatibility.md)):

| Component | Versioning | Compatibility promise |
| --- | --- | --- |
| `libsstv_encoder`, `libsstv_decoder` | SemVer, ABI-stable within major | Existing programs keep linking |
| `libsstv_station` | SemVer | Internal, but ABI-stable for the Flutter FFI boundary |
| **Station API** | SemVer, negotiated at connect | Same major, daemon minor ≥ client minor |
| Daemon and front ends | Application SemVer | A front end works with any daemon of the same API major |
| Database and config | Integer schema version | Forward migrations only; newer schema opens read-only |

Support policy: one previous API major is accepted for one release cycle after
a bump, with deprecation warnings in the daemon log and the changelog. Front
ends state the API version they need in their about screen and in
`--version`, because the first question in any bug report is which pieces are
talking to each other.

## Packaging

| Target | Artefact | Notes |
| --- | --- | --- |
| Raspberry Pi OS, Debian, Ubuntu | `.deb` for `pocketsstvd`, `pocketsstv`, and the front end | systemd unit, `pocketsstv` group owns the socket, config in `/etc/pocketsstv`, hardened unit (`ProtectSystem`, `NoNewPrivileges`, device allow-list for audio, serial and GPIO) |
| Other Linux | Flatpak | Portals for files; device access declared |
| macOS | Signed, notarised `.app` plus a Homebrew cask | Daemon optional; embedded by default |
| iOS | App Store build | LGPL components in dynamic frameworks, relinking instructions published ([ADR-0006](../decisions/0006-licensing-and-app-store.md)) |
| Everyone | Source tarball | Reproducible build documented and verified in CI |

A Pi image with the station pre-installed is a stretch goal, not a
first-release commitment; the `.deb` on a stock Raspberry Pi OS is the
supported path.

## Releases

- **Channels**: `stable`, `beta` (every iteration's end, for the beta group),
  `nightly` (built, unsupported).
- Release notes are written for operators — what changed on the air — with the
  engineering detail left in the changelog.
- Tags are signed; artefacts are checksummed and signed; an SBOM ships with
  each release.
- Rollback is a documented path: previous packages stay published, and the
  database migration notes say what a downgrade costs (newer schema opens
  read-only rather than corrupting).
- A release is never published on a Friday, because stuck-transmitter bugs
  need people awake.

## When it breaks in the field

We collect nothing automatically (`N-5`), so the tooling has to make it easy
for an operator to tell us what happened:

- A crash writes a local report — stack, versions, configuration with the
  callsign and location redactable — and the next launch offers to open it.
- `pocketsstv diagnostics` produces a bundle: versions, devices, rig, recent log,
  last decode quality, configuration. Redaction is on by default.
- Optionally the operator keeps a rolling audio buffer, so a decode failure
  can be attached as a recording and replayed straight into
  `pocketsstvd --audio fake` by a developer. That path — operator recording to
  developer reproduction — is what makes a no-telemetry project debuggable.

## Environments

| Environment | Hardware | Purpose |
| --- | --- | --- |
| Developer machine | macOS laptop | Daily work with fakes; CoreAudio loopback via BlackHole |
| Hosted runners | x86-64 Linux, macOS | L0 suites, packaging, iOS builds |
| Linux test host | **Pi 500** (arm64) | Self-hosted runner, L1 virtual devices in containers, Qt front-end target |
| Headless target | **Pi Zero 2 W** (arm64, 512 MB) | The real deployment target: performance, memory, soak, L2 |
| Mobile target | iPhone | Flutter builds; free provisioning for development, paid account only for TestFlight and the App Store |
| Bench | QMX on USB (audio and CAT in one cable) attached to the Pi 500, keying-sense optocoupler into GPIO, dummy load and attenuator; other radios connected as the test needs | L3, weekly and pre-release |
| Beta stations | Real operators | On the air |

Both Pis are arm64, so one build serves the headless target and the desktop
front end. The Zero 2 W's 512 MB is the binding memory constraint, not the
Pi 500's.

## Risks this section owns

| Risk | Mitigation |
| --- | --- |
| CI slows down and gets bypassed | 15-minute budget enforced; heavy suites moved to nightly |
| Bench Pi becomes a pet nobody can rebuild | Its setup is scripted and version-controlled like everything else |
| Three front ends release out of step | API major/minor policy above; front ends state their requirement; CI builds the matrix |
| No telemetry means no visibility | Diagnostics bundles, rolling audio capture, an active beta group |
| Signing and notarisation credentials bottleneck one person | Documented, stored in the project's secret store, at least two holders |
