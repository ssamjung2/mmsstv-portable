# 0003: hamlib for rig control, keying kept separate

- Status: Accepted
- Date: 2026-09-19

## Context

MMSSTV controlled radios with hand-written hexadecimal command strings per
model, configured by the operator. Every new radio meant new strings. Keying
was bound up in the same serial-port configuration.

## Decision

Use hamlib for CAT: frequency, mode and, where supported, PTT. Link
`libhamlib` where a radio is attached locally; talk to `rigctld` over TCP for
iOS and for radios attached to another machine.

Keep keying a separate concern with its own backends (serial RTS/DTR, CAT PTT,
CM108 HID, Pi GPIO, VOX), because most stations key without CAT and some have
no CAT at all.

## Consequences

- Hundreds of radios are supported without us knowing anything about them.
- hamlib is LGPL 2.1+, which fits our licensing posture.
- hamlib calls can block for tens of milliseconds, so rig I/O lives on the
  control thread, never near audio.
- We inherit hamlib's model numbering and its occasional per-radio quirks, and
  we must pin and track its version.
- Operators without a supported radio lose nothing: keying and VOX work on
  their own.
