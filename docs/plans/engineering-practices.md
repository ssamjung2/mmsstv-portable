# Engineering practices

How code in this project is written and reviewed. Two companions cover the
rest: [devops and delivery](devops-and-delivery.md) owns the process,
pipeline, versioning, packaging and releases, and
[the test strategy](test-strategy.md) owns what gets tested and what blocks a
release.

The application is a large step up in scope from a library, and three front
ends plus a daemon plus radios attached to real transmitters is exactly the
kind of project that rots without agreed practice.

Most of these are standard. They are written down because "standard" varies,
and because contributors arrive years apart.

## Licensing and provenance

- Code derived from MMSSTV — the encoder, the decoder — stays **LGPL v3**.
  Everything new, including the station core, daemon, CLI and front ends, is
  **Apache-2.0** ([ADR-0015](../decisions/0015-licensing-of-new-components.md)).
  The boundary follows the module boundary and is checked in CI, because an
  LGPL file compiled into an Apache-2.0 binary is a real defect.
- Every source file carries an **SPDX identifier**; the repository follows the
  [REUSE](https://reuse.software) convention so licensing is machine-checkable
  in CI.
- **Third-party dependencies are inventoried** with name, version, licence and
  why we need it. A release publishes an SBOM.
- LGPL components stay dynamically linked and relinkable
  ([ADR-0006](../decisions/0006-licensing-and-app-store.md)). CI fails if an
  LGPL object is statically linked into a distributable binary.
- Contributions are accepted under the **Developer Certificate of Origin**
  (`Signed-off-by`). No copyright assignment, no CLA.
- MMSSTV's authorship is credited wherever the port is described, as now.

## Working in the repository

- **Trunk-based**: short-lived branches, reviewed pull requests, `main` always
  releasable.
- **Conventional Commits**, which give us changelog entries and version bumps
  without invention.
- **Semantic versioning**, with the API schema versioned separately from the
  application ([ADR-0005](../decisions/0005-api-protocol.md)).
- [CHANGELOG.md](../../CHANGELOG.md) is updated in the same pull request as
  the change, not reconstructed later. We have already paid that debt once.
- **Decisions go in [ADRs](../decisions/)** when they would be expensive to
  reverse.
- Issue and pull request templates ask the questions reviewers would ask
  anyway: what changed, how it was tested, what it risks on the air.

## Code standards

| | |
| --- | --- |
| Languages | C99 for public headers, C++17 for the core, Dart for the Flutter client, QML/C++ for Qt |
| Formatting | `clang-format`, enforced in CI; `dart format`; `qmlformat` |
| Static analysis | `clang-tidy`, compiler warnings as errors on the core |
| Sanitizers | Address, undefined-behaviour and thread sanitizers in CI test runs |
| Memory | RAII in the core; no raw owning pointers; the public C API owns its own resources and documents lifetimes |
| Errors | Status codes across the C ABI, exceptions only inside the C++ core, never across a boundary |
| Real-time code | No allocation, locks, logging or I/O in an audio callback. Reviewed as a hard rule |
| Public API | Additive changes only within a major version; deprecations documented and kept for one cycle |

## Comments and documentation

The rule the existing documentation already follows: **write for the person
reading it in three years**, who has none of today's context.

- Comments explain *why*, not *what the code plainly says*. A comment that
  restates the line above it is noise.
- Anything non-obvious about the radio, the standard or MMSSTV's behaviour
  gets a comment with a reference, because that knowledge is not recoverable
  from the code.
- Public headers carry documentation comments; they are the API reference.
- Behavioural changes update the relevant document in the same pull request.
  Documentation drift is treated as a defect, not a chore.
- Documentation stays sorted by kind, as
  [docs/README.md](../README.md) describes: standards, specifications, guide,
  plans, archive. New documents declare which they are.
- Diagrams are source, not screenshots: `.drawio` XML and Mermaid, both
  diffable and both editable with free tools
  ([docs/diagrams](../diagrams/)).

## Testing

Owned by [the test strategy](test-strategy.md): the levels, what each asserts,
the simulation harness that lets the whole stack run without hardware, the
gates that block a release, and the coverage and flaky-test policies. Three
rules belong here because they are code-review concerns:

- Tests are deterministic. No live radio, no network, no wall-clock timing.
- A fix for an S1 or S2 defect arrives with the test that would have caught
  it.
- Code that can key a transmitter is reviewed against the watchdog rules and
  carries 100% test coverage.

## Continuous integration and releases

Owned by [devops and delivery](devops-and-delivery.md): the pipeline stages
and their time budgets, the version and compatibility rules across five
components, packaging per platform, release channels and signing.

## Dependencies

- Few, and justified. Each new one needs a note in the pull request saying
  what it replaces and what it costs on a Pi image.
- Pinned versions, updated deliberately, with automated security alerts.
- Vendoring is acceptable for single-file libraries (miniaudio, stb-style) and
  discouraged otherwise.
- No dependency may require network access at runtime.

## Security and privacy

- **No telemetry. Ever.** No analytics, no crash upload without an explicit,
  per-incident action by the operator.
- The local API socket is restricted to the local user by default; network
  exposure is opt-in, authenticated with a token and encrypted.
- Images and logs stay on the operator's machine. Any future sharing feature
  is explicit, per-item and off by default.
- A `SECURITY.md` states how to report a vulnerability and what response to
  expect.
- Threat model kept short and honest: the realistic risks are a misconfigured
  network API and a malicious image file, and both get fuzzed inputs in CI.

## Accessibility and internationalisation

Treated as engineering requirements with tests, not as a design aspiration:

- Strings live in one catalogue with a translator workflow that needs no
  developer (`R-CFG-4`); no string concatenation in code.
- Locale-aware dates, times, numbers and units; UTC and local time both shown
  where operators need them.
- Font fallback covers CJK, since the original community is substantially
  Japanese.
- Each front end runs its platform's accessibility checker in CI where one
  exists, and the manual walkthrough includes a screen-reader pass.

## Observability

- Structured logs with levels, to the journal on Linux and the platform log
  elsewhere. Default level says what the station did, not what the code did.
- A "diagnostics bundle" command collects configuration, device list, recent
  log and the last decode's quality summary, with callsign and location
  redactable, so operators can report problems usefully.
- Every on-air action is recorded in a station journal the operator can read.

## Community

The files that make an open-source project usable by strangers, delivered in
M0 rather than "later":

`README.md` · `CONTRIBUTING.md` · `CODE_OF_CONDUCT.md` · `SECURITY.md` ·
`LICENSE` and per-component licences · issue and pull request templates ·
a public roadmap ([application-roadmap.md](application-roadmap.md)) ·
release notes for operators.
