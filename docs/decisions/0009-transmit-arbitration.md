# 0009: Transmit is an exclusive lease, everything else is shared

- Status: Accepted
- Date: 2026-09-19

## Context

The daemon serves several clients at once: the Pi's touchscreen, an ssh
session, a phone. Nothing in the original plan said what happens when two of
them transmit at the same time. The answer cannot be "last one wins": keying a
transmitter twice, or starting a second picture over a first, is an on-air
fault and potentially a hardware one.

## Decision

- Receiving, the waterfall, the library and the log are **shared**: any client
  may read, and events go to every subscriber.
- **Transmitting and tuning require an exclusive lock**, acquired explicitly,
  leased for a bounded time and renewed by heartbeat. It is released on
  request, on disconnect, or on expiry.
- Configuration writes are serialised and broadcast; device changes are
  refused unless the session is idle.
- The lock is an arbitration mechanism, not a safety mechanism. The PTT
  watchdog remains the thing that guarantees the radio unkeys.

## Consequences

- A client that crashes mid-transmission does not leave the station locked,
  and does not leave it keyed either.
- Clients must handle `tx_locked` as a normal outcome and show who holds it.
- An operator at the radio can take over from a remote client by waiting out
  the lease, which is deliberately short (30 seconds).
- Slightly more protocol surface: acquire, heartbeat, release, plus lock state
  in `session.status`.
