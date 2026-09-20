# Building and installing

Requirements: CMake 3.10 or newer, a C++11 compiler and a C99 compiler. No
external dependencies. Developed and tested on macOS; the code is portable C
and C++ and is intended to build on Linux and Raspberry Pi as well.

```bash
cmake -S . -B build
cmake --build build -j
```

## The station application

PocketSSTV — the daemon and its command-line client — builds from the same
tree, with presets:

```bash
cmake --preset dev          # Debug, tests on
cmake --build --preset dev -j
```

That produces `bin/pocketsstvd` (the daemon) and `bin/pocketsstv` (the CLI).
To try them:

```bash
./bin/pocketsstvd --socket /tmp/pocketsstvd.sock &
./bin/pocketsstv --socket /tmp/pocketsstvd.sock info
```

The control socket speaks newline-delimited JSON-RPC, so a shell is a
first-class client:

```bash
printf '{"jsonrpc":"2.0","id":1,"method":"hello","params":{"api":"1.0"}}\n' \
  | nc -U /tmp/pocketsstvd.sock
```

Set `BUILD_STATION=OFF` to build only the signal libraries.

## Options

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_SHARED` | ON | Shared libraries |
| `BUILD_STATIC` | ON | Static libraries |
| `BUILD_RX` | ON | Build the decoder library and the decoder tools |
| `BUILD_EXAMPLES` | ON | Command-line tools, see [cli-tools.md](cli-tools.md) |
| `BUILD_TESTS` | OFF | Test programs and CTest registration, see [the test suite](../../tests/README.md) |
| `BUILD_UTILS` | OFF | Diagnostic programs in `utils/` (filter and VCO probes) |
| `BUILD_STATION` | ON | The PocketSSTV station core, daemon and CLI |

For example, a decoder-only build with tests:

```bash
cmake -S . -B build -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=ON
cmake --build build -j
```

## Output locations

All executables and shared libraries are written to `<repo>/bin/`, whatever
the build directory is called: the top-level `CMakeLists.txt` sets the runtime
output directory explicitly. Static libraries are written to the build
directory.

Note that building overwrites anything already in `bin/`.

## Installing

```bash
cmake --install build --prefix /usr/local
```

This installs:

- `libsstv_encoder` and `libsstv_decoder` (shared and static, depending on the
  options above) into `<prefix>/lib`,
- `sstv_encoder.h` and `sstv_decoder.h` into `<prefix>/include`,
- `sstv_encoder.pc` into `<prefix>/lib/pkgconfig`.

The pkg-config file covers the encoder only. To link the decoder, name it
directly:

```bash
cc myapp.c $(pkg-config --cflags --libs sstv_encoder) -lsstv_decoder -lm
```

## Using the library in another CMake project

The libraries have no install-time CMake package config yet, so add this
repository as a subdirectory and link the targets directly:

```cmake
add_subdirectory(mmsstv-portable)
target_link_libraries(myapp PRIVATE sstv_encoder sstv_decoder)
```

Available targets: `sstv_encoder`, `sstv_encoder_static`, `sstv_decoder` and
`sstv_decoder_static`.
