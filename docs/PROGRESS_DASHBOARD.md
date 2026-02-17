# Master Plan Progress Dashboard

**mmsstv-portable** — Cross-Platform SSTV Library  
**Date:** February 5, 2026  
**Overall Progress:** 35–40% Complete

---

## 📊 Progress by Phase

```
Phase 1: Foundation & Infrastructure
████████████████████████████████████ 100% ✅ COMPLETE

Phase 2: DSP Components  
██████████████████████░░░░░░░░░░░░░░ 60% ⏳ IN PROGRESS
  └─ Core: ✅ (CVCO, CFIR2, CIIRTANK, CIIR, DoFIR)
  └─ Advanced: ⏳ (CPLL, CFQC pending; CLMS, CFFT deferred)

Phase 3: SSTV Core Logic
████████████████░░░░░░░░░░░░░░░░░░░░ 40% ⏳ IN PROGRESS (CRITICAL)
  └─ Infrastructure: ✅ (demod pipeline, sync machine, VIS bits)
  ✋ BLOCKED: VIS→mode mapping, image buffer, line timing

Phase 4: Image Handling
████████░░░░░░░░░░░░░░░░░░░░░░░░░░░ 20% ⏳ PENDING
  └─ Deferred until Phase 3 complete

Phase 5: API Implementation
██████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 15% ⏳ PENDING
  └─ Basic scaffolding done; callbacks, features pending

Phase 6: Testing & Validation
██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 5% ⏳ PENDING
  └─ Deferred until phases 3–5 complete
```

---

## 🎯 Current Status

### ✅ Working & Tested
- ✅ Build system (CMake)
- ✅ All 43 SSTV mode definitions
- ✅ SSTV encoder (full functionality)
- ✅ FM tone generators (CVCO)
- ✅ DSP filters (FIR, IIR, CIIRTANK)
- ✅ Filter designers (Kaiser, Butterworth, Chebyshev)
- ✅ Audio sample processing pipeline
- ✅ Sync pulse detection (basic)
- ✅ VIS bit accumulation (logic only)

### ⏳ Partially Done
- ⏳ Decoder API (scaffolding exists, features missing)
- ⏳ Image buffer (struct defined, not implemented)
- ⏳ Line timing (pulse detection works, pixel clock missing)
- ⏳ Color support (architecture planned, not coded)
- ⏳ Test harness (7 basic tests, need functional tests)

### ❌ Not Started
- ❌ VIS→mode mapping (CRITICAL BLOCKER #1)
- ❌ Image buffer allocation (CRITICAL BLOCKER #2)
- ❌ Line-by-line pixel accumulation (CRITICAL BLOCKER #3)
- ❌ CPLL (Phase-locked loop) for AFC
- ❌ Slant correction
- ❌ Color mode decoders
- ❌ JPEG integration
- ❌ End-to-end integration tests
- ❌ Real-world signal testing

---

## 🚨 Critical Blockers

### Blocker #1: VIS Code Mapping
**What:** Bits accumulate, but don't convert to SSTV mode  
**Fix:** Create static lookup table (VIS byte → mode enum)  
**Effort:** 2 hours  
**Impact:** HIGH — blocks auto-mode detection  

### Blocker #2: Image Buffer Management
**What:** Buffer struct defined, no pixel allocation/storage  
**Fix:** Allocate RGB24 buffer per mode, implement line storage  
**Effort:** 2 hours  
**Impact:** HIGH — blocks image output  

### Blocker #3: Line Synchronization
**What:** Sync pulses detected, but no pixel clock/timing  
**Fix:** Count samples between syncs, fill pixels at correct rate  
**Effort:** 2 hours  
**Impact:** HIGH — blocks multi-line images  

**Total to unblock:** ~6 hours (achievable this week)

---

## 📈 What's Next

### This Week: Complete Phase 3 Core
```
VIS Mapping      (2h) → Enables mode detection
    ↓
Image Buffer     (2h) → Enables pixel storage
    ↓
Line Timing      (2h) → Enables full image
    ↓
Grayscale Test   (1h) → Validate end-to-end
    ↓
✅ First working decoder!
```

### Next Week: Phase 3 Extensions + Phase 4
- Add color mode support (3 hours)
- Implement JPEG integration (2 hours)
- Create WAV file decoder utility (2 hours)
- Port CPLL for AFC (4 hours)

### Following Week: Phase 5–6
- Complete API (callbacks, error handling)
- Comprehensive testing (all 43 modes)
- Real-world signal validation
- Documentation & examples

---

## 📋 Decision Checklist

**Architecture Decisions Made:**
- ✅ Keep decoder audio-source agnostic (user provides samples)
- ✅ Use float format (-1.0 to +1.0) for samples
- ✅ Support arbitrary sample rates
- ✅ Mono input (no stereo conversion)
- ✅ Buffer entire image (avoid progressive callbacks for now)
- ✅ Work in RGB internally (convert from mode-native as needed)
- ✅ Use sync-lock for line timing (resync each line)

**Pending Decisions:**
- [ ] JPEG vs. PPM for image export (recommend: start with PPM, add JPEG later)
- [ ] Callback strategy (recommend: add in Phase 5)
- [ ] Real-time vs. offline emphasis (recommend: support both via flexibility)
- [ ] Multi-threading model (recommend: single-threaded for now, add async in Phase C)

---

## 💡 Opportunities to Capture

### High Priority (Do This Week)
1. **VIS Code Mapping** — Unblock mode detection (2h)
2. **Synthetic SSTV Test** — Validate end-to-end (2h)

### Medium Priority (Do Next Week)
1. **WAV Decoder Tool** — Enables offline testing (2h)
2. **CPLL Port** — Enables off-frequency reception (4h)
3. **First Color Mode** — Martin 1 decoder (2h)

### Low Priority (Phase 4+)
1. **Frequency Response Analyzer** — Diagnostic tool (2h)
2. **Slant Correction** — Clock drift compensation (3h)
3. **Real-Time RX Tool** — PortAudio integration (4h)

---

## 🔍 Key Metrics

| Metric | Current | Target | Status |
|--------|---------|--------|--------|
| Modes supported | 0 (decoder not working) | 43 | ⏳ |
| Image formats | Structure only | RGB24, GRAY8, JPEG | ⏳ |
| DSP tests passing | 14/17 (82%) | 17/17 (100%) | ⏳ |
| Functional tests | 7 (lifecycle only) | 50+ (end-to-end) | ⏳ |
| Real-world signals tested | 0 | 20+ | ⏳ |
| Sample rate support | 48 kHz (tested) | Any | ✅ |
| Code coverage (est.) | 40% | 85% | ⏳ |

---

## 📝 Next Actions (Today)

### Immediate (Start Now)
1. [ ] Read VIS code reference: [tests/vis_codes.json](../tests/vis_codes.json)
2. [ ] Extract VIS→mode mapping from reference
3. [ ] Implement `decoder_check_vis_ready()` function
4. [ ] Create unit test for VIS code recognition

### Short Term (This Week)
1. [ ] Implement image buffer allocation (per mode)
2. [ ] Implement line synchronization logic
3. [ ] Create synthetic SSTV test transmission
4. [ ] Decode Robot 36 grayscale image

### Medium Term (Next Week)
1. [ ] Add color mode support
2. [ ] Port CPLL for frequency tracking
3. [ ] Create WAV file decoder utility
4. [ ] Test with captured real signals

---

## 📞 Points of Contact

**Reference Code (MMSSTV):**
- FM demod: `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.cpp` lines 1819–2000
- VIS decode: `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.cpp` lines 1900–2200
- Line sync: `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.cpp` lines 2324–2378
- Mode info: `/Users/ssamjung/Desktop/WIP/mmsstv/sstv.h` lines 1–300

**Current Implementation:**
- Decoder: [src/decoder.cpp](../src/decoder.cpp)
- API: [include/sstv_decoder.h](../include/sstv_decoder.h)
- Tests: [tests/test_decoder_basic.c](../tests/test_decoder_basic.c)

**Documentation:**
- Full analysis: [docs/PORTING_ANALYSIS.md](../docs/PORTING_ANALYSIS.md)
- Detailed plan: [docs/MASTER_PLAN_REVIEW.md](../docs/MASTER_PLAN_REVIEW.md) ← YOU ARE HERE
- RX progress: [docs/RX_DECODER_PROGRESS.md](../docs/RX_DECODER_PROGRESS.md)
- DSP guide: [docs/DSP_CONSOLIDATED_GUIDE.md](../docs/DSP_CONSOLIDATED_GUIDE.md)

---

**Status:** Ready to refocus on Phase 3 completion  
**ETA to first working decoder:** 1 week  
**ETA to full feature parity:** 3 weeks  

*Review completed: February 5, 2026*
