# Master Plan Review & Refocus
## mmsstv-portable: Porting Analysis vs. Current Progress

**Date:** February 5, 2026  
**Status:** Mid-Phase 2, transitioning to Phase 3

---

## 1. EXECUTIVE SUMMARY

### Overall Progress: 35–40% Complete

| Phase | Target | Actual | Status |
|-------|--------|--------|--------|
| **Phase 1** (Foundation & Infrastructure) | Weeks 1-2 | ✅ Complete | 100% |
| **Phase 2** (DSP Components) | Weeks 3-4 | ⏳ 60% | In Progress |
| **Phase 3** (SSTV Core Logic) | Weeks 5-6 | ⏳ 40% | Just Started |
| **Phase 4** (Image Handling) | Week 7 | ⏳ 20% | Pending |
| **Phase 5** (API Implementation) | Week 8 | ⏳ 15% | Pending |
| **Phase 6** (Testing & Validation) | Weeks 9-10 | ⏳ 5% | Pending |

---

## 2. CURRENT STATE BY PHASE

### ✅ PHASE 1: Foundation & Infrastructure — COMPLETE

**Completed:**
- CMake build system fully operational
- Directory structure organized (include/, src/, tests/)
- Mode enumerations extracted (43 SSTV modes)
- Mode information tables created
- VIS code mappings established
- Testing framework set up (CTest + custom tests)
- LGPL v3 licensing in place

**Not Done (Deferred):**
- [ ] CI/CD pipeline (GitHub Actions) — nice-to-have, not blocking

**Verdict:** ✅ **On Track**

---

### ⏳ PHASE 2: DSP Components — 60% COMPLETE

#### ✅ Completed:
1. **CVCO** (VCO oscillator)
   - Fully ported, tested, working
   - Sine table generation verified
   - Frequency generation accurate

2. **CFIR2** (FIR filters)
   - Kaiser-windowed FIR implemented
   - LPF, HPF, BPF, BEF all working
   - Stress tests: >500× frequency selectivity ✅

3. **MakeFilter** / **MakeHilbert** (Filter designers)
   - Kaiser window designer working
   - Hilbert transformer taps generated
   - Butterworth/Chebyshev coefficient generation ✅

4. **CIIRTANK** (IIR resonator)
   - 2nd-order resonator implemented
   - Coefficient calculation verified
   - Used successfully in tone detection

5. **CIIR** (IIR filters)
   - Bilinear transform working
   - Butterworth cascaded stages validated
   - Stability verified (noise bounded)

6. **DoFIR** (Single-tap FIR)
   - Circular buffer implementation working
   - Impulse response correct
   - Moving average validated

#### ⏳ In Progress / Partial:
7. **CPLL** (Phase-locked loop)
   - Not yet ported
   - Needed for: frequency tracking, AFC
   - Priority: MEDIUM (Phase 3)

8. **CFQC** (Frequency counter)
   - Not yet ported
   - Needed for: tone detection refinement
   - Priority: MEDIUM (Phase 3)

9. **CLMS** (LMS adaptive filter)
   - Not yet ported
   - Needed for: notch filtering, interference rejection
   - Priority: LOW (Phase C robustness)

10. **CFFT** (FFT)
    - Not yet ported
    - Needed for: diagnostics, spectrum analysis
    - Priority: LOW (Phase D optional)

#### Test Results:
- 17 DSP tests total
- 14 passing (82.4%)
- 3 failing (tolerance issues, not functional problems)
- Stress tests all passing ✅

**Verdict:** ✅ **On Track** — Core filters working, advanced features can be added incrementally

---

### ⏳ PHASE 3: SSTV Core Logic — 40% COMPLETE

#### ✅ Completed:
1. **Decoder Infrastructure**
   - Decoder struct with full state machine
   - BPF + tone detector pipeline
   - AGC tracking
   - Sync state machine (5 states)
   - VIS bit accumulation logic
   - Image buffer stubs

2. **Sample Processing Pipeline**
   - Audio input → HPF → BPF → CIIRTANK
   - Mark/space/sync tone detection
   - Envelope extraction
   - Bit decision (mark > space = 1, else 0)
   - Sync pulse tracking

3. **Test Harness**
   - Basic lifecycle tests (all passing)
   - Sample feeding with synthetic tones
   - State management verified

#### ⏳ In Progress / Partial:
4. **VIS Code Mapping**
   - Bit accumulation working ✅
   - VIS→mode lookup: **NOT STARTED**
   - Need: 43 mode → VIS code mapping
   - Need: Auto-detection logic
   - Priority: **HIGH** (blocks mode detection)

5. **Image Buffer Management**
   - Buffer struct defined
   - Pixel allocation: **NOT STARTED**
   - Line accumulation: **NOT STARTED**
   - RGB/grayscale support: **NOT STARTED**
   - Priority: **HIGH** (blocks image output)

6. **Sync Detection (from MMSSTV)**
   - State machine: ✅ Basic version working
   - Pulse detection: ✅ Working
   - Line timing: **NOT STARTED**
   - Resync logic: **NOT STARTED**
   - Priority: **MEDIUM**

7. **Color Decoding**
   - RGB sequential (Martin/Scottie): **NOT STARTED**
   - YC format (Robot): **NOT STARTED**
   - RGB parallel (PD modes): **NOT STARTED**
   - Priority: **MEDIUM** (comes after grayscale)

#### Missing (Not Ported Yet):
- **CSYNCINT** (Sync integrator) — Referenced but not fully ported
- **AFC** (Auto Frequency Control) — PLL-based frequency tracking
- **Slant correction** — Clock mismatch compensation
- **FSK decoder** — For callsign ID transmission

**Verdict:** ⚠️ **At Inflection Point** — Need to shift from infrastructure to functional decoding

---

### ⏳ PHASE 4: Image Handling — 20% COMPLETE

#### ✅ Completed:
1. **Image Structure**
   - sstv_image_t defined
   - RGB24, RGBA32, GRAY8 formats specified
   - Stride/alignment documented

2. **JPEG Integration**
   - libjpeg available (embedded in MMSSTV)
   - Wrapper API planned

#### ⏳ Not Started:
- [ ] JPEG load/save implementation
- [ ] Color conversion (RGB ↔ YUV)
- [ ] Gamma correction
- [ ] Color bar test pattern generator
- [ ] Raw PPM/PGM export (simpler alternative to JPEG)

**Verdict:** ⏳ **Deferred** — Depends on Phase 3 completion

---

### ⏳ PHASE 5: API Implementation — 15% COMPLETE

#### ✅ Completed:
1. **Decoder API (partial)**
   - sstv_decoder_create() ✅
   - sstv_decoder_free() ✅
   - sstv_decoder_feed() ✅ (basic)
   - sstv_decoder_reset() ✅
   - sstv_decoder_get_image() ✅ (stub)

2. **Encoder API (mostly done in Phase 1)**
   - sstv_encoder_create() ✅
   - sstv_encoder_free() ✅
   - sstv_encoder_generate() ✅
   - Status/progress APIs ✅

3. **Mode Info API**
   - sstv_get_mode_info() ✅
   - sstv_get_all_modes() ✅

#### ⏳ Not Started:
- [ ] Callback mechanisms (sync, line, complete)
- [ ] AFC/slant correction configuration
- [ ] Error handling and result codes
- [ ] C++ RAII wrappers
- [ ] STL integration helpers

**Verdict:** ⏳ **Partially Ready** — Basic scaffolding done, needs callbacks and features

---

### ⏳ PHASE 6: Testing & Validation — 5% COMPLETE

#### ✅ Completed:
1. **DSP Tests**
   - 17 tests (14 passing, 3 tolerance issues)
   - Impulse response validation
   - Frequency response (stress tests)
   - Stability checks

2. **Encoder Tests**
   - Basic smoke test (test_encode_smoke.c)
   - Mode validation test (test_vis_codes.c)

#### ⏳ Not Started:
- [ ] End-to-end decoder tests (with synthetic SSTV)
- [ ] Integration tests (encoder → decoder round-trip)
- [ ] Real-world signal tests
- [ ] Multi-mode tests (all 43 modes)
- [ ] Robustness tests (noise, slant, clock drift)
- [ ] Performance benchmarks

**Verdict:** ⏳ **Deferred** — Needs Phase 3 & 4 completion first

---

## 3. CRITICAL BLOCKERS & OPPORTUNITIES

### 🚨 Critical Blockers (Must Solve to Progress)

#### 1. **VIS Code Mapping** (Blocks mode detection)
**Current:** Bits accumulating but not converted to mode  
**Need:** VIS → SSTV mode lookup table  
**Effort:** ~2 hours  
**Impact:** HIGH (enables auto-mode selection)  
**Solution:**
```c
static const vis_map_t {
    { 0x00, SSTV_R24 },
    { 0x04, SSTV_R36 },
    { 0x08, SSTV_R72 },
    /* ... all 43 */
};
```

#### 2. **Image Buffer Management** (Blocks pixel output)
**Current:** Buffer struct defined, no allocation/accumulation  
**Need:** Create buffer from mode dimensions, accumulate pixels  
**Effort:** ~3 hours  
**Impact:** HIGH (enables image output)  
**Solution:** Allocate RGB24 buffer on VIS detection, fill line-by-line

#### 3. **Line Synchronization** (Blocks multi-line images)
**Current:** Sync pulse detection working, no line timing  
**Need:** Count samples between syncs, fill pixel line at correct timing  
**Effort:** ~2 hours (extract from MMSSTV CSTVDEM::Do())  
**Impact:** HIGH (enables full image reconstruction)  
**Solution:** Implement pixel clock synchronization to line sync intervals

#### 4. **Color Decoding** (Blocks color modes)
**Current:** Tone detection works, no color demux  
**Need:** Separate R/G/B or Y/Cb/Cr from tone sequence  
**Effort:** ~3 hours (implement for each encoding scheme)  
**Impact:** MEDIUM (RGB modes are 60% of SSTV usage)  
**Solution:** Implement color-specific decoders per mode family

---

### ✨ Opportunities for Parallel Work

#### A. **WAV File Decoder Utility** (Phase 4 enhancement)
**Effort:** ~3 hours  
**Benefit:** Enables offline testing without real radio  
**Implementation:**
- Simple WAV header parser (no external lib needed)
- Stream WAV samples to decoder
- Output decoded image as PPM/PNG

**Priority:** MEDIUM (nice for testing, not blocking)  
**Can start:** Immediately (parallel to VIS mapping)

#### B. **Synthetic SSTV Generator** (Phase 2/3 enhancement)
**Effort:** ~2 hours  
**Benefit:** Unit tests for full demod → image pipeline  
**Implementation:**
- Generate VIS code + pixel data as test tones
- Feed to decoder
- Compare output with known input

**Priority:** HIGH (validates end-to-end)  
**Can start:** After VIS mapping

#### C. **Frequency Response Analyzer** (Phase D diagnostic)
**Effort:** ~2 hours  
**Benefit:** Validate filter behavior with measured spectrum  
**Implementation:**
- Use FFT to measure filter cut at various frequencies
- Compare to expected transfer function
- Create frequency response plots

**Priority:** LOW (nice-to-have for validation)  
**Can start:** Anytime (uses existing DSP)

#### D. **Real-Time RX CLI Tool** (Phase 4/5 integration)
**Effort:** ~4 hours (includes PortAudio integration)  
**Benefit:** Full end-to-end decoding from microphone  
**Implementation:**
- Use PortAudio for audio input
- Feed samples to decoder
- Save image on completion

**Priority:** MEDIUM (integration showcase)  
**Can start:** After Phase 3 mostly complete

#### E. **CPLL & AFC Implementation** (Phase 3 enhancement)
**Effort:** ~4 hours  
**Benefit:** Robust frequency tracking for off-center signals  
**Implementation:**
- Port CPLL from MMSSTV
- Use in tone detection loop
- Enables reception of off-frequency transmissions

**Priority:** MEDIUM (improves robustness)  
**Can start:** After Phase 3 core complete

---

## 4. RECOMMENDATIONS FOR NEXT FOCUS

### 🎯 Immediate Priority (This Week)

**Goal:** Complete Phase 3 core to reach first end-to-end decode

**Tasks (in order):**

1. **VIS Code Mapping** (2 hours)
   - Read [tests/vis_codes.json](../tests/vis_codes.json)
   - Create static VIS→mode lookup
   - Implement `decoder_check_vis_ready()` fully
   - Test with synthetic VIS sequences

2. **Image Buffer Allocation** (2 hours)
   - Allocate RGB24 buffer when VIS detected
   - Implement line storage struct
   - Add pixel insertion API

3. **Line Sync Timing** (2 hours)
   - Extract line sync from MMSSTV CSSTVDEM::Do() (~line 2324)
   - Implement pixel clock (count samples, divide by pixels per line)
   - Fill image buffer line-by-line

4. **Grayscale Pixel Extraction** (1 hour)
   - Convert mark/space tone energies to pixel level (0–255)
   - Fill image buffer as lines complete

5. **End-to-End Test** (2 hours)
   - Create synthetic Robot 36 transmission
   - Feed to decoder
   - Verify image output

**Total:** ~9 hours → **One day of focused work**

---

### 📋 Secondary Priority (Next Week)

1. **Color Mode Support** (3 hours)
   - Implement RGB sequential decoder (Martin/Scottie)
   - Test with synthetic color transmission

2. **WAV File Reader** (2 hours)
   - Simple utility for offline testing
   - Enable testing with captured signals

3. **Robustness Features** (4 hours)
   - AFC (frequency tracking) — needs CPLL port
   - Slant correction — clock compensation
   - Error recovery — dropped line handling

4. **Test Coverage** (3 hours)
   - Multi-mode tests (all 43 modes)
   - Noise immunity tests
   - Real-world signal testing

---

### 🏗️ Architectural Decisions to Make

#### 1. **Image Accumulation Strategy**
**Options:**
- **Option A:** Buffer entire image (current plan)
  - Pros: Simpler, matches MMSSTV
  - Cons: Memory-intensive for large modes (P7 = 640×496×3 = ~950 KB)
  
- **Option B:** Progressive callback (line-by-line)
  - Pros: Low memory, real-time capable
  - Cons: More complex, changes API
  
**Recommendation:** **Start with Option A**, add Option B callbacks in Phase 5

#### 2. **Color Space Handling**
**Options:**
- **Option A:** Work in RGB internally, convert on output
  - Pros: Simpler, matches original
  - Cons: Color mode demux more complex
  
- **Option B:** Work in mode-native format (YC, RGB parallel, etc.)
  - Pros: More efficient
  - Cons: More decoder variants
  
**Recommendation:** **Option A** (simpler to start, refactor if needed)

#### 3. **Sync vs. Clock Drift**
**Options:**
- **Option A:** Lock to sync pulses (resync each line)
  - Pros: Robust, handles clock drift
  - Cons: Adds complexity
  
- **Option B:** Use PLL for continuous tracking
  - Pros: Smoother, lower jitter
  - Cons: Needs CPLL port
  
**Recommendation:** **Option A for MVP**, add Option B in Phase C

---

## 5. RESOURCE ALLOCATION

### Phase 3 Completion: Estimated Effort

| Task | Effort | Owner | Notes |
|------|--------|-------|-------|
| VIS code mapping | 2h | Primary | Highest priority |
| Image buffer mgmt | 2h | Primary | Unblock output |
| Line sync timing | 2h | Primary | Unblock multi-line |
| Grayscale pixels | 1h | Primary | Quick once timing done |
| End-to-end test | 2h | Primary | Validation |
| Color mode 1 (Martin) | 2h | Secondary | Parallel work |
| Color mode 2 (PD) | 2h | Secondary | Follow color mode 1 |
| **Total** | **~15 hours** | | 2–3 days focused work |

### Parallel Opportunities

| Task | Effort | Owner | Can Start |
|------|--------|-------|-----------|
| WAV reader tool | 2h | Secondary | Now (independent) |
| Synthetic SSTV gen | 2h | Secondary | After VIS mapping |
| CPLL port | 4h | Secondary | After Phase 3 core |
| Freq response analyzer | 2h | Secondary | Now (uses DSP) |

---

## 6. SUCCESS CRITERIA FOR PHASES

### Phase 3 Complete ✅
- [ ] VIS code decoded correctly
- [ ] Mode auto-detection working
- [ ] Image buffer allocated per mode
- [ ] Grayscale Robot 36 decoded end-to-end
- [ ] At least one color mode (Martin) working
- [ ] All 3 critical blockers resolved

### Phase 4 Complete ✅
- [ ] JPEG load/save working
- [ ] All 43 modes supported
- [ ] Color bar test pattern generator
- [ ] Image scaling/alignment
- [ ] PPM export for debugging

### Phase 5 Complete ✅
- [ ] Full C API with callbacks
- [ ] C++ RAII wrappers
- [ ] Error handling complete
- [ ] Documentation complete
- [ ] Example programs (decode_wav, realtime_rx)

### Phase 6 Complete ✅
- [ ] 100+ test cases
- [ ] All 43 modes tested
- [ ] Robustness validated (noise, drift, slant)
- [ ] Performance benchmarked
- [ ] Real-world signal testing

---

## 7. MASTER PLAN MASTER CHECKLIST

### High-Level Milestones

- [x] **Milestone 1: Build System Ready**
  - CMake, project structure, testing framework

- [x] **Milestone 2: Encoder Complete**
  - All 43 modes, VIS transmission, FM modulation

- [x] **Milestone 3: DSP Foundation**
  - All critical filters, tone detection, sync pulses

- [ ] **Milestone 4: Decoder Core** ⬅️ **CURRENT FOCUS**
  - VIS decoding, mode detection, basic image buffer
  - **Target:** This week

- [ ] **Milestone 5: Full Image Reconstruction**
  - All 43 modes, color support, line timing
  - **Target:** Next week

- [ ] **Milestone 6: Robustness & Polish**
  - AFC, slant correction, error recovery, real-world testing
  - **Target:** Following week

- [ ] **Milestone 7: Library Release v1.0**
  - Documentation, examples, CI/CD
  - **Target:** End of month

---

## 8. REFOCUSING RECOMMENDATION

### What's Working Well ✅
1. DSP layer is solid (82% tests passing, stress tests excellent)
2. Encoder is complete and functional
3. Build system is clean and well-organized
4. API structure is clear and extensible

### What Needs Immediate Attention ⚠️
1. **VIS decoding** — Currently bits accumulate but don't map to mode
2. **Image buffer** — Structure defined but not allocated/filled
3. **Line timing** — Sync detection works, but no pixel clock
4. **Color support** — Framework exists, not implemented

### Recommendation
**Shift focus from infrastructure to functional decoding:**

Instead of continuing incremental improvements to DSP/tests, pivot to completing the three critical blockers (VIS mapping, image buffer, line timing) to achieve first end-to-end decode by end of week.

This will:
- ✅ Prove the architecture works
- ✅ Enable testing with real signals
- ✅ Create foundation for color modes
- ✅ Unblock downstream phases

---

## 9. QUICK REFERENCE: File Locations

**Core Implementation:**
- [src/decoder.cpp](../src/decoder.cpp) — Decoder logic (need: VIS mapping, pixel filling)
- [include/sstv_decoder.h](../include/sstv_decoder.h) — Public API
- [src/sstv.cpp](../src/sstv.cpp) — Mode definitions
- [src/dsp_filters.cpp](../src/dsp_filters.cpp) — Tone detectors

**Tests:**
- [tests/test_decoder_basic.c](../tests/test_decoder_basic.c) — Lifecycle tests
- [tests/vis_codes.json](../tests/vis_codes.json) — VIS code reference
- [tests/test_encode_smoke.c](../tests/test_encode_smoke.c) — Encoder validation

**Reference (MMSSTV):**
- `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.cpp` — Original CSSTVDEM::Do() (line ~1819)
- `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.h` — Original class definitions

---

*Last reviewed: February 5, 2026*  
*Next review: After Phase 3 completion (target: February 10, 2026)*
