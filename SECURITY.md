# Security policy

## Reporting a vulnerability

Report privately through GitHub's **[private vulnerability reporting](https://github.com/ssamjung2/mmsstv-portable/security/advisories/new)**
rather than opening a public issue. If that is unavailable, open an issue
saying only that you have a security report and asking for a contact address;
do not include details.

Expect an acknowledgement within a couple weeks. This is a hobby project maintained by
one person, so please be patient — and please do not assume silence means the
report was ignored.

## What is in scope

This software decodes audio from strangers (eventually radios) and, in later versions, keys a radio transmitter. The interesting attack surface is:

| Area | Why it matters |
| --- | --- |
| `core/src/json.cpp` | Hand-written parser on the control socket; the socket may be exposed to a network |
| `core/src/audio.cpp` | Parses WAV headers, which are attacker-controlled |
| `src/decoder.cpp` | Decodes arbitrary audio |
| `apps/pocketsstvd` | Long-running daemon: lifecycle, resource exhaustion, client isolation |
| Keying paths (planned) | A transmitter stuck keyed is an interference incident and can damage hardware |

Memory-safety bugs, crashes from malformed input, resource exhaustion, and
anything that could key a transmitter unexpectedly are all in scope.

## What is not in scope

- Denial of service that requires local filesystem access you already have.
- The absence of authentication on the local Unix socket: it is deliberately
  restricted by file permissions to the local user.
- Findings in third-party code (`external/`), which should go upstream.

## Safety, not just security

If you find a way to make the software transmit when it should not, treat it
as the most serious class of bug here and report it privately. That includes
transmitting without identification, outside a configured band plan, or
failing to release the transmitter.
