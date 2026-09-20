# 0002: A different UI toolkit per platform family

- Status: Accepted
- Date: 2026-09-19

## Context

The targets pull in different directions. A Raspberry Pi wants something tiny
that runs well on modest hardware and a small touchscreen. A Linux desktop
wants a native-feeling desktop application. iOS wants a genuinely modern
touch application, and the goal there is explicitly to be better than what
SSTV operators have today.

No single toolkit is excellent at all three. Qt on iOS is awkward to ship,
Flutter on a Pi is community-supported at best, and Dear ImGui does not meet
iOS conventions.

## Decision

- **Raspberry Pi**: Dear ImGui + SDL3.
- **Linux desktop**: Qt 6 / QML.
- **macOS and iOS**: Flutter, calling the core through C FFI.

## Consequences

- Each platform gets a front end suited to it, and each can be built by
  someone fluent in that stack.
- Three UIs means three implementations of every screen. This is only
  tolerable because [ADR-0001](0001-core-library-and-daemon.md) keeps them
  thin; the moment logic leaks into a front end, the cost triples.
- A shared UX specification and shared design tokens become mandatory
  artefacts, not nice-to-haves. See [the UX design](../plans/ui-ux-design.md).
- The API needs first-class clients in C and Dart.
- Front ends can ship on different schedules, which suits the per-platform
  milestones in [the roadmap](../plans/application-roadmap.md).
