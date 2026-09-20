# 0011: The whole stack runs without hardware

- Status: Accepted
- Date: 2026-09-19

## Context

This project needs a sound card, a radio, a keying interface and propagation
to exercise properly. If that is the only way to run it, development
serialises behind hardware, CI cannot test anything meaningful, and
reproducing a field bug requires owning the reporter's equipment.

## Decision

Ship test doubles as first-class, configurable parts of the product, built in
M0 before the features that need them:

- A fake audio device that plays recordings into the capture path and captures
  playback, under a virtual clock that can run faster than real time.
- Loopback audio, so the encoder feeds the decoder with no cable.
- A scriptable fake rig, and a loopback keying interface that reports the
  asserted state back to the test.
- A fault injector for device loss, disk full, corrupt database and process
  kills.
- A replay mode that serves a recorded session to the API for front-end work.

They are selected by configuration, not by a build flag, so the shipping
binary can run against them.

## Consequences

- Front ends, core and CI all proceed without hardware.
- A field problem can be reproduced from the operator's own recording, which
  is how a project with no telemetry stays debuggable.
- The doubles are code that must itself be maintained and must not drift from
  the real backends; the hardware-in-the-loop suite exists to catch that.
- Some classes of bug — driver quirks, electrical keying behaviour, real
  propagation — are still only visible on the bench, and the nightly hardware
  run is not optional.
