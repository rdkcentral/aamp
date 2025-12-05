# MP4 Box Parsing Tests

## Overview

This directory contains comprehensive unit tests for validating the `Mp4Demux::Parse()` API for different MP4 box types.

**Note**: This test suite is now **completely separated** from `AampMp4DemuxTests`. It has its own executable and can be built and run independently.

## Test Structure

The tests are organized to validate parsing of various MP4 boxes including:

### Container Boxes
- **ftyp** (File Type) - Identifies file specifications ✅
- **moof** (Movie Fragment) - Contains movie fragments ✅
- **traf** (Track Fragment) - Contains track fragment information

### Track Fragment Boxes  
- **tfhd** (Track Fragment Header) - Sets default values for track fragment ✅
  - Triggers `ParseTrackFragmentHeader()` in the demuxer
- **trun** (Track Run) - Contains sample information ✅
  - Triggers `ParseTrackRun()` when tfhd is present

### Encryption/DRM Boxes
- **pssh** (Protection System Specific Header) - DRM-specific data ✅
- **saiz** (Sample Auxiliary Information Sizes) - Size info for auxiliary data ⚠️
- **senc** (Sample Encryption) - Per-sample IVs and subsample encryption info ⚠️
- **tenc** (Track Encryption) - Default encryption parameters for track ⚠️

### Error Handling Tests
- Malformed box sizes
- Empty buffers
- Incomplete headers

## Test Validation Approach

The tests use several approaches to validate correct parsing:

1. **No-Throw Validation**: Many tests simply verify that Parse() doesn't crash or throw exceptions
2. **State Validation**: Some tests query demuxer state (e.g., `GetProtectionEvents()` for pssh)
3. **Structure Validation**: Tests build complete valid MP4 structures with proper nesting

## Building and Running

### Build
```bash
cd /home/lash/aamp_new_1/aamp/build
make Mp4BoxParsingTests -j$(nproc)
```

### Run
```bash
cd /home/lash/aamp_new_1/aamp/build/test/utests/tests/Mp4BoxParsingTests
./Mp4BoxParsingTests
```

### Run Specific Test
```bash
./Mp4BoxParsingTests --gtest_filter=Mp4BoxParsingTestFixture.ParseTfhdBox
```

## Implementation Notes

### Box Structure
All MP4 boxes follow this structure:
```
[4 bytes: size][4 bytes: type][payload]
```

Full boxes (with version/flags) add:
```
[1 byte: version][3 bytes: flags]
```

### Helper Functions

- `CreateBoxHeader(size, type)` - Creates basic box header
- `CreateFullBoxHeader(size, type, version, flags)` - Creates full box header with version/flags

### Context Requirements

Some boxes require context (parent boxes) to be parsed correctly:
- `tfhd` requires `moov` and `moof` context
- `trun` requires `tfhd` to be parsed first
- `senc` requires `traf` context
- `saiz` requires `traf` context

## Known Issues

⚠️ **Assertion Failures**: Some encryption-related box tests (`saiz`, `senc`, `tenc`) trigger assertions in MP4Demux.cpp due to strict parsing requirements. These boxes require very precise context and structure to parse successfully.

## Future Enhancements

1. Add mock/fake for MP4Demux internal state to validate parsing results
2. Add tests for:
   - `stsd` (Sample Description)
   - `mvhd` (Movie Header)
   - `mdhd` (Media Header)
   - `tkhd` (Track Header)
   - `sidx` (Segment Index)
3. Add integration tests that combine multiple boxes
4. Add performance tests for large buffers

## References

- ISO/IEC 14496-12 (ISO Base Media File Format)
- ISO/IEC 23001-7 (Common Encryption)
- MP4Demux implementation: `aamp/mp4demux/MP4Demux.cpp`

## Test Status Legend

- ✅ Test passes
- ⚠️ Test has known issues (assertion failures)
- ❌ Test fails
- 🚧 Test not yet implemented
