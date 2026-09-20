# 0015: Apache-2.0 for new components, LGPL core unchanged

- Status: Accepted
- Date: 2026-09-19

## Context

The encoder and decoder are LGPL v3, inherited from MMSSTV, and that is not a
choice. The station core, daemon, CLI and front ends are new code, and their
licence interacts with the App Store path
([ADR-0006](0006-licensing-and-app-store.md)): GPL v3 is incompatible with
Apple's distribution terms, so choosing it for the applications would rule out
an iOS release.

## Decision

- `libsstv_encoder`, `libsstv_decoder` and anything derived from MMSSTV stay
  **LGPL v3**, dynamically linked and relinkable.
- All new code — `libsstv_station`, `pocketsstvd`, the CLI and the front ends — is
  **Apache-2.0**.
- Every file carries an SPDX identifier; the boundary between the two licences
  follows the module boundary and is checked in CI.
- Third-party components keep their own licences, inventoried in the SBOM.

## Consequences

- The App Store path stays open, and the patent grant protects contributors
  and users.
- Others may embed the daemon or write their own front end without copyleft
  obligations, which is the right trade for a small project that wants
  contributors.
- The LGPL core's obligations still apply to anyone distributing binaries:
  dynamic linking, and source availability for the core.
- Apache-2.0 is incompatible with GPL v2 (but fine with v3), so a future
  dependency under GPL v2-only would force a rethink.
- Mixing licences means the licence check in CI is not optional: an LGPL file
  compiled into an Apache-2.0 binary would be a real defect.
