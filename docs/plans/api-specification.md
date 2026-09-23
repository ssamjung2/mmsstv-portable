# Station API specification

The contract between the core and every client: the CLI, the three front ends
and anything an operator writes themselves. [ADR-0005](../decisions/0005-api-protocol.md)
chose the shape (JSON-RPC control plane, binary data frames); this document
specifies it well enough to implement and to test against.

Behaviour behind these calls is in [session behaviour](session-behaviour.md);
the objects they carry are in [the data model](data-model.md).

## Transport and framing

| | Local | Remote | Embedded (iOS) |
| --- | --- | --- | --- |
| Transport | Unix domain socket | WebSocket over TLS | Direct calls |
| Default location | `$XDG_RUNTIME_DIR/pocketsstvd.sock` | port 4532 is hamlib's; we use **4544**, off by default | — |
| Auth | File permissions (0600, owner only) | Bearer token, per-client, revocable | Process boundary |
| Framing | Length-prefixed frames | WebSocket frames | Callbacks |

The control plane is **newline-delimited JSON-RPC**: one message per line, so
`nc -U` with `grep` or `jq` is a working client. Bulk data — waterfall
columns, scan lines, picture tiles — travels on an **optional second channel**
whose frames carry a `u32` length, a `u64` stream id and a `u64` timestamp.

A client that never opens the data channel still receives every control event.
Because the two planes are separate connections, ordering between them is
**established by sequence number, not arrival order**: each bulk payload is
announced on the control plane with a sequence number that the binary frame
repeats. See [ADR-0005](../decisions/0005-api-protocol.md) for the measurements
behind this.

## Versioning and skew

- The API has its own semantic version, independent of the application's.
- A client sends `hello` with the version it was built against. The daemon
  accepts any client with the same major version and a minor version at or
  below its own, and reports its own version back.
- **Additive changes only** within a major version: new methods, new optional
  parameters, new event fields. Clients ignore fields they do not know.
- Removing or changing the meaning of anything requires a major bump, an entry
  in [the changelog](../../CHANGELOG.md), and one release of overlap where the
  old form still works and warns.
- Mismatch is an explicit error (`version_unsupported`) naming both versions,
  never a mysterious failure. See
  [ADR-0010](../decisions/0010-versioning-and-compatibility.md).

The schema lives in `clients/schema/` as JSON Schema, is the source for the
generated C and Dart clients, and is what the contract tests run against.

## Methods

Grouped by noun. Every method returns `{ok: true, ...}` or a structured error.

### session

| Method | Parameters | Returns / effect |
| --- | --- | --- |
| `session.status` | — | State, mode, current line, signal quality, lock holder, since |
| `session.startRx` | `mode?` | `Idle`/`Hunting` → listening. With `mode`, starts receiving immediately (`R-RX-3`) |
| `session.stopRx` | — | → `Idle`, releases capture |
| `session.abortRx` | — | Abandons the current picture, stores it as `partial` |
| `session.acquireTx` | `lease_seconds?` | Transmit lock; returns a lock id and expiry |
| `session.heartbeatTx` | `lock` | Extends the lease |
| `session.releaseTx` | `lock` | Releases early |
| `session.transmit` | `lock`, `picture`, `mode`, `template?`, `fields?`, `interrupt?` | Renders, then runs the keying sequence |
| `session.tune` | `lock`, `frequency?`, `seconds?` | Tune tone for repeater access (`R-TX-6`) |
| `session.abortTx` | `lock` | Stops audio, unkeys |
| `session.acknowledgeFault` | — | Leaves `Fault` once release is verified |

### audio

| Method | Parameters | Returns / effect |
| --- | --- | --- |
| `audio.devices` | — | Inputs and outputs: id, name, rates, channels, current default |
| `audio.select` | `input?`, `output?` | Persists the choice by stable device id; rejected unless `Idle` |
| `audio.levels` | — | Input and output level, clipping counters |
| `audio.setGain` | `input?`, `output?` | Software gain (`R-AUD-3`) |
| `audio.calibrate` | `direction`, `method` | Starts clock calibration (`R-AUD-5`), reports ppm when done |
| `audio.testTone` | `lock`, `seconds` | Drive-level calibration tone with peak reporting (`R-TX-11`) |

### rig

| Method | Parameters | Returns / effect |
| --- | --- | --- |
| `rig.status` | — | Connected, model, frequency, mode, PTT state, **keying capability** (`confirmed` or `assumed`), last error |
| `rig.configure` | `backend`, `model?`, `port?`, `baud?`, `host?` | hamlib model or network `rigctld` |
| `rig.setFrequency` / `rig.setMode` | value | `R-RIG-3` |
| `rig.ptt` | `lock`, `state` | Manual keying, for testing the interface |
| `rig.keyingConfigure` | `method`, options | serial RTS/DTR, CAT, CM108, GPIO, VOX |
| `rig.keyingTest` | `lock` | Asserts and releases, reports what was observed (`R-RIG-5`) |

### pictures, library, log, config

| Method | Parameters | Returns / effect |
| --- | --- | --- |
| `pictures.import` | `path` or bytes, `fit?` | Decodes, fits to a mode's geometry, returns a picture id |
| `pictures.render` | `picture`, `mode`, `template?`, `fields?` | Returns the exact pixels that would be transmitted (`R-TX-1`) |
| `pictures.adjust` | `picture`, crop/rotate/brightness/contrast/sharpen | New picture id; originals are never modified |
| `library.query` | `since?`, `until?`, `callsign?`, `mode?`, `band?`, `partial?`, `limit`, `cursor?` | Page of records (`R-LIB-2`) |
| `library.get` / `library.delete` / `library.export` | `id`(s) | Undo window on delete (10 s) |
| `library.repair` | — | Rebuilds the index from files on disk |
| `log.add` / `log.update` / `log.query` | contact fields | RSV, not RST (`R-LOG-5`) |
| `log.importAdif` / `log.exportAdif` | `path`, `filter?` | `R-LOG-2` |
| `config.get` / `config.set` / `config.describe` | `key`, `value` | `describe` returns type, default, range and help text, so a UI can build a settings screen without hard-coding it |
| `templates.list` / `templates.get` / `templates.put` | template document | See [the data model](data-model.md#template-documents) |
| `system.info` | — | Versions, platform, uptime, device and rig summary |
| `system.diagnostics` | `redact?` | Diagnostics bundle path (`R-OPS-2`) |

### Development

Selected by configuration, not by a build flag, so a shipping binary can
reproduce a problem from an operator's own recording
([ADR-0011](../decisions/0011-simulation-first-development.md)).

| Method | Parameters | Returns / effect |
| --- | --- | --- |
| `dev.feed` | `file` | Plays a 16-bit mono WAV through the fake audio device, as though it had arrived from the radio. Returns the sample rate and length |

## Events

Clients subscribe with `events.subscribe {topics, rates?}`. Every event has a
monotonic sequence number, so a reconnecting client can tell whether it missed
anything.

| Topic | Payload | Rate |
| --- | --- | --- |
| `session.state` | Old state, new state, reason | On change |
| `rx.pictureStarted` | Picture id, mode, start time | On change |
| `rx.line` | Picture id, line index; pixels on the binary stream | Per line |
| `rx.quality` | Signal level, sync stability, timing error, dropped lines | 2 Hz |
| `rx.pictureComplete` | Picture id, `complete` or `partial`, metadata | On change |
| `waterfall.column` | Binary: FFT magnitudes | Configurable, default 20 Hz |
| `tx.progress` | Line, fraction, remaining seconds | 2 Hz |
| `ptt.state` | Asserted, released, method, `confirmed` (false when the backend cannot read the line back) | On change |
| `audio.level` | Input and output peak and RMS, clipping | 10 Hz |
| `audio.deviceLost` / `audio.deviceRestored` | Device id, reason | On change |
| `audio.sourceEnded` | Reason | When a finite source, such as a recording, runs out |
| `rig.state` | Frequency, mode, connection | On change, ≤2 Hz |
| `fault.raised` / `fault.cleared` | Code, detail, remediation | On change |
| `log.changed`, `library.changed`, `config.changed` | Ids or keys affected | On change |

### Back-pressure

High-rate topics (`rx.line`, `waterfall.column`, `audio.level`) are **lossy by
design**. Each subscription has a bounded queue; past a soft limit those events
are dropped and counted, and the count rides along on the next event the client
does receive, as a `dropped` field. A slow phone on a weak Wi-Fi link must
never stall the decoder. Low-rate topics (state, faults, completion) are
**never dropped**: past a hard limit the client is disconnected instead,
because a client that missed a fault is dangerous.

This is implemented and tested: with a subscriber that stops reading entirely,
a decode still completes, and the subscriber is told how many high-rate events
it missed. The client sockets are non-blocking for the same reason — a
blocking write to a full socket freezes the whole station.

## Errors

```json
{"jsonrpc":"2.0","id":7,"error":{"code":-32001,
 "message":"transmit lock held by another client",
 "data":{"kind":"tx_locked","holder":"pi-touchscreen","expires_in":22,
         "remedy":"Wait for the lease to expire or ask that client to release it"}}}
```

`kind` is the stable, machine-readable identifier; `message` is for logs;
`remedy` is shown to humans. Clients switch on `kind`, never on prose.

| `kind` | Meaning |
| --- | --- |
| `version_unsupported` | Client and daemon major versions differ |
| `unauthorized` | Missing or revoked token |
| `tx_locked`, `no_tx_lock`, `lock_expired` | Transmit arbitration |
| `busy_receiving` | Transmit refused mid-picture without `interrupt` |
| `device_unavailable`, `device_busy` | Audio device gone or held elsewhere |
| `rig_error`, `keying_error` | With the backend's own message in `data` |
| `fault_active` | Station is in `Fault`; only `acknowledgeFault` is accepted |
| `invalid_image`, `unsupported_format`, `geometry_mismatch` | Picture import and render |
| `storage_full`, `storage_readonly` | Disk and database |
| `not_found`, `invalid_argument`, `unsupported_in_state` | Ordinary argument and state errors |

Rule: **no error is a bare string**, and every error a human can cause carries
a `remedy` that says what to do.

## Security

- The local socket is `0600`, owned by the daemon's user (`N-6`).
- Network access is off by default. Turning it on mints a token; tokens are
  per-client, named, listed and revocable, and shown once.
- TLS with a self-signed certificate by default, fingerprint shown for
  out-of-band checking; a real certificate can be configured.
- Pairing: `pocketsstv pair` prints a QR code with host, port, fingerprint and token
  so a phone can join without typing (`R-NET-2`).
- mDNS advertisement (`_pocketsstv._tcp`) is opt-in and off by default (`R-NET-1`).
- Rate limits on authentication attempts; all failures logged.
- Every parameter is validated against the schema before dispatch. Image bytes
  from the network are treated as hostile and decoded in a restricted path —
  see [the test strategy](test-strategy.md#fuzzing).

## Example: receive, then reply

```text
→ {"method":"hello","params":{"api":"1.0","client":"sstv-cli/0.3"}}
← {"result":{"api":"1.0","daemon":"0.3.1","platform":"linux-arm64"}}
→ {"method":"events.subscribe","params":{"topics":["session.state","rx.*","fault.*"]}}
→ {"method":"session.startRx"}
← event session.state {from:"Idle",to:"Hunting"}
← event rx.pictureStarted {picture:"p_01H…",mode:"Martin 1"}
← event rx.line {picture:"p_01H…",line:0}          (pixels on stream 3)
← event rx.pictureComplete {picture:"p_01H…",status:"complete"}
→ {"method":"session.acquireTx","params":{"lease_seconds":120}}
← {"result":{"lock":"L-91f2","expires_in":120}}
→ {"method":"session.transmit","params":{"lock":"L-91f2","picture":"p_00A…",
    "mode":"Martin 1","template":"t_reply","fields":{"their_call":"G0ABC","rsv":"595"}}}
← event ptt.state {asserted:true,method:"serial-rts",confirmed:true}
← event tx.progress {line:120,fraction:0.47,remaining_s:60}
← event session.state {from:"Unkeying",to:"Hunting"}
```

The CLI is this conversation with a nicer face, which is the point: anything
the GUI can do is scriptable (`R-CLI-5`).
