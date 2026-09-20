# Decision records

One file per architectural decision that would be expensive to reverse, in the
order they were made. Each records what we chose, why, and what it costs us.

They are dated and immutable: when a decision changes, we add a new record
that supersedes the old one rather than editing history. If you are about to
ask "why on earth is it done this way", the answer should be here.

**Status** is `Accepted` (decided, build against it), `Proposed` (a
recommendation waiting for a decision) or `Superseded by NNNN`.

| ADR | Decision | Status |
| --- | --- | --- |
| [0001](0001-core-library-and-daemon.md) | All logic in a core library, deployed as a daemon or embedded | Accepted |
| [0002](0002-three-front-ends.md) | A different UI toolkit per platform family | Accepted |
| [0003](0003-hamlib.md) | hamlib for rig control, keying kept separate | Accepted |
| [0004](0004-audio-backend.md) | miniaudio behind our own audio abstraction | Proposed — [spike SP-1](../plans/feature-backlog.md#spikes-before-m0-closes) |
| [0005](0005-api-protocol.md) | JSON-RPC control plane with binary data frames | Proposed — [spike SP-2](../plans/feature-backlog.md#spikes-before-m0-closes) |
| [0006](0006-licensing-and-app-store.md) | Keep LGPL parts dynamically linked and relinkable | Accepted |
| [0007](0007-template-rendering.md) | Templates render in the core, not in front ends | Proposed |
| [0008](0008-storage.md) | SQLite index, content-addressed image files, TOML settings | Proposed — [spike SP-3](../plans/feature-backlog.md#spikes-before-m0-closes) |
| [0009](0009-transmit-arbitration.md) | Transmit is an exclusive lease, everything else is shared | Accepted |
| [0010](0010-versioning-and-compatibility.md) | Five components, one compatibility rule | Accepted |
| [0011](0011-simulation-first-development.md) | The whole stack runs without hardware | Accepted |
| [0012](0012-risk-tiered-methodology.md) | Rigor by blast radius, not uniformly | Accepted |
| [0013](0013-test-environments.md) | Virtual devices and containers instead of a large hardware bench | Accepted |
| [0014](0014-regulatory-guardrails.md) | The software helps the operator stay legal | Accepted |
| [0015](0015-licensing-of-new-components.md) | Apache-2.0 for new components, LGPL core unchanged | Accepted |
