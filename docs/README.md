# libsstv - Portable SSTV Encoder/Decoder Library

A lightweight, portable C/C++ library for encoding and decoding SSTV (Slow Scan Television) signals.

**Based on MMSSTV** by Makoto Mori (JE3HHT) and Nobuyuki Oba

## Status

- ✅ **Encoder:** Production ready (all 43 modes)
- 🔄 **Decoder:** In development (VIS detection, image decoding)

## Features

### Encoder (Complete)
- ✅ **43 SSTV modes** supported (Robot, Scottie, Martin, PD, Pasokon, etc.)
- ✅ **VIS code encoding** for automatic mode detection
- ✅ **WAV file output** with proper RIFF headers
- ✅ **Portable** - Works on Linux, macOS, Raspberry Pi, Windows
- ✅ **Fast** - Generates audio faster than real-time

### Decoder (In Progress)
- 🔄 **VIS code detection** - Automatic mode identification
- 🔄 **DSP pipeline** - IIR/FIR filters, AGC, tone detection
- 🔄 **Image demodulation** - RGB/YC color decoding
- ⏸️ **Sync tracking** - Horizontal sync and slant correction
- ⏸️ **AFC** - Automatic frequency control for radio drift

## Documentation

**NEW - Decoder Architecture:**
- [`DECODER_ARCHITECTURE_BASELINE.md`](DECODER_ARCHITECTURE_BASELINE.md) - Complete decoder pipeline architecture
- [`DECODER_STATUS.md`](DECODER_STATUS.md) - Current implementation status

**Encoder Documentation:**
- [`DOCUMENTATION_INDEX.md`](DOCUMENTATION_INDEX.md) - Complete documentation index
- [`DSP_CONSOLIDATED_GUIDE.md`](DSP_CONSOLIDATED_GUIDE.md) - DSP filter reference

## Supported SSTV Modes

| Mode | Resolution | Color | Duration |
|------|-----------|-------|----------|
| Robot 36 | 320×240 | Yes | 150s |
| Robot 72 | 320×240 | Yes | 300s |
| Scottie 1 | 320×256 | Yes | 428s |
| Scottie 2 | 320×256 | Yes | 278s |
| Martin 1 | 320×256 | Yes | 446s |
| Martin 2 | 320×256 | Yes | 227s |
| PD120 | 640×496 | Yes | 508s |
| PD180 | 640×496 | Yes | 754s |
| PD240 | 640×496 | Yes | 1000s |
| ... and 35 more modes |

## Quick Start

### Building

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

### Usage Example

```c
#include <sstv_encoder.h>

// Load RGB image data (320×256 for Scottie 1)
uint8_t *rgb_data = load_your_image();

// Create image structure
sstv_image_t image = sstv_image_from_rgb(rgb_data, 320, 256);

// Create encoder for Scottie 1 at 48kHz
sstv_encoder_t *encoder = sstv_encoder_create(SSTV_SCOTTIE1, 48000.0);
sstv_encoder_set_image(encoder, &image);
sstv_encoder_set_vis_enabled(encoder, 1);

// Generate audio samples
float samples[4096];
while (!sstv_encoder_is_complete(encoder)) {
    size_t n = sstv_encoder_generate(encoder, samples, 4096);
    // Write samples to audio output or file
    write_samples(samples, n);
}

sstv_encoder_free(encoder);
```

## Documentation

See the [API documentation](docs/API.md) for detailed information.

## License

This library is derived from MMSSTV and is licensed under LGPL v3.

Copyright (C) 2000-2013 Makoto Mori, Nobuyuki Oba (original MMSSTV)  
Copyright (C) 2026 (library port)

## Building from Source

### Requirements

- CMake 3.10 or later
- C++11 compatible compiler (GCC, Clang, MSVC)
- Standard C99 compiler

### Build Options

```bash
cmake -DBUILD_EXAMPLES=ON -DBUILD_SHARED=ON -DBUILD_STATIC=OFF ..
```

Options:
- `BUILD_EXAMPLES` - Build example programs (default: ON)
- `BUILD_SHARED` - Build shared library (default: ON)
- `BUILD_STATIC` - Build static library (default: ON)
- `BUILD_TESTS` - Build tests (default: OFF)

## Examples

The `examples/` directory contains:
- `list_modes.c` - List all available SSTV modes
- More examples coming soon...

## Project Status

🚧 **Work in Progress** - This library is currently under development.

Current status:
- [x] Project structure
- [ ] Mode definitions
- [ ] VCO implementation
- [ ] VIS encoder
- [ ] Main encoder
- [ ] Examples
- [ ] Tests

## Contributing

This is an extraction/port of MMSSTV for use as a library. Contributions welcome!

## Credits

This library is based on the excellent MMSSTV software by:
- Makoto Mori (JE3HHT)
- Nobuyuki Oba

MMSSTV: http://hamsoft.ca/pages/mmsstv.php
