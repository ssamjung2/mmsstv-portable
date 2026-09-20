# 0005: JSON-RPC control plane with binary data frames

- Status: Accepted, amended by spike SP-2 (2026-09-19)
- Date: 2026-09-19

## Context

Clients are written in C, C++, QML and Dart, plus shell scripts using the CLI.
The API carries two very different kinds of traffic: occasional commands, and
continuous high-rate data (waterfall columns, scan lines, picture tiles).

## Decision

**Amended after SP-2.** The original proposal put both planes on one
length-prefixed connection. The spike showed that makes the stream unreadable
to shell tools, which conflicts with `R-CLI-4` and the promise that anything
the GUI can do is scriptable. The shape is now:

- **Control plane**: newline-delimited JSON-RPC 2.0 on the main socket, one
  message per line, complete and useful on its own. `nc -U` plus `grep` or
  `jq` is a working client.
- **Data plane**: an **optional** second channel carrying length-prefixed
  binary frames for waterfall columns, scan lines and picture tiles. A client
  that never opens it still receives every control event.
- **Correlation**: each bulk payload is announced in the control plane with a
  sequence number that the binary frame repeats. Ordering between the planes
  is established by that number, not by arrival order — two channels cannot
  guarantee interleaving.
- **Events**: subscriptions on the control plane.

Transports: Unix domain socket locally, WebSocket for opt-in network access.
The embedded build calls the same dispatch in-process and skips serialisation
for bulk data.

## Evidence from SP-2

A throwaway daemon and client implementing both shapes, on macOS (Apple
silicon), 256-bin columns:

| Measurement | Single framed connection | NDJSON + data channel |
| --- | --- | --- |
| Ordering errors, sequence gaps at 20 Hz and 200 Hz | none | none |
| Latency, 20 Hz | mean 0.061 ms, max 0.184 ms | mean 0.086 ms, max 0.201 ms |
| Daemon CPU, 20 Hz | 0.16% of one core | 0.16% |
| Client CPU, 20 Hz | 0.28% | 0.33% |
| Bandwidth, 20 Hz | 6.7 kB/s | 6.6 kB/s |
| At 200 Hz (ten times the waterfall rate) | daemon 0.69%, client 0.55%, 66 kB/s, max latency 0.76 ms | — |
| Consumable by `nc -U` and `grep` | **no** — binary prefixes destroy line semantics | **yes** — 55 events parsed with plain `grep` |

Two findings beyond the numbers:

1. **The data channel must be optional.** The first version of the spike
   blocked on accepting it, so a control-only shell client hung the daemon.
   Any client must be able to ignore bulk data entirely.
2. **Cost is not the deciding factor.** Even at ten times the intended
   waterfall rate the transport costs well under 1% of a core on a laptop;
   the choice is about scriptability, not throughput. The Raspberry Pi Zero
   2 W figure is still to be measured with `run_on_zero.sh`, and the decision
   does not hinge on it.

## Consequences

- Every language we need can speak it today; debugging is possible with a
  terminal.
- Scripts get a stable, documented surface (`R-CLI-4`, `R-CLI-5`).
- Two encodings to specify and test, and a schema to version honestly.
- Slow subscribers must be dropped from high-rate streams rather than allowed
  to stall the producer.

## Alternatives

- **gRPC**: good streaming and codegen, but a heavy dependency for a Pi and
  awkward from shell scripts.
- **Everything in JSON**: simplest, but base64 waterfall data would cost more
  CPU than decoding does.
- **Custom binary protocol throughout**: efficient, hostile to scripting and
  to casual debugging.
