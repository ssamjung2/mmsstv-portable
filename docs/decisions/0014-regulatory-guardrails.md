# 0014: The software helps the operator stay legal

- Status: Accepted
- Date: 2026-09-19

## Context

The application can set a radio's frequency over CAT, key a transmitter, and
run unattended for days. Amateur radio is licensed: transmitting outside your
privileges, failing to identify, or leaving an automatic station running
without supervision are regulatory matters, not preferences. The first draft
of the requirements said nothing about any of it.

The rules differ by country and by licence class, and they change. We are not
going to encode the world's regulations, and we are not going to pretend the
problem does not exist either.

## Decision

The software knows three things and acts on them:

1. **A band plan and a privilege set**, configured per station, shipped with
   sensible defaults per region and editable. Transmitting outside it requires
   a deliberate override, and the override is logged.
2. **An identification schedule**: the operator's regulator requires a
   callsign at intervals and at the end of a sequence. The station sends it
   automatically, and unattended modes refuse to run without it configured.
3. **Limits for automatic operation**: a supervisory timeout, a maximum
   session length, an immediate stop, and occupancy checking before every
   unattended transmission.

Two defaults follow from the same reasoning: **remote transmit is a separate
permission from remote access and is off by default**, and a received picture
is never retransmitted unless repeater mode is explicitly enabled.

The software advises; the operator is responsible. Wording in the interface
says so, and no warning claims to be legal advice.

## Consequences

- Band-plan data becomes a maintained asset with a schema and a provenance
  note, not a hard-coded table.
- The identification schedule interacts with the session state machine: a
  transmission may be extended by an identifier, and unattended sequences must
  budget for it.
- Some operators will find the guardrails patronising. They are overridable,
  once, deliberately, per session.
- This is tier 0 work under
  [ADR-0012](0012-risk-tiered-methodology.md): it sits in the path that keys a
  transmitter.
