# Contributing

Thanks for looking. This is a port of MMSSTV's SSTV encoder and decoder, plus
an application built on top of them. Before writing code, the two
documents worth reading are [docs/README.md](docs/README.md), which explains
how the documentation is organised, and
[docs/plans/README.md](docs/plans/README.md), which explains what is being
built and why.

## Build and test

```bash
cmake --preset dev
cmake --build --preset dev -j
(cd build/dev/tests && ctest --output-on-failure)
```

Requirements: CMake 3.10+, a C++17 compiler for the code, a C99
compiler for the public headers. No external dependencies.

Two tests need fixtures that are not in the repository:

- `decode_modes` needs `tests/test_modes/`, generated with
  `./bin/generate_all_modes tests/test_modes 48000`.
- Everything else runs from a fresh clone.

## What a good change looks like

- **Tests at the right level.** A fix for a defect arrives with the test that
  would have caught it. [docs/plans/test-strategy.md](docs/plans/test-strategy.md)
  says which level suits which code.
- **Documentation in the same pull request.** A behavioural change that
  leaves the specification stale is incomplete. Documentation drift is
  treated as a defect.
- **No new warnings.** The build is warning-free with `-Wall -Wextra`, and
  `clang-tidy` reports nothing on `core/` and `apps/`. Please keep it that
  way, and use the committed `.clang-format`:

  ```sh
  cmake --preset dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  find core apps \( -name '*.cpp' -o -name '*.h' \) \
    -exec clang-format --dry-run --Werror {} +
  clang-tidy -p build/dev core/src/*.cpp apps/*/main.cpp
  ```

  **On macOS**, add `--extra-arg="-isysroot$(xcrun --show-sdk-path)"` to the
  `clang-tidy` line. A Homebrew clang-tidy does not know Apple's SDK paths,
  and without it the standard headers are not found. It does not fail
  loudly — it reports a handful of plausible-looking `bugprone-*` findings
  derived from an AST where `std::vector` and `size_t` are unknown types. If
  clang-tidy reports `'cstddef' file not found`, every other finding in that
  run is noise.
- **Explain why, not what.** Comments that restate the code are noise.
  Anything non-obvious about the radio, the SSTV standard or MMSSTV's
  behaviour deserves a comment, because that knowledge is not recoverable
  from the code.

## Code that can key a transmitter

Keying, the watchdog and the transmit state machine are held to a higher
standard than the rest: written test-first, 100% branch coverage, and
verified against real hardware before merge. A transmitter stuck keyed is an
interference incident and can damage equipment. If your change touches that
path, say so in the pull request and expect more questions than usual.

## Licensing of contributions

This repository carries two licences, split along the module boundary — see
[LICENSING.md](LICENSING.md):

- Code derived from MMSSTV (`src/`, `include/`) is **LGPL v3**.
- Everything new (`core/`, `apps/`, tests for them) is **Apache-2.0**.

Contributions are accepted under the [Developer Certificate of
Origin](https://developercertificate.org/): add a `Signed-off-by` line with
`git commit -s`. There is no copyright assignment and no CLA. Every new file
needs an `SPDX-License-Identifier` matching the directory it lives in.

## Reporting bugs

For anything security- or safety-related, follow [SECURITY.md](SECURITY.md)
rather than opening an issue. For ordinary bugs, a recording that reproduces
the problem is worth more than a description: the daemon can replay one with
`pocketsstv feed recording.wav`, so a failing decode can be reproduced exactly
on another machine.
