# Standards and reference material

What SSTV *is*, independent of this codebase. These documents describe the
protocol and the reference sources; nothing here depends on how the library is
written, and they change only if the standards do.

| Document | Contents |
| --- | --- |
| [sstv-signal-format.md](sstv-signal-format.md) | The protocol reference: tones, preamble, VIS, 16-bit VIS, N-VIS, the AVT header and the scan-line structure of every mode family |
| [avt.md](avt.md) | SSTV Handbook text on the AVT modes, with a note on what this library implements |
| [wraase-sc2.md](wraase-sc2.md) | SSTV Handbook text on the Wraase SC-2 modes, with a note on how MMSSTV's timing differs |
| [s-units-and-dbm.md](s-units-and-dbm.md) | S-meter units, dBm, and the signal levels used when testing under HF conditions |
| [sstv-handbook.pdf](sstv-handbook.pdf) | The SSTV Handbook itself. Its LaTeX source is in `sstv-handbook/` |

## The two authorities

This project treats two sources as authoritative:

1. **The SSTV Handbook**, for what the modes are and what a conforming signal
   looks like.
2. **The MMSSTV source** (expected at `../mmsstv/`), for what stations
   actually transmit and accept. Its files are Shift-JIS encoded, so convert
   before reading or searching:
   `iconv -c -f CP932 -t UTF-8 ../mmsstv/sstv.cpp > /tmp/sstv.cpp`.

They disagree in places. This library follows MMSSTV, because
interoperability with real stations matters more than matching a document, and
each such difference is recorded where it applies: the Wraase SC-2 channel
split, the AVT variants, the image sizes for Robot and AVT, and MMSSTV's odd
VIS parity for B/W 12. The differences are collected in
[the MMSSTV source map](../specs/mmsstv-source-map.md).
