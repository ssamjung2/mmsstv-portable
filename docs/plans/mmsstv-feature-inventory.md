# What MMSSTV does, and what we should keep

An inventory of the original application, taken before designing its
replacement. The library port covers MMSSTV's signal processing; this document
covers everything *around* it — the parts an operator actually touches.

Sources: the English manual (`../mmsstv/EMMSSTV.TXT`), the **version history**
(`EUPDATE.TXT`), the auxiliary specifications shipped with the program
(`Repeater.txt`, `fskid.txt`, `mode.txt`), the 40 Borland form files, the
shipped settings profile (`Mmsstv English.ini`, 1118 keys) and the source
units. Form files are binary Delphi (`TPF0`) with Shift-JIS captions,
so the settings file and the manual are the readable inventory.

The verdicts (**Keep**, **Modernise**, **Replace**, **Drop**) are proposals
that feed [the requirements](application-requirements.md). They are opinions,
not findings.

## The original at a glance

| | |
| --- | --- |
| UI surface | 40 forms, ~370 menu items, one 411 KB main unit |
| Main window | Five tabs: RX, TX, Template, Sync, History, plus stock gallery |
| Configuration | ~1100 settings keys, 8 demodulator profiles |
| Platform | Windows, Borland C++ Builder VCL, ~2000–2013 |
| Licence | LGPL v3 |

## Receive

| What it does | Verdict | Notes |
| --- | --- | --- |
| Auto-start on VIS, or on sync-pulse interval when VIS is missed | **Keep** | Both paths matter on HF; the library detects VIS and N-VIS already |
| Four-level squelch on the start trigger | **Modernise** | Keep the behaviour, express it as one "sensitivity" control with a meaningful scale |
| Auto stop, auto restart, auto resync | **Keep** | |
| Manual mode buttons to start mid-picture | **Keep** | Essential when you tune in late |
| Slant correction: high-accuracy (least squares over ≥16 lines), automatic, manual two-click | **Modernise** | The library now corrects drift automatically; keep a manual override and the "remember this clock" action, drop the two-click ritual in favour of a drag |
| Phase/sync fine tune by clicking the sync line | **Modernise** | Same idea, direct manipulation on the image |
| Spectrum and waterfall with 1200/1500/1900/2300 Hz markers | **Keep** | The tuning aid operators rely on |
| RX history, FIFO, default 32 images, JPEG option, auto-save, auto-copy to folder | **Modernise** | Becomes a real library with search and metadata, not a ring buffer |
| Demodulator profiles (Hilbert, PLL, FQC), AFC, auto-notch (LMS), BPF, differentiator | **Modernise** | The port implements the Hilbert path; expose a small number of named presets rather than 30 numeric fields |

## Transmit

| What it does | Verdict | Notes |
| --- | --- | --- |
| "What you see is what you transmit" TX pane | **Keep** | The clearest idea in the whole application |
| Mode selection, or follow the last received mode | **Keep** | |
| TX button starts and aborts; auto-return to RX | **Keep** | |
| Tune tone (1750 Hz default) for repeaters | **Keep** | |
| CW identifier after transmission, with WPM, tone and macro text | **Keep** | Required by regulation in some regions |
| FSK identifier, MMV voice identifier | **Keep** FSK, **Drop** MMV | FSK ID is cheap; MMV was a Windows audio-clip feature |
| Separate TX/RX sound-card clock calibration | **Modernise** | Keep the capability, calibrate from measurement rather than by eye |
| 12-second sound buffer with FIFO tuning in the options | **Drop** | An artefact of 2000s Windows audio; modern callbacks handle this |

## Images, templates and the stock area

| What it does | Verdict | Notes |
| --- | --- | --- |
| Template overlay on the TX image, with a transparent colour | **Modernise** | Replace colour-keying with real alpha compositing and layers |
| Template items: line, rectangle, filled rectangle, text, picture, colour bar | **Keep** | |
| Macros in template text (`%c` their call, `%m` my call, RST, contest number) | **Keep** | This is what makes templates worth having; bind them to QSO fields |
| Stock area, up to 300 images, each with an associated template | **Modernise** | Becomes a gallery with tags and reusable "card" presets |
| The 16-line header convention, with four shift/adjust buttons | **Modernise** | Do it automatically from the mode's geometry; offer one "header band" toggle |
| Drag and drop from Explorer, clipboard paste, thumbnails of eight folders | **Keep** | Native equivalents on each platform |
| Image formats: BMP and JPEG | **Replace** | PNG, JPEG, WebP, HEIC in, PNG/JPEG out, with EXIF orientation honoured |
| Picture clipper, filters, bit-mask, perspective, colour-bar tools | **Modernise** | Keep crop, rotate, brightness/contrast and sharpen; drop the rest and defer to real editors |

## Logging

| What it does | Verdict | Notes |
| --- | --- | --- |
| QSO log with call, date, time, band, mode, RSV, QTH, remarks | **Keep** | |
| ADIF import and export | **Keep** | Already present (`LogList::LoadADIF` / `SaveADIF`); make it the primary format |
| QSL field storing the index of the received image | **Modernise** | Link QSOs to images properly, both directions |
| HAMLOG (Japanese) and MMLink integration with sister applications | **Drop** | Replaced by ADIF and, later, a local API |
| DXCC data file (`ARRL.DX`) | **Modernise** | Use a maintained country file |

## Radio control and keying

| What it does | Verdict | Notes |
| --- | --- | --- |
| PTT on a COM port via RTS/DTR, with PTT lock and "RTS on RX" | **Keep** | Still how most interfaces key |
| CAT as hand-written hex command strings per radio (`CmdRx`, `CmdTx`), with polling | **Replace** | This is the single clearest case for [hamlib](application-architecture.md#radio-control) |
| VOX with a configurable tone sequence | **Keep** | The primary path on macOS and iOS |
| Repeater tone sequences and definitions | **Keep** | |
| External program launcher with "suspend" to release the sound card and COM port | **Drop** | Device arbitration is the operating system's job now |

## Configuration and shell

| What it does | Verdict | Notes |
| --- | --- | --- |
| ~1100 INI keys across 40 dialogs | **Modernise** | A small, documented settings file, most keys derived rather than exposed |
| Two language INI profiles (English, Japanese) | **Replace** | Real internationalisation with translator workflow |
| Window geometry for every panel persisted in the INI | **Keep** | Per-platform, in platform-appropriate storage |
| Stay-on-top, priority class, "memory window" tuning | **Drop** | |

## Second pass: what the first inventory missed

The first pass used the manual and the form names. Reading the version history
and the auxiliary specifications turned up a substantial amount more, most of
it relevant to this project's priority platform.

### It is already an unattended station

**MMSSTV has a complete SSTV repeater mode** (`MMSSTV.EXE -r`, specified in
`Repeater.txt`), which the first inventory missed entirely:

| Feature | Detail |
| --- | --- |
| Tone access | Configurable tone (default 1750 Hz), detection time, sensitivity |
| CW identifier answer | Sent after an access tone, with a configurable wait |
| Replay | Receives a picture and retransmits it, optionally in the mode it arrived in |
| Beacon | Periodic transmission, with a channel-clear "silence time" check, its own mode, and a rotating list of templates and pictures |
| Channel occupancy | An auto-correlator squelch gates every transmission; beacons are postponed while the frequency is busy |
| Auto-save | Transmitted pictures written to a folder as JPEG |
| Supervision | A state machine with counters for answers, pictures received and sent, beacons, and the correlator level |

**Verdict: Keep, and treat as a first-class feature.** This is precisely the
Raspberry Pi use case, and its squelch-before-transmit discipline is a model
for our own unattended behaviour.

### Receive-side processing we did not plan for

| Feature | Verdict | Note |
| --- | --- | --- |
| Three demodulators: zero-cross, PLL, Hilbert, with documented trade-offs | **Modernise** | We ported Hilbert only. Zero-cross is the low-CPU option, which may matter on a Pi Zero 2 W |
| Notch filter, placed by clicking the spectrum | **Keep** | A carrier in the passband is an everyday HF condition |
| LMS adaptive filter: noise smoothing and automatic notch | **Keep** | `src/SpectralSubtractionDNR.cpp` exists but is unused |
| Automatic frequency control (AFC), improved for narrow modes | **Keep** | Not ported. Required for satellite work, where Doppler moves the signal |
| Level converter: linear or 17th-order polynomial, with a 20-second automatic calibration | **Later** | Compensates a non-linear demodulator; our Hilbert path is close to linear |
| Differentiator: high-frequency boost for sharpness | **Later** | |
| Demodulator profiles: 8 named parameter sets plus a read-only factory default | **Modernise** | Becomes a small set of named presets |
| Auto-correlator squelch | **Keep** | Needed by the repeater, useful on its own |
| Sample rates 8000–44100 Hz, FFT fixed at 2048 points with decimation above 18 kHz | **Keep** | Confirms our native-rate approach |
| Stereo source channel selection (left, right, mono) | **Keep** | Common with a shared interface |
| RTS line used to mute audio while receiving | **Drop** | Hardware-specific |

### Transmit and identification

| Feature | Verdict | Note |
| --- | --- | --- |
| **CW keyboard**: arbitrary CW messages with macros, plus seven presets (`QSL 73 TU`, `QRZ?`, `%c de %m` …) | **Keep** | We planned only an automatic identifier |
| **FSK identifier**: 45.45 baud Baudot, 1900/2100 Hz, specified in `fskid.txt`, with an optional contest number | **Keep** | Shares the narrow-mode FSK machinery we already ported |
| **VariSSTV**: per-colour transmit power shaping, proposed by Samuel Hunt, to protect the transmitter's final device | **Keep** | Directly relevant to the duty-cycle protection we lack |
| TX low-pass and band-pass filters, switchable | **Later** | Recommended on when sending CW ID |
| Tune tone with a satellite option | **Keep** | |
| Customisable VOX tone sequence | **Keep** | |

### Radio control and logging

| Feature | Verdict | Note |
| --- | --- | --- |
| **Radio command menu**: named frequency and mode presets sent as CAT macros | **Keep** | hamlib replaces the macro syntax; the preset list is the feature |
| External PTT through a plug-in DLL | **Replace** | Our keying backends cover the same ground |
| **Custom log export**: user-defined text formats built from macros (`%CALL`, `%HIS`, `%MY`, `%FREQ`, `%MODE`, `%POWER`, `%NAME`, `%QTH`, `%REM`, `%QSL`, `%S`, `%R`, `%EOD`, date and time forms) | **Keep** | More flexible than ADIF alone |
| Log backup | **Keep** | |
| Contest support: dupe checking, contest serial numbers in templates and the FSK identifier | **Later** | |
| Country file (`ARRL.DX`) | **Modernise** | |

### Interface and workflow

| Feature | Verdict | Note |
| --- | --- | --- |
| Custom sounds on events | **Keep** | Becomes notifications and audible cues |
| Template extras: overlay, perspective and 3-D text, undo, a library of custom items (QSL boxes, text art) | **Modernise** | Overlay and undo yes; 3-D text no |
| Image tools: clipper with pop-up menus, filters, bit mask, 320×240 handling schemes | **Modernise** | Crop and basic adjustment only |
| Thumbnail browsing with multiple pages, and a file preview tool | **Keep** | |
| Two window layouts, detachable control-button window, desktop-attached RX window | **Drop** | Platform-native window management instead |
| OLE embedding from other Windows applications | **Drop** | |
| A command-line option to shift all tones down by 1 kHz | **Later** | For transverter and intermediate-frequency use |

## What the original does not have

These are gaps to fill, not features to port:

- **No remote operation.** Everything requires the GUI on the same machine. A
  Raspberry Pi in the shack, controlled from a laptop or phone, was not a
  thought in 2000. Note the correction, though: MMSSTV *can* run unattended,
  through its repeater mode.
- **No programmable interface.** Macros, repeater automation and CAT command
  strings are configuration, not an API: no other program can ask "what did
  you just receive?" or be told when a picture completes.
- **No modern rig control.** Hand-written CAT strings per radio, where hamlib
  covers hundreds of radios today.
- **No touch or high-DPI support**, no accessibility, no dark mode.
- **No image metadata.** Received images carry no record of frequency, mode,
  signal report or the station that sent them beyond the log's QSL index.
- **Limited formats.** BMP and JPEG only, no alpha, no EXIF handling.
- **No tests and no packaging** beyond a Windows installer.

## What to carry forward

Four ideas from MMSSTV are worth preserving exactly as they are, because
twenty years of operators have built habits on them:

1. **WYSIWYT.** The TX pane shows precisely what will be sent, template and
   all.
2. **The mode is a decision, not a dialog.** Mode buttons are one click away,
   always visible.
3. **Templates are data-bound.** A template with `%c` in it is worth more than
   a pretty static overlay.
4. **The waterfall is the tuning instrument.** Markers at the four
   significant frequencies, always on screen while receiving.
