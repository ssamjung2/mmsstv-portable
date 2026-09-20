# 0006: Keep LGPL parts dynamically linked and relinkable

- Status: Accepted
- Date: 2026-09-19

## Context

The encoder and decoder are LGPL v3, inherited from MMSSTV, and hamlib is
LGPL 2.1+. The plan includes an App Store release. LGPL requires that a user
can replace the LGPL component with their own build; Apple's distribution
terms restrict redistribution, which is what makes GPL software impossible on
the App Store and LGPL software merely delicate.

## Decision

Design for App Store distribution from the start:

- Every LGPL component ships as a dynamically linked framework, never
  statically linked into the application binary.
- The project publishes the object files and the documented procedure needed
  to relink the application against a modified core.
- Module boundaries are drawn so that no LGPL code is compiled into
  proprietary or store-signed binaries.
- A legal review happens before the first submission, and the outcome is
  recorded here.

## Consequences

- Constrains the build layout now rather than at submission time, which is the
  whole point of deciding early.
- Slightly larger app bundles and a more complex Apple build.
- If the review concludes App Store distribution is untenable, the fallback is
  TestFlight and source builds; nothing else in the design changes.
- Qt 6 on the Linux desktop gets the same dynamic-linking treatment, for the
  same reason.
