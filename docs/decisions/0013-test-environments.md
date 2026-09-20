# 0013: Virtual devices and containers instead of a large hardware bench

- Status: Accepted
- Date: 2026-09-19

## Context

The plan assumed a hardware bench for anything touching audio, keying or
radios. For a solo developer that bench is expensive, becomes a pet nobody can
rebuild, and blocks CI entirely: a GitHub runner has no sound card.

Linux provides virtual devices that exercise the real driver paths, and
containers make those environments reproducible and disposable.

## Decision

Five test layers, with hardware only where physics demands it:

| Layer | Environment | Cadence | Proves |
| --- | --- | --- | --- |
| L0 | In-process fakes, any machine | Every push | Logic and contracts |
| L1 | Containers with kernel virtual devices | Every push | Real ALSA, libgpiod, serial and hamlib code paths |
| L2 | Container on real Pi hardware, devices passed through | Nightly | Real USB audio, timing, thermals, memory |
| L3 | Physical loopback and a radio into a dummy load | Weekly, pre-release | Electrical keying, audio path, VOX |
| L4 | On the air | Beta | Propagation, real stations |

The virtual devices at L1: `snd-aloop` for audio loopback, `gpio-sim` for Pi
GPIO keying, `tty0tty` for serial with working modem-control lines, and
hamlib's dummy backend and rig simulators for CAT.

One **device conformance suite** runs the same assertions against fake,
virtual and real backends, so a fake that diverges from reality fails in CI
rather than on the bench.

## Consequences

- Most of what was bench-only becomes ordinary CI work, and tests more than
  our own fakes do, because the kernel is in the loop.
- The bench shrinks to: a Pi, two audio interfaces, a keying-sense circuit and
  a radio into a dummy load. It is scripted and rebuildable.
- Containers on a Pi need `--device /dev/snd`, `/dev/gpiochip0` and the serial
  device, plus `--cap-add=SYS_NICE` and `--ulimit rtprio=99`: without
  real-time priority the audio measurements are meaningless.
- Kernel modules load on the **host**, not in the container, so runners must
  be able to `modprobe`. `tty0tty` is out-of-tree and needs DKMS.
- Emulated arm64 (QEMU) is acceptable for unit tests and useless for timing.
  Performance runs on native arm64 hardware.
- Three assumptions need a spike before we depend on them: module
  availability on the CI runners, `tty0tty` building on Raspberry Pi OS, and
  whether containerised audio on the Pi matches bare-metal latency.
