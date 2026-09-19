# Documentation

Four kinds of document, kept apart on purpose, because they answer different
questions and age at different speeds.

| Directory | Answers | Maintained |
| --- | --- | --- |
| [standards/](standards/) | What is SSTV? Tones, headers, mode timing, and the reference sources | Changes only if the standards do |
| [specs/](specs/) | How does this library work? Encoder and decoder design, DSP, provenance | Kept in step with the code |
| [guide/](guide/) | How do I use it? Build, API, tools, debugging | Kept in step with the code |
| [plans/](plans/) | What was intended, and what is still open | Frozen, except the roadmap |
| [archive/](archive/) | What happened along the way | Frozen, known to contain errors |

Plus [CHANGELOG.md](../CHANGELOG.md) at the repository root: what changed and
when, consolidated from the session reports in `archive/`.

## Start here

- New to the project: [guide/getting-started.md](guide/getting-started.md).
- Looking for the mode list, or what the library is: the
  [project README](../README.md).
- Running or extending the tests: [the test suite](../tests/README.md).

## By question

| Question | Document |
| --- | --- |
| What does an SSTV transmission actually contain? | [standards/sstv-signal-format.md](standards/sstv-signal-format.md) |
| How do I encode an image? | [guide/encoder-api.md](guide/encoder-api.md) |
| How do I decode audio? | [guide/decoder-api.md](guide/decoder-api.md) |
| What can the decoder not do yet? | [specs/decoder-status.md](specs/decoder-status.md) |
| Why is my decoded image slanted or shifted? | [specs/decoder.md](specs/decoder.md), then [guide/debug-wav.md](guide/debug-wav.md) |
| Which filter does what, and how well? | [specs/filters.md](specs/filters.md) |
| Does this match MMSSTV? | [specs/mmsstv-source-map.md](specs/mmsstv-source-map.md) and [specs/filter-verification.md](specs/filter-verification.md) |
| Why 1080/1320 Hz in the decoder but 1100/1300 Hz in the encoder? | [specs/vis-tone-frequencies.md](specs/vis-tone-frequencies.md) |
| How does it hold up under noise? | [guide/hf-impairment-testing.md](guide/hf-impairment-testing.md) and [standards/s-units-and-dbm.md](standards/s-units-and-dbm.md) |
| What is left to build? | [plans/production-roadmap.md](plans/production-roadmap.md) |

## Conventions

- **Authorities.** The SSTV Handbook and the MMSSTV source decide disputes,
  in that order for what a mode *is* and the reverse for what stations
  actually accept. See [the project README](standards/README.md).
- **One home per fact.** The protocol is described in `standards/`, the
  implementation in `specs/`, usage in `guide/`. Documents link rather than
  repeat, so a correction lands in one place.
- **Historical records are labelled.** Anything in `archive/`, and the older
  documents in `plans/`, opens with a banner listing its known errors. They
  are never a specification.
- **Measured, not assumed.** Filter responses, timings and error figures in
  `specs/` come from running the code; where a number is a target rather than
  a measurement, the text says so.
