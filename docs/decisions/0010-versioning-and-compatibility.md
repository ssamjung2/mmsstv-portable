# 0010: Five components, one compatibility rule

- Status: Accepted
- Date: 2026-09-19

## Context

We ship two libraries, a core, a daemon, a CLI and three front ends, which
will not be updated together. A Pi running a packaged daemon will meet a phone
app updated from the App Store that morning. Without a rule, every
combination is an unknown.

## Decision

- Each component versions with SemVer; the **API has its own version**,
  negotiated at connect.
- A client works with any daemon sharing the API **major** version whose
  **minor** is at least the client's. Mismatch is an explicit
  `version_unsupported` error naming both sides.
- Within a major version, changes are additive only: new methods, new optional
  parameters, new event fields. Clients ignore what they do not recognise.
- Database and configuration use an integer schema version with forward-only
  migrations; a newer schema opens read-only rather than being upgraded by an
  older binary.
- One previous API major is accepted for one release cycle after a bump.

## Consequences

- Version skew produces a clear message instead of strange behaviour.
- The schema becomes a reviewed artefact: adding a required field is a
  breaking change and must be argued for.
- Front ends must display the API version they need, and `--version` prints
  every component, because that is the first question in any bug report.
- We carry compatibility code for one cycle after each major bump, and CI
  tests the previous version against the current daemon.
