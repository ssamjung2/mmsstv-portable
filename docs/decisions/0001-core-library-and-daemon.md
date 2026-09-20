# 0001: All logic in a core library, deployed as a daemon or embedded

- Status: Accepted
- Date: 2026-09-19

## Context

The Raspberry Pi is the priority platform, and Pi stations are often headless
or driven from another machine. The CLI is to stay first-class. Three separate
GUIs are planned. Meanwhile iOS does not permit a long-running background
daemon process at all.

## Decision

All behaviour lives in `libsstv_station`. It is deployed two ways:

- **Hosted**: `pocketsstvd` owns the hardware; clients connect over a local socket.
  Used on Raspberry Pi and Linux, optional on macOS.
- **Embedded**: the library is linked into the application and called
  in-process. Required on iOS.

Both expose the same API vocabulary, so a front end is written once against a
client library that hides the difference.

## Consequences

- The GUIs stay thin, which is what makes three of them affordable.
- The CLI is a client of the same API, so scriptability is structural rather
  than an afterthought.
- A Pi station can be driven from a laptop or a phone without extra work.
- We pay for an IPC layer: a schema to version, serialisation costs, and
  tests for two deployment shapes.
- The embedded path must not become a second implementation. Same dispatch
  code, different transport.
