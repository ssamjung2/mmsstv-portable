# Start here

mmsstv-portable is a C/C++ port of the SSTV encoder and decoder from MMSSTV.
It builds two libraries, `libsstv_encoder` and `libsstv_decoder`, and a few
command-line tools. It covers all 43 MMSSTV modes, including MMSSTV's
MR/MP/ML and narrow modes.

## Build, test, try it

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j
(cd build/tests && ctest --output-on-failure)

./bin/encode_wav /tmp/s1.wav "Scottie 1"
./bin/decode_wav /tmp/s1.wav /tmp/s1.ppm
```

Executables are written to `bin/` in the repository root.

## Reading path

1. [README.md](README.md): what the library does, the mode table, build
   options and API examples.
2. [ENCODER.md](ENCODER.md): how an image becomes audio (headers, tones, line
   structure).
3. [DECODER_ARCHITECTURE_BASELINE.md](DECODER_ARCHITECTURE_BASELINE.md): how
   audio becomes an image. Then [DECODER_STATUS.md](DECODER_STATUS.md) for
   known limitations.
4. [../tests/README.md](../tests/README.md): what each test checks.
5. [MMSSTV_ARCHITECTURE_ANALYSIS.md](MMSSTV_ARCHITECTURE_ANALYSIS.md): where
   to look in the MMSSTV source when you need to check a detail.

Everything else is listed in [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md).

## Ground rules

- The MMSSTV source (`../mmsstv/`, Shift-JIS; convert with
  `iconv -c -f CP932 -t UTF-8`) and the SSTV Handbook
  ([sstv-handbook.pdf](sstv-handbook.pdf)) are the authorities. Where this
  library differs from MMSSTV on purpose, the reference documents say so.
- Documents marked **Historical record** at the top are kept for history and
  contain known errors; the banner lists them. Don't use them as a
  specification.
- Before changing timing, filters or the VIS logic, run the full test suite;
  `test_roundtrip` encodes and decodes all 43 modes and catches most
  regressions.
- Open work is listed in [PRODUCTION_READY_PLAN.md](PRODUCTION_READY_PLAN.md).
