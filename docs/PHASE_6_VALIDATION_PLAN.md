# Phase 6: External Validation & Decoder Testing Plan

**Date Started**: January 30, 2026  
**Objective**: Validate all 43 generated WAV files with external SSTV decoders  
**Target Outcome**: Production-ready encoder ready for public release  
**Est. Duration**: 1-2 weeks (depending on decoder availability)

---

## 🎯 Primary Objective

Confirm that the SSTV encoder library generates valid audio that can be correctly decoded by standard SSTV receiver software (MMSSTV, QSSTV, etc.).

### Success Metrics
- ✅ All 43 modes decode in MMSSTV without errors
- ✅ Color bar patterns correctly reproduced in decoded images
- ✅ Cross-validation with QSSTV successful
- ✅ No audible artifacts or dropouts
- ✅ Timing accuracy verified (±1% tolerance)
- ✅ Color accuracy within standard SSTV range

---

## 📋 Task Breakdown

### Task 1: Environment Setup
**Status**: 🔄 NOT STARTED  
**Estimated Time**: 2-4 hours

#### 1.1: MMSSTV Setup
- [ ] Option A (Windows): Install MMSSTV directly
- [ ] Option B (macOS): Set up Windows VM or Wine
- [ ] Option C (Cross-platform): Use online SSTV decoder if available
- [ ] Verify MMSSTV is functioning with known-good SSTV files
- [ ] Document MMSSTV version and settings used

#### 1.2: QSSTV Setup (Linux/macOS)
- [ ] Install QSSTV on available Linux/macOS system
- [ ] Verify it can decode standard SSTV WAV files
- [ ] Document QSSTV version and settings used
- [ ] Set up audio routing (if needed)

#### 1.3: Test File Preparation
- [ ] Organize 43 WAV files by mode family:
  - Robot family (5 modes)
  - Scottie family (3 modes)
  - Martin family (5 modes)
  - PD family (7 modes)
  - Narrow modes (11 modes)
  - Others (12 modes)
- [ ] Create manifest with expected properties per mode
- [ ] Document any known decoder limitations

---

### Task 2: MMSSTV Validation

**Status**: 🔄 NOT STARTED  
**Estimated Time**: 4-6 hours  
**Note**: Sequential testing to identify issues early

#### 2.1: Basic Mode Testing (5 Modes)
Test core modes first to catch fundamental issues:

- [ ] **Robot 36** (baseline SSTV mode)
  - [ ] Import WAV file
  - [ ] Verify image dimensions (120×96)
  - [ ] Verify color bar pattern visible
  - [ ] Check for sync issues
  - [ ] Document results

- [ ] **Scottie 1** (common mode)
  - [ ] Import WAV file
  - [ ] Verify image dimensions (320×256)
  - [ ] Verify color bar pattern visible
  - [ ] Check timing accuracy
  - [ ] Document results

- [ ] **Martin 1** (RGB sequential)
  - [ ] Import WAV file
  - [ ] Verify color rendition
  - [ ] Check for color shift
  - [ ] Document results

- [ ] **PD120** (different color encoding)
  - [ ] Import WAV file
  - [ ] Verify YC color space handling
  - [ ] Check for color accuracy
  - [ ] Document results

- [ ] **MN140** (narrow mode - edge case)
  - [ ] Import WAV file
  - [ ] Verify timing accuracy
  - [ ] Check for decoder-specific issues
  - [ ] Document results

#### 2.2: Comprehensive Mode Testing (All 43 Modes)
Once basic testing succeeds:

- [ ] Test all remaining 38 modes in batches
- [ ] Document any failures or anomalies
- [ ] Categorize by error type (if any):
  - Timing issues
  - Color accuracy problems
  - Decoder compatibility
  - Library bugs
- [ ] Create detailed test report

#### 2.3: Color Accuracy Analysis

For each mode:
- [ ] Compare decoded colors to original test pattern
- [ ] Measure color accuracy (if tools available)
- [ ] Document color space handling
- [ ] Note any systematic color shifts
- [ ] Identify modes with known limitations

---

### Task 3: QSSTV Cross-Platform Validation

**Status**: 🔄 NOT STARTED  
**Estimated Time**: 2-3 hours

#### 3.1: QSSTV Testing
- [ ] Select 5-10 representative modes
- [ ] Test in QSSTV decoder
- [ ] Compare results with MMSSTV decoding
- [ ] Document any differences
- [ ] Verify cross-platform compatibility

#### 3.2: Results Comparison
- [ ] Create comparison matrix (MMSSTV vs QSSTV)
- [ ] Identify decoder-specific behaviors
- [ ] Document any mode-specific issues
- [ ] Validate timing consistency

---

### Task 4: Advanced Testing

**Status**: 🔄 NOT STARTED  
**Estimated Time**: 3-4 hours

#### 4.1: Real Image Encoding
- [ ] Create `encode_image.c` example with stb_image integration
- [ ] Implement image loading for BMP/JPEG/PNG
- [ ] Add image scaling to mode resolution
- [ ] Generate test WAV with standard test image (Lenna)
- [ ] Verify decoding accuracy with real image data
- [ ] Document color accuracy metrics

#### 4.2: Stress Testing
- [ ] Test extreme color values (pure red, pure blue, etc.)
- [ ] Test grayscale (if supported by mode)
- [ ] Test fast transitions between colors
- [ ] Test full-color image edge cases
- [ ] Document any stability issues

#### 4.3: Performance Profiling
- [ ] Measure encoding time for all 43 modes
- [ ] Calculate speedup vs realtime (target: > 10x)
- [ ] Measure memory usage per mode
- [ ] Profile on Raspberry Pi (if available)
- [ ] Document performance metrics

---

### Task 5: Documentation & Release

**Status**: 🔄 NOT STARTED  
**Estimated Time**: 2-3 hours

#### 5.1: Test Results Report
- [ ] Compile all test results
- [ ] Create summary table (mode name, VIS code, result)
- [ ] Document any failures and root causes
- [ ] Include performance metrics
- [ ] Create README for test suite usage

#### 5.2: GitHub Repository Setup
- [ ] Create GitHub repository
- [ ] Upload source code
- [ ] Add comprehensive README
- [ ] Add build instructions
- [ ] Add usage examples
- [ ] Add test results and validation data

#### 5.3: Distribution Packages
- [ ] Create CMake package targets
- [ ] Build for multiple platforms:
  - [ ] macOS (Universal)
  - [ ] Linux (x86_64, ARM)
  - [ ] Raspberry Pi (ARMv7/ARMv8)
- [ ] Create documentation
- [ ] Prepare release notes

---

## 📊 Testing Matrix

### Modes to Test (Priority Order)

**Tier 1 - Core Modes (Must Work)**
- [ ] Robot 36 (0x88) - Baseline B/W mode
- [ ] Scottie 1 (0x3C) - Common color mode
- [ ] Martin 1 (0x44) - RGB sequential
- [ ] PD120 (0x57) - YC sequential
- [ ] MN110 (0x98) - Narrow mode

**Tier 2 - Common Variants (Should Work)**
- [ ] Robot 72 (0x89)
- [ ] Scottie 2 (0x3E)
- [ ] Martin 2 (0x45)
- [ ] PD180 (0x5F)
- [ ] SC2-120 (0xE0)

**Tier 3 - All Remaining (Nice to Have)**
- [ ] All other 33 modes

---

## 🔍 Known Issues to Watch For

### Potential Problem Areas
1. **Narrow mode timing** - Test if decoders handle 400ms preamble correctly
2. **VIS duration** - Verify 940ms VIS header decodes properly
3. **Color accuracy** - YC vs RGB encoding differences
4. **Parity handling** - Some modes use odd parity (unusual for SSTV)
5. **No-VIS modes** - Test modes without VIS header
6. **Sample rate variations** - Test with multiple sample rates (48000, 44100, etc.)

### Decoder Limitations
- MMSSTV may have issues with some narrow modes
- QSSTV may not support all exotic SSTV variants
- Some decoders may require specific audio levels
- Timing tolerance varies by decoder

---

## 📋 Validation Checklist

### Before Starting Tests
- [ ] All 43 WAV files generated successfully
- [ ] WAV file format verified (16-bit PCM, 48 kHz)
- [ ] MMSSTV installed and tested with known file
- [ ] QSSTV installed (if testing cross-platform)
- [ ] Test manifest prepared

### During Testing
- [ ] Each mode tested independently
- [ ] Results documented per mode
- [ ] Any anomalies investigated
- [ ] Screenshots/logs saved for failed tests

### After Testing
- [ ] Summary report generated
- [ ] All failures root-caused or documented
- [ ] GitHub repository created
- [ ] Release notes prepared
- [ ] Distribution packages built

---

## 📝 Test Report Template

For each mode tested:

```
## Mode: [Mode Name]
- VIS Code: 0x[HEX]
- Test File: [Filename]
- Test Date: [Date]
- Decoder: [MMSSTV/QSSTV version]

### Results
- **Decoding**: PASS / FAIL / PARTIAL
- **Color Accuracy**: [Assessment]
- **Timing**: OK / SLOW / FAST by [±X%]
- **Artifacts**: [None / Description]
- **Notes**: [Any observations]

### Issues (if any)
- [Issue 1]
- [Issue 2]

### Screenshots/Evidence
- [Decoded image]
- [Comparison with original]
```

---

## 🎉 Success Criteria - Phase 6

### Minimum Criteria (Go/No-Go)
- ✅ At least 40/43 modes decode in MMSSTV
- ✅ No crashes in decoder
- ✅ Color bar pattern visible in decoded output
- ✅ Timing within ±5% of specification

### Target Criteria
- ✅ All 43 modes decode successfully
- ✅ QSSTV cross-validation successful
- ✅ Color accuracy within ±5 units (8-bit scale)
- ✅ Real image encoding working
- ✅ Performance > 10x realtime on modern hardware

### Release Criteria
- ✅ All target criteria met
- ✅ Comprehensive documentation complete
- ✅ GitHub repository public and complete
- ✅ Distribution packages available
- ✅ Known limitations documented

---

## 📅 Timeline Estimate

| Phase | Task | Est. Hours | Status |
|-------|------|-----------|--------|
| 1 | Environment Setup | 2-4 | 🔄 |
| 2 | MMSSTV Testing | 4-6 | 🔄 |
| 3 | QSSTV Testing | 2-3 | 🔄 |
| 4 | Advanced Testing | 3-4 | 🔄 |
| 5 | Documentation | 2-3 | 🔄 |
| **Total** | | **13-20 hours** | 🔄 |

**Realistic Completion**: 1-2 weeks (depending on decoder access and issue frequency)

---

## 🚀 Next Steps

1. **Environment Assessment**
   - Determine MMSSTV availability (Windows VM? Wine? Online tools?)
   - Check QSSTV availability for cross-platform testing
   - Identify any external dependencies

2. **Initial Testing**
   - Test Robot 36 (most basic mode)
   - If successful, proceed to Tier 1 modes
   - If issues found, debug and document

3. **Rapid Iteration**
   - Create test script/checklist for efficiency
   - Batch test multiple modes
   - Document patterns in any failures

4. **Release Preparation**
   - Compile final test results
   - Create GitHub repository
   - Prepare distribution packages

---

**Status**: ✅ Ready to Begin Phase 6  
**Prerequisites Met**: ✅ YES (All 43 modes generated, encoder complete)  
**Target Completion**: Mid-February 2026  

