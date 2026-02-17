# MMSSTV to Portable Library - Architecture Analysis & Porting Plan

**Analysis Date:** January 28, 2026  
**Source Code:** MMSSTV LGPL (Copyright 2000-2013 Makoto Mori, Nobuyuki Oba)  
**Target:** Cross-platform C/C++ library for *nix/macOS/Raspberry Pi

---

## Executive Summary

MMSSTV is a sophisticated Windows-based SSTV (Slow Scan Television) application built using Borland C++ Builder with VCL (Visual Component Library). The codebase contains excellent DSP and SSTV protocol implementations that can be extracted into a portable library. The main challenge is separating the core signal processing and SSTV logic from the Windows-specific UI and I/O layers.

**Key Statistics:**
- ~3089 lines in sstv.cpp (core SSTV logic)
- 43+ SSTV modes supported
- Comprehensive DSP implementation (FFT, FIR, IIR, PLL, VCO, LMS, etc.)
- Built-in JPEG support for image handling
- Full duplex TX/RX capabilities

---

## Current Architecture Analysis

### 1. Build System & Dependencies

**Build Environment:**
- Borland C++ Builder (.cbproj, .bpr project files)
- VCL framework (Visual Component Library) - Windows only
- Compiler: C++Builder with Borland-specific extensions (`__fastcall`, `__stdcall`)
- Forms: .dfm (Delphi Form) files for UI components

**Key Dependencies:**
- **VCL Components:** TForm, TCanvas, TBitmap, TImage, TPaintBox, TPanel, TThread
- **Windows APIs:** 
  - mmsystem.h (Wave audio I/O: waveInOpen, waveOutOpen, etc.)
  - Windows.h (HANDLE, CRITICAL_SECTION, threading, file I/O)
  - COM/OLE for certain features
- **JPEG Library:** Embedded libjpeg (in jpeg/ subdirectory) - **PORTABLE**
- **Math:** Standard C/C++ math library - **PORTABLE**

### 2. Core Component Architecture

The codebase is organized into distinct functional layers:

```
┌─────────────────────────────────────────────────────────┐
│                  UI Layer (Windows/VCL)                 │
│  Main.cpp/h, *Dlg.cpp/h, Draw.cpp/h, RxView, etc.     │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              Application Layer                          │
│  Sound.cpp/h (TSound thread), Comm.cpp/h (PTT/Radio)   │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│          Core SSTV Engine (PORTABLE CORE)               │
│  sstv.cpp/h - CSSTVDEM, CSSTVMOD, CSSTVSET             │
│  • Mode definitions and timing                          │
│  • VIS code encoding/decoding                           │
│  • Sync detection and tracking                          │
│  • Modulation/Demodulation                              │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│         DSP Processing Layer (PORTABLE)                 │
│  • Fft.cpp/h - FFT implementation                       │
│  • fir.cpp/h - FIR/IIR filters, LMS                     │
│  • VCO, PLL, Frequency detectors                        │
│  • Hilbert transformers, Notch filters                  │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│           I/O Abstraction Layer                         │
│  • Wave.cpp/h - Audio I/O (Windows MME)                 │
│  • ComLib.cpp/h - Utilities                             │
└─────────────────────────────────────────────────────────┘
```

### 3. SSTV Mode Definitions

**Supported Modes (43 modes):**

| Category | Modes | Resolution | Notes |
|----------|-------|------------|-------|
| **Robot** | R24, R36, R72, RM8, RM12 | 320x240, 160x120 | Color, grayscale |
| **Scottie** | SCT1, SCT2, SCTDX | 320x256 | Standard modes |
| **Martin** | MRT1, MRT2 | 320x256 | Popular modes |
| **Scottie C2** | SC2-60, SC2-120, SC2-180 | 320x256 | Variants |
| **PD** | PD50, PD90, PD120, PD160, PD180, PD240, PD290 | 320-800x256-616 | Variable resolution |
| **Pasokon** | P3, P5, P7 | 640x496 | High resolution |
| **Martin R** | MR73, MR90, MR115, MR140, MR175 | 320x256 | Fast variants |
| **Martin P** | MP73, MP115, MP140, MP175 | 320x256 | Precision modes |
| **Martin L** | ML180, ML240, ML280, ML320 | 640x496 | Large format |
| **Narrow** | MN73, MN110, MN140, MC110, MC140, MC180 | Various | Narrow bandwidth (1900Hz) |
| **AVT** | AVT 90 | 320x240 | Alternative |

**VIS Codes Implemented:**
Each mode has a unique VIS (Vertical Interval Signaling) code for auto-detection:
- Robot 36: 0x88
- Scottie 1: 0x3c
- Martin 1: 0xac
- PD120: 0x5f
- etc. (full mapping in sstv.cpp lines 2000-2100)

### 4. Key Classes & Data Structures

#### 4.1 Core SSTV Classes

**CSSTVSET** - Mode Configuration & Timing
```cpp
class CSSTVSET {
    int m_Mode;           // Current RX mode
    int m_TxMode;         // Current TX mode
    double m_TW;          // Timing width
    double m_KS, m_KS2;   // Scan timing parameters
    double m_OF, m_OFP;   // Offset parameters
    int m_WD, m_L;        // Width and line count
    double m_SampFreq;    // Sample frequency
    
    // Methods
    void SetMode(int mode);
    void GetBitmapSize(int &w, int &h, int mode);
    void GetPictureSize(int &w, int &h, int &hp, int mode);
    double GetTiming(int mode);
    void SetSampFreq(void);
};
```

**CSSTVDEM** - SSTV Demodulator/Decoder
```cpp
class CSSTVDEM {
    // Signal processing chain
    CIIRTANK m_iir11, m_iir12, m_iir13, m_iir19;  // Tone detectors
    CPLL m_pll;                                    // Phase-locked loop
    CFQC m_fqc;                                    // Frequency counter
    CLVL m_lvl;                                    // Level detector
    
    // Sync detection
    CSYNCINT m_sint1, m_sint2, m_sint3;           // Sync integrators
    int m_Sync, m_SyncMode;                        // Sync state
    int m_VisData, m_VisCnt;                       // VIS decoding
    
    // Image buffer
    short *m_Buf;                                  // Receive buffer
    int m_wLine;                                   // Current line
    
    // AFC (Auto Frequency Control)
    int m_afc, m_AFCCount;
    double m_AFCData, m_AFCLock;
    
    // Methods
    void Start(int mode, int f);
    void Do(double d);                             // Process one sample
    void CalcBPF(void);                            // Calculate bandpass filter
    void SyncFreq(double d);                       // Sync frequency tracking
};
```

**CSSTVMOD** - SSTV Modulator/Encoder
```cpp
class CSSTVMOD {
    CVCO m_vco;                                    // Voltage-controlled oscillator
    CFIR2 m_BPF;                                   // Bandpass filter
    
    short *m_TXBuf;                                // Transmit buffer
    int m_TXBufLen, m_TXMax;                       // Buffer management
    
    int m_wLine;                                   // Current TX line
    short *pRow;                                   // Current row data
    
    // Methods
    void Write(short fq);                          // Write frequency
    void Write(short fq, double tim);              // Write with timing
    double Do(void);                               // Generate one sample
    void WriteCWID(char c);                        // CW ID encoding
    void WriteFSK(BYTE c);                         // FSK encoding
};
```

#### 4.2 DSP Classes (All Portable)

**CVCO** - Voltage Controlled Oscillator
```cpp
class CVCO {
    double *pSinTbl;                               // Sine lookup table
    int m_TableSize;
    double m_SampleFreq, m_FreeFreq;
    double m_z;                                    // Phase accumulator
    
    void SetFreeFreq(double f);
    void SetGain(double gain);
    double Do(double d);                           // Generate sample
};
```

**CPLL** - Phase-Locked Loop
```cpp
class CPLL {
    CVCO vco;
    CIIR loopLPF, outLPF;                         // Loop filters
    double m_err, m_out, m_vcoout;
    double m_vcogain, m_outgain;
    
    void SetFreeFreq(double f1, double f2);
    double Do(double d);
    double GetErr(void);
};
```

**CFIR2** - FIR Filter (Double Buffer)
```cpp
class CFIR2 {
    int m_Tap;
    double *m_pZ;                                  // Delay line (Z-buffer)
    double *m_pH;                                  // Filter coefficients
    
    void Create(int tap, int type, double fs, 
                double fcl, double fch, 
                double att, double gain);
    double Do(double d);                           // Filter one sample
};
```

**CIIR** - IIR Filter (Biquad)
```cpp
class CIIR {
    double a0, a1, a2;                            // Feedforward coefficients
    double b1, b2;                                // Feedback coefficients
    double z1, z2;                                // Delay elements
    
    void MakeIIR(double fc, double q, int type);
    double Do(double d);
};
```

**CFFT** - Fast Fourier Transform
```cpp
class CFFT {
    double m_CollectFFTBuf[3][2048];              // Collection buffers
    double *m_tSinCos, *m_tWindow;                // Trig tables, window
    int m_fft[2048];                              // Output spectrum
    
    void CollectFFT(double *lp, int size);
    void CalcFFT(int size, double gain, int stg);
    void TrigFFT(void);
};
```

**CLMS** - Least Mean Squares Adaptive Filter
```cpp
class CLMS {
    double *Z;                                     // FIR delay line
    double *D;                                     // LMS delay
    double *H;                                     // Adaptive coefficients
    int m_Tap;
    
    void Init(int tap);
    double Do(double d, double m);
};
```

**CFQC** - Frequency Counter/Detector
```cpp
class CFQC {
    CIIR m_iir;
    CSmooz m_fir;
    double m_out, m_fq;
    
    double Do(double s);                           // Returns detected frequency
};
```

### 5. Audio I/O Architecture

**Current Implementation (Windows MME):**
```cpp
class CWave {
    // Windows-specific
    HWAVEIN m_hin;
    HWAVEOUT m_hout;
    WAVEFORMATEX m_WFX;
    WAVEHDR *m_pInBuff[WAVE_FIFO_MAX];
    WAVEHDR *m_pOutBuff[WAVE_FIFO_MAX];
    
    // Thread synchronization
    CRITICAL_SECTION m_InCS, m_OutCS;
    HANDLE m_InEvent, m_OutEvent;
    
    // FIFO management
    int m_InWP, m_InRP, m_OutWP, m_OutRP;
    
    BOOL InOpen(int sampfreq, int size);
    BOOL OutOpen(int sampfreq, int size);
    BOOL InRead(double *p, int len);
    BOOL OutWrite(double *p, int len);
};
```

**TSound** - Main Audio Thread
```cpp
class TSound : public TThread {
    CWave Wave;                                    // Audio device
    CSSTVDEM SSTVDEM;                              // Demodulator
    CSSTVMOD SSTVMOD;                              // Modulator
    CFFT fftIN;                                    // Input FFT
    CWaveFile WaveFile;                            // File I/O
    
protected:
    void Execute();                                // Main thread loop
    
public:
    void SetTXRX(int sw);                         // TX/RX switching
    void ReadWrite(double *s, int size);          // Process audio block
};
```

### 6. Sync Detection & VIS Decoding

**Sync Detection Process:**
1. **Initial Detection:** Monitor 1200Hz tone (30ms minimum)
2. **VIS Decoding:** Decode 8-bit VIS code using 1100Hz/1300Hz FSK
3. **Mode Start:** Initialize demodulator for detected mode
4. **Horizontal Sync:** Track sync pulses for each scan line
5. **Resync:** Adaptive sync tracking with AFC

**VIS Code Structure:**
```
Break (300ms 1900Hz) → Leader (300ms 1900Hz) → 
Start Bit (30ms 1200Hz) → 8 Data Bits → Stop Bit (30ms 1200Hz)

Data bits: 30ms each
  - 1100Hz = Logic 1
  - 1300Hz = Logic 0
```

**Sync Tracking:**
```cpp
class CSYNCINT {
    DWORD m_MSyncList[MSYNCLINE];                 // Sync interval history
    int m_MSyncIntPos, m_MSyncIntMax;
    
    int SyncCheck(void);                           // Check sync validity
    void SyncTrig(int d);                          // Trigger on sync pulse
    int SyncStart(void);                           // Start of sync detection
};
```

### 7. Image Data Flow

**RX (Receive) Path:**
```
Audio In → BPF → Frequency Detector → Color Decoder → Line Buffer → 
Slant Correction → Image Buffer → Display/Save
```

**TX (Transmit) Path:**
```
Image File → Color Encoder → Line Scanner → Frequency Modulator → 
BPF → Audio Out
```

**Color Encoding Schemes:**

Different modes use different color encoding:

1. **RGB Sequential** (Martin, Scottie):
   - Green line → Blue line → Red line → Sync
   
2. **YC (Luminance/Chrominance)** (Robot):
   - Y (luminance) → R-Y → B-Y → Sync
   
3. **RGB Parallel** (PD modes):
   - Y-Sync → R line → G line → B line

**Pixel Rate Calculation:**
```cpp
// For each mode, calculate pixel duration
pixel_duration_ms = line_time_ms / pixels_per_line

// E.g., Scottie 1 (smSCT1):
// Line time: 138.24ms per color component
// Pixels: 320
// Pixel duration: 138.24 / 320 = 0.4320ms per pixel
```

### 8. Slant Correction & Timing

**Slant Adjustment:**
- Caused by clock mismatch between TX and RX
- MMSSTV implements adaptive slant correction
- Manual and automatic modes available
- Correction applied during scan line assembly

**Clock Adjustment:**
```cpp
// In ClockAdj.cpp/h
class TClockAdjDlg {
    // Stores clock offset per mode
    // Applied during demodulation
    // Persists in configuration
};
```

### 9. File I/O & Image Handling

**Supported Formats:**
- JPEG (via embedded libjpeg) - **PRIMARY**
- BMP (Windows DIB format)
- Native SSTV formats (.sst)

**Image Processing:**
```cpp
// Draw.cpp contains:
- Bitmap manipulation
- Color conversion
- Perspective transformation
- Text overlay
- OLE object support (Windows-specific)
```

### 10. Radio Control & PTT

```cpp
class CComm {
    HANDLE m_fHnd;                                 // Serial port handle
    DCB m_dcb;                                     // Device Control Block
    
    BOOL Open(LPCTSTR PortName);                   // Open COM port
    void SetPTT(int sw);                           // Push-to-talk control
    void SetScan(int sw);                          // Scanning control
};

class CRadio {
    // CAT (Computer Aided Transceiver) control
    // Supports various radio protocols
    // Frequency control
    // Mode switching
};
```

---

## Platform Dependencies Analysis

### Critical Dependencies (Require Replacement)

| Component | Current | Portable Alternative |
|-----------|---------|---------------------|
| **Audio I/O** | Windows MME (waveIn/waveOut) | PortAudio, RtAudio, or ALSA/Core Audio |
| **Threading** | TThread (VCL), CRITICAL_SECTION | C++11 std::thread, std::mutex |
| **UI Components** | VCL (TForm, TCanvas, TBitmap) | None needed for library (callbacks) |
| **File I/O** | Windows FILE API, AnsiString | C++ std::fstream, std::string |
| **Serial Port** | Windows COM API | libserialport, termios |
| **Timers** | TTimer (VCL) | std::chrono, timer_create |
| **Image Format** | TBitmap (Windows DIB) | Raw RGB/YUV buffers |

### Portable Components (Can Reuse)

| Component | Status | Notes |
|-----------|--------|-------|
| **JPEG Library** | ✓ Portable | Already cross-platform |
| **FFT** | ✓ Portable | Pure C++ implementation |
| **FIR/IIR Filters** | ✓ Portable | Pure math, no dependencies |
| **VCO/PLL** | ✓ Portable | Pure DSP algorithm |
| **LMS Filter** | ✓ Portable | Self-contained |
| **SSTV Logic** | ✓ Mostly Portable | Remove UI dependencies |
| **Mode Definitions** | ✓ Fully Portable | Static data tables |
| **VIS Encoding/Decoding** | ✓ Fully Portable | Pure algorithm |
| **Sync Detection** | ✓ Fully Portable | No dependencies |

---

## Proposed Library Architecture

### Design Goals

1. **Zero UI Dependencies** - Pure signal processing library
2. **Callback-Based** - User provides data sources/sinks
3. **Platform Agnostic** - No OS-specific code in core
4. **Modern C++** - Use C++11/14 features where appropriate
5. **Simple API** - High-level interface for common tasks
6. **Extensible** - Allow custom modes and filters

### Library Structure

```
libsstv/
├── include/
│   └── libsstv/
│       ├── sstv.h              // Main public API
│       ├── modes.h             // Mode definitions
│       ├── encoder.h           // Encoding API
│       ├── decoder.h           // Decoding API
│       ├── types.h             // Common types
│       └── version.h           // Version info
│
├── src/
│   ├── core/
│   │   ├── sstv_modes.cpp      // Mode timing/config
│   │   ├── vis_codec.cpp       // VIS encode/decode
│   │   ├── sync_detector.cpp   // Sync detection
│   │   └── color_codec.cpp     // Color encoding schemes
│   │
│   ├── dsp/
│   │   ├── fft.cpp             // FFT implementation
│   │   ├── fir.cpp             // FIR filters
│   │   ├── iir.cpp             // IIR filters
│   │   ├── vco.cpp             // VCO oscillator
│   │   ├── pll.cpp             // Phase-locked loop
│   │   ├── lms.cpp             // LMS adaptive filter
│   │   ├── hilbert.cpp         // Hilbert transform
│   │   └── freq_detector.cpp   // Frequency detection
│   │
│   ├── codec/
│   │   ├── decoder.cpp         // SSTV decoder
│   │   ├── encoder.cpp         // SSTV encoder
│   │   ├── slant.cpp           // Slant correction
│   │   └── afc.cpp             // Auto frequency control
│   │
│   └── util/
│       ├── ringbuffer.cpp      // Lock-free ring buffer
│       ├── resampler.cpp       // Sample rate conversion
│       └── image.cpp           // Image buffer management
│
├── examples/
│   ├── decode_wav.cpp          // Decode SSTV from WAV file
│   ├── encode_image.cpp        // Encode image to SSTV
│   ├── realtime_rx.cpp         // Real-time RX with PortAudio
│   └── realtime_tx.cpp         // Real-time TX with PortAudio
│
├── tests/
│   ├── test_modes.cpp
│   ├── test_vis.cpp
│   ├── test_dsp.cpp
│   └── test_codec.cpp
│
├── docs/
│   ├── API.md
│   ├── MODES.md
│   └── EXAMPLES.md
│
├── CMakeLists.txt
├── README.md
└── LICENSE (LGPL v3)
```

### Public API Design

```cpp
// libsstv/sstv.h - Main public header
#ifndef LIBSSTV_H
#define LIBSSTV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Library version
#define LIBSSTV_VERSION_MAJOR 1
#define LIBSSTV_VERSION_MINOR 0
#define LIBSSTV_VERSION_PATCH 0

// Return codes
typedef enum {
    SSTV_OK = 0,
    SSTV_ERROR = -1,
    SSTV_INVALID_PARAM = -2,
    SSTV_OUT_OF_MEMORY = -3,
    SSTV_INVALID_MODE = -4,
    SSTV_BUFFER_FULL = -5,
    SSTV_BUFFER_EMPTY = -6,
    SSTV_NOT_SYNCED = -7
} sstv_result_t;

// SSTV Modes (enum matches original mode IDs)
typedef enum {
    SSTV_MODE_R36 = 0,
    SSTV_MODE_R72,
    SSTV_MODE_AVT90,
    SSTV_MODE_SCOTTIE1,
    SSTV_MODE_SCOTTIE2,
    SSTV_MODE_SCOTTIEDX,
    SSTV_MODE_MARTIN1,
    SSTV_MODE_MARTIN2,
    SSTV_MODE_SC2_180,
    SSTV_MODE_SC2_120,
    SSTV_MODE_SC2_60,
    SSTV_MODE_PD50,
    SSTV_MODE_PD90,
    SSTV_MODE_PD120,
    SSTV_MODE_PD160,
    SSTV_MODE_PD180,
    SSTV_MODE_PD240,
    SSTV_MODE_PD290,
    SSTV_MODE_P3,
    SSTV_MODE_P5,
    SSTV_MODE_P7,
    // ... all 43 modes
    SSTV_MODE_AUTO = -1  // Auto-detect mode
} sstv_mode_t;

// Pixel format
typedef enum {
    SSTV_PIXEL_RGB24,     // 24-bit RGB (8-8-8)
    SSTV_PIXEL_RGBA32,    // 32-bit RGBA (8-8-8-8)
    SSTV_PIXEL_GRAY8      // 8-bit grayscale
} sstv_pixel_format_t;

// Image structure
typedef struct {
    uint8_t *data;                  // Pixel data
    size_t width;                   // Image width
    size_t height;                  // Image height
    size_t stride;                  // Bytes per row
    sstv_pixel_format_t format;     // Pixel format
} sstv_image_t;

// Mode information
typedef struct {
    sstv_mode_t mode;
    const char *name;               // E.g., "Scottie 1"
    size_t width;                   // Image width
    size_t height;                  // Image height
    double duration_ms;             // Total transmission time (ms)
    uint8_t vis_code;               // VIS code for auto-detection
    bool is_color;                  // Color or B&W
    bool is_narrow;                 // Narrow bandwidth mode
} sstv_mode_info_t;

// Forward declarations
typedef struct sstv_decoder_s sstv_decoder_t;
typedef struct sstv_encoder_s sstv_encoder_t;

//==============================================================================
// DECODER API
//==============================================================================

// Create a decoder instance
sstv_decoder_t* sstv_decoder_create(double sample_rate);

// Free decoder
void sstv_decoder_free(sstv_decoder_t *decoder);

// Set decoder mode (use SSTV_MODE_AUTO for auto-detection)
sstv_result_t sstv_decoder_set_mode(sstv_decoder_t *decoder, sstv_mode_t mode);

// Enable/disable features
sstv_result_t sstv_decoder_enable_afc(sstv_decoder_t *decoder, bool enable);
sstv_result_t sstv_decoder_enable_slant_correction(sstv_decoder_t *decoder, bool enable);

// Process audio samples (mono, float, -1.0 to +1.0)
sstv_result_t sstv_decoder_process(
    sstv_decoder_t *decoder,
    const float *samples,
    size_t num_samples
);

// Get decoder status
bool sstv_decoder_is_synced(sstv_decoder_t *decoder);
sstv_mode_t sstv_decoder_get_detected_mode(sstv_decoder_t *decoder);
float sstv_decoder_get_progress(sstv_decoder_t *decoder);  // 0.0 to 1.0

// Get decoded image (non-blocking, returns NULL if not ready)
sstv_image_t* sstv_decoder_get_image(sstv_decoder_t *decoder);

// Callbacks
typedef void (*sstv_sync_callback_t)(void *user_data, sstv_mode_t mode);
typedef void (*sstv_line_callback_t)(void *user_data, int line_number);
typedef void (*sstv_complete_callback_t)(void *user_data, sstv_image_t *image);

sstv_result_t sstv_decoder_set_sync_callback(
    sstv_decoder_t *decoder,
    sstv_sync_callback_t callback,
    void *user_data
);

sstv_result_t sstv_decoder_set_line_callback(
    sstv_decoder_t *decoder,
    sstv_line_callback_t callback,
    void *user_data
);

sstv_result_t sstv_decoder_set_complete_callback(
    sstv_decoder_t *decoder,
    sstv_complete_callback_t callback,
    void *user_data
);

// Reset decoder
void sstv_decoder_reset(sstv_decoder_t *decoder);

//==============================================================================
// ENCODER API
//==============================================================================

// Create an encoder instance
sstv_encoder_t* sstv_encoder_create(
    sstv_mode_t mode,
    double sample_rate
);

// Free encoder
void sstv_encoder_free(sstv_encoder_t *encoder);

// Set source image
sstv_result_t sstv_encoder_set_image(
    sstv_encoder_t *encoder,
    const sstv_image_t *image
);

// Enable VIS code transmission
sstv_result_t sstv_encoder_enable_vis(sstv_encoder_t *encoder, bool enable);

// Generate audio samples (returns number of samples generated)
size_t sstv_encoder_generate(
    sstv_encoder_t *encoder,
    float *samples,
    size_t max_samples
);

// Check if encoding is complete
bool sstv_encoder_is_complete(sstv_encoder_t *encoder);

// Get progress (0.0 to 1.0)
float sstv_encoder_get_progress(sstv_encoder_t *encoder);

// Reset encoder
void sstv_encoder_reset(sstv_encoder_t *encoder);

//==============================================================================
// MODE INFORMATION API
//==============================================================================

// Get mode information
const sstv_mode_info_t* sstv_get_mode_info(sstv_mode_t mode);

// Get all available modes
const sstv_mode_info_t* sstv_get_all_modes(size_t *count);

// Find mode by VIS code
sstv_mode_t sstv_find_mode_by_vis(uint8_t vis_code);

// Find mode by name
sstv_mode_t sstv_find_mode_by_name(const char *name);

//==============================================================================
// UTILITY API
//==============================================================================

// Get library version
const char* sstv_get_version(void);

// Get error string
const char* sstv_get_error_string(sstv_result_t result);

// Image allocation helpers
sstv_image_t* sstv_image_create(size_t width, size_t height, sstv_pixel_format_t format);
void sstv_image_free(sstv_image_t *image);
sstv_result_t sstv_image_copy(sstv_image_t *dst, const sstv_image_t *src);

#ifdef __cplusplus
}
#endif

#endif // LIBSSTV_H
```

### C++ High-Level API

```cpp
// libsstv/sstv.hpp - C++ wrapper
#ifndef LIBSSTV_HPP
#define LIBSSTV_HPP

#include <libsstv/sstv.h>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <stdexcept>

namespace sstv {

class Exception : public std::runtime_error {
public:
    explicit Exception(sstv_result_t result)
        : std::runtime_error(sstv_get_error_string(result))
        , m_result(result) {}
    
    sstv_result_t result() const { return m_result; }
private:
    sstv_result_t m_result;
};

class Image {
public:
    Image(size_t width, size_t height, sstv_pixel_format_t format = SSTV_PIXEL_RGB24)
        : m_img(sstv_image_create(width, height, format), sstv_image_free) {
        if (!m_img) throw Exception(SSTV_OUT_OF_MEMORY);
    }
    
    size_t width() const { return m_img->width; }
    size_t height() const { return m_img->height; }
    uint8_t* data() { return m_img->data; }
    const uint8_t* data() const { return m_img->data; }
    
    sstv_image_t* handle() { return m_img.get(); }
    
private:
    std::unique_ptr<sstv_image_t, decltype(&sstv_image_free)> m_img;
};

class Decoder {
public:
    using SyncCallback = std::function<void(sstv_mode_t)>;
    using LineCallback = std::function<void(int)>;
    using CompleteCallback = std::function<void(std::shared_ptr<Image>)>;
    
    explicit Decoder(double sample_rate)
        : m_decoder(sstv_decoder_create(sample_rate), sstv_decoder_free) {
        if (!m_decoder) throw Exception(SSTV_OUT_OF_MEMORY);
    }
    
    void setMode(sstv_mode_t mode) {
        auto result = sstv_decoder_set_mode(m_decoder.get(), mode);
        if (result != SSTV_OK) throw Exception(result);
    }
    
    void enableAFC(bool enable) {
        auto result = sstv_decoder_enable_afc(m_decoder.get(), enable);
        if (result != SSTV_OK) throw Exception(result);
    }
    
    void enableSlantCorrection(bool enable) {
        auto result = sstv_decoder_enable_slant_correction(m_decoder.get(), enable);
        if (result != SSTV_OK) throw Exception(result);
    }
    
    void process(const std::vector<float>& samples) {
        auto result = sstv_decoder_process(
            m_decoder.get(),
            samples.data(),
            samples.size()
        );
        if (result != SSTV_OK) throw Exception(result);
    }
    
    bool isSynced() const {
        return sstv_decoder_is_synced(m_decoder.get());
    }
    
    sstv_mode_t getDetectedMode() const {
        return sstv_decoder_get_detected_mode(m_decoder.get());
    }
    
    float getProgress() const {
        return sstv_decoder_get_progress(m_decoder.get());
    }
    
    void onSync(SyncCallback callback) {
        m_sync_callback = std::move(callback);
        sstv_decoder_set_sync_callback(
            m_decoder.get(),
            [](void *user_data, sstv_mode_t mode) {
                auto self = static_cast<Decoder*>(user_data);
                if (self->m_sync_callback) {
                    self->m_sync_callback(mode);
                }
            },
            this
        );
    }
    
    void reset() {
        sstv_decoder_reset(m_decoder.get());
    }
    
private:
    std::unique_ptr<sstv_decoder_t, decltype(&sstv_decoder_free)> m_decoder;
    SyncCallback m_sync_callback;
    LineCallback m_line_callback;
    CompleteCallback m_complete_callback;
};

class Encoder {
public:
    explicit Encoder(sstv_mode_t mode, double sample_rate)
        : m_encoder(sstv_encoder_create(mode, sample_rate), sstv_encoder_free) {
        if (!m_encoder) throw Exception(SSTV_OUT_OF_MEMORY);
    }
    
    void setImage(const Image& image) {
        auto result = sstv_encoder_set_image(m_encoder.get(), image.handle());
        if (result != SSTV_OK) throw Exception(result);
    }
    
    void enableVIS(bool enable) {
        auto result = sstv_encoder_enable_vis(m_encoder.get(), enable);
        if (result != SSTV_OK) throw Exception(result);
    }
    
    std::vector<float> generate(size_t max_samples) {
        std::vector<float> samples(max_samples);
        size_t generated = sstv_encoder_generate(
            m_encoder.get(),
            samples.data(),
            max_samples
        );
        samples.resize(generated);
        return samples;
    }
    
    bool isComplete() const {
        return sstv_encoder_is_complete(m_encoder.get());
    }
    
    float getProgress() const {
        return sstv_encoder_get_progress(m_encoder.get());
    }
    
    void reset() {
        sstv_encoder_reset(m_encoder.get());
    }
    
private:
    std::unique_ptr<sstv_encoder_t, decltype(&sstv_encoder_free)> m_encoder;
};

} // namespace sstv

#endif // LIBSSTV_HPP
```

---

## Detailed Porting Plan

### Phase 1: Foundation & Infrastructure (Week 1-2)

**1.1 Project Setup**
- [x] Create CMake build system
- [x] Set up directory structure
- [x] Configure build options (shared/static library)
- [x] Set up testing framework (CTest + custom C tests)
- [ ] Create CI/CD pipeline (GitHub Actions)
- [x] License compliance (ensure LGPL v3)

**1.2 Core Type Definitions**
- [x] Extract and port mode enumerations (sstv.h)
- [x] Define portable types (replace Windows types)
- [x] Create mode information tables
- [x] Port timing tables and VIS code mappings
- [x] Document mode specifications

**1.3 DSP Foundation**
- [ ] Port mathematical constants and utilities
- [ ] Create portable filter coefficient generation
- [ ] Port window functions (Hamming, Hann, Blackman)
- [ ] Port FFT trig table generation

### Phase 2: DSP Components (Week 3-4)

**2.1 Basic DSP Classes**
- [x] Port CVCO (VCO oscillator)
  - Remove VirtualLock (Windows-specific)
  - Use std::vector for sine table
  - Add unit tests
  
- [ ] Port CIIR (IIR filter)
  - Pure algorithm, minimal changes
  - Add frequency response tests
  
- [ ] Port CFIR2 (FIR filter)
  - Replace dynamic allocation with std::vector
  - Add unit tests for various filter types

**2.2 Advanced DSP**
- [ ] Port CPLL (Phase-locked loop)
  - Integrate CVCO and CIIR
  - Test lock acquisition
  
- [ ] Port CFQC (Frequency counter)
  - Test frequency detection accuracy
  
- [ ] Port CLMS (LMS adaptive filter)
  - Validate convergence behavior
  
- [ ] Port CIIRTANK (IIR tank/resonator)
  - Used for tone detection

**2.3 FFT Implementation**
- [ ] Port CFFT class
  - Remove Windows messaging (m_Handle, CM_FFT)
  - Use callback for FFT results
  - Test with known signals

**2.4 Utility Classes**
- [ ] Port CSmooz (averaging/smoothing)
- [ ] Port CScope (data collection)
  - Remove UI dependencies
  - Provide data access methods
- [ ] Port CLVL (level detection/AGC)
- [ ] Port CNoise (noise generator for testing)

### Phase 3: SSTV Core Logic (Week 5-6)

**3.1 Mode Configuration**
- [x] Port CSSTVSET class
  - Extract mode timing calculations
  - Port SetSampFreq() methods
  - Port GetTiming(), GetBitmapSize(), GetPictureSize()
  - Validate all 43 modes
  - Create mode lookup functions

**3.2 Sync Detection**
- [ ] Port CSYNCINT (sync integrator)
  - SyncTrig(), SyncMax(), SyncCheck()
  - Interval validation
  
- [ ] Port VIS code decoder (RX)
  - Extract from CSSTVDEM::Do()
  - Create standalone vis_decode()
    - Add VIS encoder for TX
  - Test all VIS codes

**3.3 Decoder Implementation**
- [ ] Port CSSTVDEM class
  - Extract from UI dependencies
  - Remove Graphics::TBitmap references
  - Replace with raw buffer (RGB24/RGB888)
  - Port tone detectors (m_iir11, m_iir12, m_iir13, m_iir19)
  - Port PLL and FQC
  - Port sync detection logic
  - Port AFC (Auto Frequency Control)
  - Port slant correction
  - Implement line buffering
  - Implement progressive decoding callbacks

**3.4 Encoder Implementation**
- [x] Port CSSTVMOD class
  - Remove TXBuf Windows-specific allocation
  - Use std::vector for buffers
  - Port VCO modulation
  - Port color encoding logic
  - Port VIS transmission
  - Port FSK encoding (for callsign ID)
  - Implement progress tracking

### Phase 4: Image Handling (Week 7)

**4.1 Image Buffer Management**
- [x] Create portable image structure
  - RGB24, RGBA32, GRAY8 formats
  - Stride/row alignment handling
  
- [ ] Port JPEG integration
  - Wrap libjpeg API
  - Load JPEG → SSTV format
  - Save SSTV format → JPEG
  - Handle color conversion

**4.2 Color Conversion**
- [ ] RGB ↔ YUV conversion
- [x] RGB ↔ R-Y/B-Y conversion
- [ ] Gamma correction
- [ ] Color bar generation (for testing)

### Phase 5: API Implementation (Week 8)

**5.1 C API**
- [ ] Implement decoder API
  - sstv_decoder_create/free
  - sstv_decoder_process
  - sstv_decoder_get_image
  - Callback mechanisms
  
- [x] Implement encoder API
  - sstv_encoder_create/free
  - sstv_encoder_set_image
  - sstv_encoder_generate
  
- [x] Implement mode info API
- [x] Implement utility functions

**5.2 C++ API**
- [ ] RAII wrappers
- [ ] STL integration
- [ ] Exception handling
- [ ] Modern C++ idioms

### Phase 6: Testing & Validation (Week 9-10)

**6.1 Unit Tests**
- [ ] DSP component tests
  - VCO frequency accuracy
  - Filter frequency response
  - FFT accuracy
  - PLL lock time
  
- [ ] Mode tests
  - Timing calculations
  - VIS codes
  - Bitmap sizes
  
- [ ] Codec tests
  - Encode/decode round trip
  - VIS detection
  - Sync detection

**6.2 Integration Tests**
- [ ] Test with real SSTV signals
- [ ] Test all 43 modes
- [ ] Test with noisy signals
- [ ] Test AFC behavior
- [ ] Test slant correction

**6.3 Performance Tests**
- [ ] Real-time decode capability
- [ ] Memory usage
- [ ] CPU usage
- [ ] Optimize hot paths

### Phase 7: Examples & Documentation (Week 11)

**7.1 Example Applications**
- [x] WAV file decoder (scaffold)
  - Read WAV, decode SSTV, save JPEG
  
- [x] WAV file encoder
  - Read JPEG, encode SSTV, save WAV
  
- [ ] Real-time RX (PortAudio)
  - Live audio input
  - Display progress
  - Save received images
  
- [ ] Real-time TX (PortAudio)
  - Load image
  - Generate SSTV
  - Output to audio

**7.2 Documentation**
- [ ] API reference (Doxygen)
- [ ] Mode specifications
- [ ] Usage examples
- [ ] Build instructions
- [ ] Porting notes

### Phase 8: Packaging & Distribution (Week 12)

**8.1 Build System**
- [x] CMake install targets
- [x] pkg-config support
- [x] Static/shared library options
- [ ] Cross-compilation support

**8.2 Platform Testing**
- [ ] Linux (x86_64, ARM)
- [ ] macOS (x86_64, ARM64/M1)
- [ ] Raspberry Pi (ARM)
- [ ] FreeBSD (optional)

**8.3 Packaging**
- [ ] Debian/Ubuntu packages
- [ ] Homebrew formula (macOS)
- [ ] AUR package (Arch Linux)
- [ ] Source tarball release

---

## Key Technical Challenges

### 1. Floating Point Precision

**Issue:** Original code uses double precision throughout.  
**Solution:** Maintain double precision for DSP, convert at API boundary if needed.

### 2. Sample Rate Flexibility

**Issue:** Original code hardcodes SampFreq in many places.  
**Solution:** Make sample rate a parameter passed to all objects.

### 3. Thread Safety

**Issue:** Original uses Windows CRITICAL_SECTION.  
**Solution:** Use std::mutex, ensure all public APIs are thread-safe or documented as not.

### 4. Memory Management

**Issue:** Original uses new/delete, sometimes with manual tracking.  
**Solution:** Use RAII, std::unique_ptr, std::vector for automatic management.

### 5. Callback Performance

**Issue:** Callbacks can impact real-time performance.  
**Solution:** Make callbacks optional, use lock-free ring buffers where possible.

### 6. VIS Code Robustness

**Issue:** VIS detection can fail with noise.  
**Solution:** Port existing averaging and error checking, add confidence scoring.

### 7. Slant Correction Algorithm

**Issue:** Complex adaptive algorithm tied to UI.  
**Solution:** Extract algorithm, provide manual override + auto modes.

### 8. Clock Tolerance

**Issue:** Different sample rates between TX and RX.  
**Solution:** Port existing clock adjustment, add sample rate conversion option.

---

## Code Extraction Strategy

### High Priority (Core Functionality)

1. **sstv.cpp** - Lines 490-3089
   - Mode tables (lines 493-546)
   - VIS codes (lines 2000-2100)
   - SetSampFreq() (lines 660-1110)
   - GetTiming() (lines 1188-1280)
   - Sync detection (lines 1900-2100)
   - Demodulation loop

2. **fir.cpp/h** - Entire files
   - MakeFilter()
   - CFIR2 class
   - CLMS class

3. **Fft.cpp/h** - Core FFT
   - Remove UI callbacks
   - Keep algorithm

4. **sstv.h** - Class definitions
   - CVCO, CPLL, CFQC (lines 30-200)
   - CSSTVSET (lines 501-551)
   - CSSTVDEM (lines 590-770)
   - CSSTVMOD (lines 772-854)

### Medium Priority (Enhanced Features)

5. **Sound.cpp** - Audio processing loop
   - Extract processing logic
   - Remove Wave I/O
   - Extract LMS, notch filter usage

6. **jpeg/** - JPEG library
   - Already portable
   - Create wrapper API

### Low Priority (Optional Features)

7. **Comm.cpp/h** - Serial/PTT control
   - Can be separate utility library
   - Not core to SSTV codec

8. **LogFile.cpp** - Logging/QSO database
   - Application-level feature
   - Not needed in library

---

## Build System Design

### CMakeLists.txt Structure

```cmake
cmake_minimum_required(VERSION 3.12)
project(libsstv VERSION 1.0.0 LANGUAGES C CXX)

# Options
option(LIBSSTV_BUILD_SHARED "Build shared library" ON)
option(LIBSSTV_BUILD_STATIC "Build static library" ON)
option(LIBSSTV_BUILD_EXAMPLES "Build example programs" ON)
option(LIBSSTV_BUILD_TESTS "Build tests" ON)
option(LIBSSTV_ENABLE_JPEG "Enable JPEG support" ON)

# C++11 minimum
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
set(LIBSSTV_SOURCES
    src/core/sstv_modes.cpp
    src/core/vis_codec.cpp
    src/core/sync_detector.cpp
    src/core/color_codec.cpp
    src/dsp/fft.cpp
    src/dsp/fir.cpp
    src/dsp/iir.cpp
    src/dsp/vco.cpp
    src/dsp/pll.cpp
    src/dsp/lms.cpp
    src/dsp/hilbert.cpp
    src/dsp/freq_detector.cpp
    src/codec/decoder.cpp
    src/codec/encoder.cpp
    src/codec/slant.cpp
    src/codec/afc.cpp
    src/util/ringbuffer.cpp
    src/util/resampler.cpp
    src/util/image.cpp
)

# Headers
set(LIBSSTV_HEADERS
    include/libsstv/sstv.h
    include/libsstv/modes.h
    include/libsstv/encoder.h
    include/libsstv/decoder.h
    include/libsstv/types.h
    include/libsstv/version.h
)

# Library targets
if(LIBSSTV_BUILD_SHARED)
    add_library(sstv SHARED ${LIBSSTV_SOURCES})
    set_target_properties(sstv PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )
endif()

if(LIBSSTV_BUILD_STATIC)
    add_library(sstv_static STATIC ${LIBSSTV_SOURCES})
    set_target_properties(sstv_static PROPERTIES
        OUTPUT_NAME sstv
    )
endif()

# Include directories
target_include_directories(sstv PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# JPEG support
if(LIBSSTV_ENABLE_JPEG)
    find_package(JPEG REQUIRED)
    target_link_libraries(sstv PRIVATE JPEG::JPEG)
    target_compile_definitions(sstv PRIVATE LIBSSTV_HAVE_JPEG)
endif()

# Examples
if(LIBSSTV_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(LIBSSTV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS sstv
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${LIBSSTV_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libsstv
)

# pkg-config
configure_file(libsstv.pc.in libsstv.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/libsstv.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
```

---

## Testing Strategy

### 1. Unit Tests (Per Component)

```cpp
// tests/test_vco.cpp
TEST_CASE("VCO generates correct frequency") {
    VCO vco(48000.0);  // 48kHz sample rate
    vco.setFreeFreq(1900.0);  // 1900Hz
    
    // Generate 1 second of samples
    std::vector<double> samples(48000);
    for (size_t i = 0; i < samples.size(); i++) {
        samples[i] = vco.process(0.0);
    }
    
    // Verify frequency via FFT
    auto freq = detectPeakFrequency(samples, 48000.0);
    REQUIRE(freq == Approx(1900.0).epsilon(0.01));
}
```

### 2. Integration Tests

```cpp
// tests/test_encode_decode.cpp
TEST_CASE("Encode and decode round trip") {
    // Create test image
    Image img(320, 256, SSTV_PIXEL_RGB24);
    fillTestPattern(img);
    
    // Encode
    Encoder encoder(SSTV_MODE_SCOTTIE1, 48000.0);
    encoder.setImage(img);
    
    auto samples = encoder.generateAll();
    
    // Decode
    Decoder decoder(48000.0);
    decoder.setMode(SSTV_MODE_SCOTTIE1);
    decoder.process(samples);
    
    auto decoded = decoder.getImage();
    REQUIRE(decoded != nullptr);
    
    // Compare images (allow some lossy difference)
    auto similarity = compareImages(img, *decoded);
    REQUIRE(similarity > 0.95);  // 95% similar
}
```

### 3. Signal Tests (Real SSTV Samples)

```cpp
// tests/test_real_signals.cpp
TEST_CASE("Decode real SSTV signal") {
    auto samples = loadWAV("test_data/scottie1_sample.wav");
    
    Decoder decoder(8000.0);  // Sample rate from WAV
    decoder.setMode(SSTV_MODE_AUTO);
    
    bool synced = false;
    decoder.onSync([&](sstv_mode_t mode) {
        synced = true;
        REQUIRE(mode == SSTV_MODE_SCOTTIE1);
    });
    
    decoder.process(samples);
    REQUIRE(synced);
    REQUIRE(decoder.getImage() != nullptr);
}
```

---

## Performance Targets

| Operation | Target | Notes |
|-----------|--------|-------|
| **Decode** | < 1x realtime | On Raspberry Pi 4 |
| **Encode** | < 0.5x realtime | Should be faster than decode |
| **Memory** | < 10MB | Per decoder/encoder instance |
| **Latency** | < 100ms | From audio in to line callback |
| **FFT** | < 5ms | For 2048-point FFT @ 48kHz |
| **Mode Detection** | < 2s | VIS code + sync detection |

---

## Migration Path for Existing Users

For users currently using MMSSTV on Windows who want to migrate to the library-based solution:

### 1. Direct File Conversion Tool
```bash
# Decode SSTV from WAV
sstv-decode input.wav output.jpg --mode auto

# Encode image to SSTV
sstv-encode input.jpg output.wav --mode scottie1
```

### 2. Integration with Existing Applications

**FLDIGI Integration:**
- Create FLDIGI modem plugin using libsstv
- Allows SSTV in FLDIGI environment

**QSSTV Replacement:**
- Build GUI application using libsstv
- Qt-based for cross-platform GUI
- Compatible workflow with MMSSTV

**Command-Line Tools:**
- Real-time decode: `sstv-rx --audio pulse --output-dir ~/sstv`
- Real-time encode: `sstv-tx --image callsign.jpg --mode pd120`

### 3. Python Bindings

```python
import pysstv

# Decode
decoder = pysstv.Decoder(sample_rate=48000)
decoder.set_mode(pysstv.MODE_AUTO)
decoder.enable_afc(True)

with open('audio.raw', 'rb') as f:
    samples = np.frombuffer(f.read(), dtype=np.float32)
    decoder.process(samples)
    
if decoder.is_synced():
    image = decoder.get_image()
    image.save('output.jpg')

# Encode
encoder = pysstv.Encoder(pysstv.MODE_SCOTTIE1, 48000)
encoder.set_image_file('input.jpg')
encoder.enable_vis(True)

samples = encoder.generate_all()
with open('output.raw', 'wb') as f:
    f.write(samples.tobytes())
```

---

## License Compliance

**Source Code License:** LGPL v3 (as per original MMSSTV)

**Key Requirements:**
1. Derived work must remain LGPL v3
2. Must provide source code access
3. Must credit original authors (Makoto Mori, Nobuyuki Oba)
4. Must document changes made

**License Headers:**
```cpp
// libsstv - Portable SSTV Codec Library
// Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
// 
// Copyright (C) 2000-2013 Makoto Mori, Nobuyuki Oba (original MMSSTV)
// Copyright (C) 2026 [Your Name/Organization] (library port)
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
```

---

## Conclusion

This comprehensive analysis reveals that MMSSTV contains a sophisticated and well-designed SSTV implementation that can be successfully ported to a cross-platform library. The core DSP and SSTV logic is largely portable, with Windows dependencies primarily in the UI and I/O layers.

**Estimated Effort:** 12 weeks for full implementation with single developer

**Success Criteria:**
- ✓ All 43 SSTV modes supported
- ✓ VIS code auto-detection
- ✓ AFC and slant correction
- ✓ Real-time capable on Raspberry Pi 4
- ✓ Simple, well-documented API
- ✓ Comprehensive test coverage
- ✓ Example applications provided

The resulting library will provide the amateur radio and SSTV community with a modern, portable, and maintainable SSTV codec implementation suitable for integration into various applications and platforms.

---

**Next Steps:**
1. Review and approve this analysis
2. Set up development environment
3. Begin Phase 1 (Foundation & Infrastructure)
4. Establish regular testing with real SSTV signals
5. Engage with amateur radio community for feedback

