# 0012: Risk-tiered development and testing methodology

- Status: Accepted
- Date: 2026-09-19

## Context

This repository contains four kinds of software with very different failure
costs: code that can key a transmitter, numeric signal processing, contracts
that several clients depend on, and I/O-bound glue. A single methodology would
be either too heavy for the glue or far too light for the keying path. The
team is one person plus an AI assistant, so ceremony has to earn its place.

## Decision

Apply rigor by blast radius, not uniformly.

| Tier | Code | Method | Gate |
| --- | --- | --- | --- |
| 0 · Safety | Keying, watchdog, PTT release, TX transitions | Strict test-first, fault injection, mutation testing | 100% branch coverage, hardware evidence, human sign-off |
| 1 · Contracts | API dispatch, schema, migrations, ADIF, config | Contract-first: schema exists, conformance tests written before the implementation | Conformance suite green for every client |
| 2 · Logic | Session machine, library, geometry, field binding | Test-first unit tests, property-based where invariants exist | 90% coverage |
| 3 · Numeric | Decoder, encoder, filters | Characterization and property-based. **Not** test-first | Measured before and after; goldens frozen |
| 4 · Rendering | Templates | Approval testing: diffs reviewed and accepted deliberately | Golden images per platform |
| 5 · Interface | Front ends | Acceptance criteria at the API level, scripted walkthrough | No unit-test mandate on view code |

Two supporting rules:

- **The test shape is a diamond, not a pyramid.** This system is I/O-bound, so
  the highest-value tests are daemon-level integration tests driven by
  recordings through the fake audio device.
- **Executable specification.** The `Accepts:` lines in the backlog, the
  failure table in session behaviour and the error `kind`s in the API
  specification become table-driven tests, so a stale document fails CI.

## Consequences

- Effort concentrates where mistakes are expensive. The watchdog gets
  treatment the picture library does not need.
- Mutation testing on tiers 0 and 1 replaces coverage as the quality signal,
  because executed-but-unasserted code counts toward coverage and proves
  nothing.
- Test-first is a rule for tiers 0 to 2 and explicitly wrong for tier 3: you
  cannot write the expected output of a Hilbert demodulator before you have
  one. `test_roundtrip` is the model there — written against known-good
  behaviour and proven by failing on the old code.
- Every backlog epic must carry its tier, or the rule is unenforceable.
