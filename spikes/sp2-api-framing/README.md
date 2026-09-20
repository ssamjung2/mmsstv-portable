# SP-2: API framing measurement

Throwaway prototype from the SP-2 spike, kept only because the Raspberry Pi
Zero 2 W measurement is still outstanding and this is the way to reproduce it.
**Not production code, not built by CMake, no tests.** Delete it once
[ADR-0005](../../docs/decisions/0005-api-protocol.md) has its Zero figure.

It implements the two transport shapes that were compared:

- `--framed` — one socket, every message length-prefixed with a type byte.
- `--ndjson` — newline-delimited JSON control plane, with bulk data on an
  optional second socket. This is the shape that was chosen.

## Reproduce

```sh
cc -O2 -o sp2_daemon sp2_daemon.c && cc -O2 -o sp2_client sp2_client.c
./sp2_daemon --ndjson --rate 20 --seconds 5 --path /tmp/sp2.sock &
./sp2_client --ndjson --path /tmp/sp2.sock
```

On the Pi Zero 2 W, copy the two sources and `run_on_zero.sh` across and run
the script; it exercises both modes at 20 Hz and a 200 Hz stress case, and
prints CPU and latency.

Results measured so far, and the decision they produced, are recorded in
[ADR-0005](../../docs/decisions/0005-api-protocol.md).
