# Next Steps - Phase 6: External Validation

**Current Status**: ✅ Phase 5 Complete  
**Date**: January 30, 2026  
**Ready to Begin**: Phase 6 - Decoder Validation

---

## 🎯 Immediate Action Items

### 1. Review Completion Status ✅
- [x] All 43 SSTV modes fully implemented
- [x] All 43 WAV files generated successfully (570 MB)
- [x] All tests passing (2/2 CTest)
- [x] Documentation complete
- [x] Code quality verified (no crashes, no memory leaks)

**Validation**: See `ENCODING_ONLY_PLAN.md` for complete progress

---

### 2. Prepare for Validation Phase
**Time Required**: 30 minutes to 2 hours

```bash
# Navigate to test output directory
cd /Users/ssamjung/Desktop/WIP/mmsstv-portable/build/test_modes

# Verify all 43 WAV files present
ls -1 *.wav | wc -l    # Should show 43

# List by family for organization
ls -1 Robot*.wav       # 5 Robot modes
ls -1 Scottie*.wav     # 3 Scottie modes
ls -1 Martin*.wav      # 5 Martin modes
ls -1 PD*.wav          # 7 PD modes
ls -1 *M*.wav          # All narrow/specialty modes

# Review documentation
cat README.md          # Test suite overview
cat REPORT.txt         # Detailed VIS analysis (25 KB)
```

---

### 3. Choose Validation Path

#### Option A: MMSSTV on Windows/Wine (Most Authoritative)
**Best For**: Full validation against reference implementation

```bash
# Requirements:
# - Windows with MMSSTV, or
# - Wine on Linux/macOS with MMSSTV
# - Copy WAV files to Windows/Wine environment
# - Import each in MMSSTV decoder
# - Verify color bar pattern decodes correctly
```

**Next**: 
- [ ] Determine MMSSTV availability
- [ ] If available, set up test environment
- [ ] Follow TIER 1 testing checklist

#### Option B: QSSTV on Linux/macOS (Cross-Platform)
**Best For**: Quick cross-platform validation

```bash
# Install QSSTV (if not already available)
brew install qsstv    # macOS
apt-get install qsstv # Linux

# Test with a single mode
qsstv < Robot_36.wav  # Import and decode
```

**Next**:
- [ ] Install QSSTV
- [ ] Test with Robot 36
- [ ] If successful, expand to other modes

#### Option C: Online SSTV Decoder
**Best For**: No local decoder installation needed

- https://www.bluedragon.dyndns.org/sstv/
- Other online SSTV decoders (search)

**Next**:
- [ ] Find available online decoder
- [ ] Upload Robot_36.wav for testing
- [ ] If successful, test other modes

---

### 4. Start with Tier 1 Testing

**5 Core Modes** (Should all decode successfully):

1. **Robot 36** (0x88)
   - Most basic SSTV mode
   - Low resolution (120×96)
   - Good baseline test
   ```bash
   # Expected: B/W image with color bar pattern
   file: build/test_modes/Robot_36.wav
   ```

2. **Scottie 1** (0x3C)
   - Common color mode
   - Higher resolution (320×256)
   - Tests color encoding
   ```bash
   # Expected: Color image with color bars
   file: build/test_modes/Scottie_1.wav
   ```

3. **Martin 1** (0x44)
   - RGB sequential encoding
   - Medium resolution (320×256)
   - Different color handling
   ```bash
   # Expected: RGB color image
   file: build/test_modes/Martin_1.wav
   ```

4. **PD120** (0x57)
   - YC color space
   - Different timing (120ms/line)
   - Tests non-RGB encoding
   ```bash
   # Expected: YC color image
   file: build/test_modes/PD_120.wav
   ```

5. **MN110** (0x98)
   - Narrow mode (edge case)
   - Faster timing (400ms preamble)
   - Tests decoder robustness
   ```bash
   # Expected: Small fast image
   file: build/test_modes/MN_110.wav
   ```

---

### 5. Document Results

For each tested mode, record:
```
Mode: Robot 36
VIS Code: 0x88
Decoder: MMSSTV [version]
Date: [date]

Result: PASS ✅
- Image decoded successfully
- Color bar pattern visible
- No timing issues
- No artifacts

Notes: [Any observations]
```

---

### 6. Escalation Path (If Issues Found)

**If Robot 36 fails to decode:**
1. Check WAV file format: `afinfo Robot_36.wav`
2. Check sample rate (should be 48000 Hz)
3. Check duration (Robot 36: ~36 seconds)
4. Check file size (should be ~5.3 MB)
5. Review VIS analysis: `head -100 REPORT.txt`

**If specific mode fails:**
1. Check VIS code in INDEX.txt
2. Review mode timing in summary.md
3. Check GENERATION_REPORT.md for specs
4. If encoder bug suspected, see encoder.cpp lines [TBD]

**If multiple modes fail:**
1. Verify decoder is working (test with known-good file)
2. Check audio levels (may need attenuation)
3. Check sample rate (try different rates)
4. Review error patterns to identify systematic issue

---

## 📋 Validation Checklist

### Pre-Validation (This Session)
- [ ] Read `PHASE_6_VALIDATION_PLAN.md` (detailed plan)
- [ ] Understand validation objectives
- [ ] Identify available decoder(s)
- [ ] Gather any tools needed

### Phase 6a: Environment Setup
- [ ] MMSSTV / QSSTV / Online decoder ready
- [ ] Test files organized
- [ ] Documentation reviewed

### Phase 6b: Tier 1 Testing  
- [ ] Robot 36 tested
- [ ] Scottie 1 tested
- [ ] Martin 1 tested
- [ ] PD120 tested
- [ ] MN110 tested
- [ ] All Tier 1 results documented

### Phase 6c: Comprehensive Testing
- [ ] All 43 modes tested (if Tier 1 successful)
- [ ] Results compiled
- [ ] Failures categorized

### Phase 6d: Release Preparation
- [ ] GitHub repository created
- [ ] Documentation complete
- [ ] Distribution packages ready

---

## 📚 Documentation to Review

**Before starting validation:**

1. **PHASE_6_VALIDATION_PLAN.md** (this directory)
   - Detailed testing strategy
   - Task breakdown
   - Success criteria
   - ~200 lines, 15-20 min read

2. **ENCODING_ONLY_PLAN.md** 
   - Project completion summary
   - All phases documented
   - ~1200 lines (skim completion section)

3. **build/test_modes/README.md**
   - Test suite overview
   - WAV file listing
   - ~50 lines, 5 min read

4. **build/test_modes/REPORT.txt**
   - VIS header analysis for all 43 modes
   - Verify encoder output quality
   - ~600 lines (reference only)

---

## 🎯 Success Looks Like

### Minimum Success (Go Ahead with Release)
✅ At least 40/43 modes decode  
✅ No decoder crashes  
✅ Color bar pattern visible  
✅ Timing within specification  

### Ideal Success (Highly Polished Release)
✅ All 43/43 modes decode perfectly  
✅ Cross-platform validation (MMSSTV + QSSTV)  
✅ Real image encoding working  
✅ Performance metrics documented  
✅ GitHub repo public and complete  

---

## ⏱️ Time Investment

| Activity | Est. Time | Priority |
|----------|-----------|----------|
| Read Phase 6 plan | 20 min | HIGH |
| Set up decoder | 30 min-2h | MEDIUM |
| Test Tier 1 (5 modes) | 1-2h | HIGH |
| Test all 43 modes | 4-6h | MEDIUM |
| Document results | 1-2h | MEDIUM |
| Release prep | 2-3h | LOW |
| **Total** | **9-17h** | |

---

## 🚀 Ready to Begin?

**Prerequisites Checklist:**
- [x] Encoder fully implemented
- [x] All 43 modes generating audio
- [x] All tests passing
- [x] WAV files ready for validation
- [x] Documentation complete
- [ ] Decoder environment available (your choice)

**Next Action:**
1. Choose validation path (MMSSTV / QSSTV / Online)
2. Read `PHASE_6_VALIDATION_PLAN.md` 
3. Test Robot 36 with your chosen decoder
4. Proceed with Tier 1 testing if successful

**Support Resources:**
- `PHASE_6_VALIDATION_PLAN.md` - Full validation plan
- `build/test_modes/REPORT.txt` - VIS header analysis
- `ENCODING_ONLY_PLAN.md` - Project completion details
- Test suite at: `/Users/ssamjung/Desktop/WIP/mmsstv-portable/build/test_modes/`

---

**Status**: ✅ READY FOR PHASE 6  
**Date**: January 30, 2026  
**All Systems Go**: 🎉 YES

