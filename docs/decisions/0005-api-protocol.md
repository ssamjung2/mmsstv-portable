# 0005: JSON-RPC control plane with binary data frames

- Status: Proposed
- Date: 2026-09-19

## Context

Clients are written in C, C++, QML and Dart, plus shell scripts using the CLI.
The API carries two very different kinds of traffic: occasional commands, and
continuous high-rate data (waterfall columns, scan lines, picture tiles).

## Decision

One connection, two planes:

- **Control**: JSON-RPC 2.0, human-readable, with a published JSON Schema and
  a version negotiated at connect.
- **Data**: length-prefixed binary frames for bulk and high-rate data.
- **Events**: subscriptions on the same connection, preserving ordering.

Transports: Unix domain socket locally, WebSocket for opt-in network access.
The embedded build calls the same dispatch in-process and skips serialisation
for bulk data.

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
