# 0004: miniaudio behind our own audio abstraction

- Status: Proposed
- Date: 2026-09-19

## Context

We need capture and playback on ALSA/PipeWire (Pi and Linux), CoreAudio
(macOS) and iOS with its audio session rules. A Raspberry Pi image should not
grow a large dependency tree, and the audio callback must be allocation-free.

## Decision

Define our own narrow audio interface (enumerate, open, start, stop, level,
callback) and implement it with [miniaudio](https://miniaud.io): a single
permissively licensed source file covering every target we have, including
iOS.

Resampling stays ours: devices run at their native rate and the core converts.

## Consequences

- One dependency, no packaging burden on the Pi, no licence friction.
- The abstraction means miniaudio can be replaced per platform later if a
  backend disappoints, without touching callers.
- We own resampling quality and must test it (a 44.1 kHz dongle is the common
  case, not the exception).
- iOS audio session policy, interruptions and route changes still need
  platform-specific handling above the abstraction.

## Alternatives

- **PortAudio**: mature and familiar, but weaker iOS support and a heavier
  build.
- **RtAudio**: good desktop coverage, no iOS.
- **Native per platform**: best behaviour, three implementations to write and
  maintain. Reconsider if miniaudio proves limiting on iOS.
