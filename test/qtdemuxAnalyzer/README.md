# qtdemuxAnalyzer - MP4 Fragment Analysis Tool

A GStreamer-based MP4 fragment analyzer for testing and debugging DASH/HLS fragment processing with protection metadata analysis.

## Overview

qtdemuxAnalyzer creates a GStreamer pipeline to analyze MP4 initialization headers and media fragments, with comprehensive protection metadata probing for encrypted content. It includes a dummy Widevine decryptor plugin to bypass qtdemux protection checks and enable full fragment analysis.

## Features

- **MP4 Fragment Processing**: Handles initialization segments and media fragments
- **Protection Metadata Analysis**: Comprehensive DRM metadata inspection and logging
- **Dummy Widevine Decryptor**: Bypasses qtdemux protection requirements for analysis
- **Synchronous Context Handling**: Proper DRM context negotiation
- **Real-time Probing**: Buffer-level analysis with timing information

## Pipeline Architecture

```
[MP4 Init File] -> [appsrc] -> [qtdemux] -> [probe] -> [fakesink]
[MP4 Fragment]                    |
                                  -> [widevine-decrypt] (if encrypted)
```

## Building

### Prerequisites

**macOS (Homebrew):**
```bash
brew install gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
```

**Ubuntu/Debian:**
```bash
sudo apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
sudo apt-get install gstreamer1.0-plugins-good gstreamer1.0-plugins-bad
```

### Compilation

```bash
make
```

On macOS with Homebrew GStreamer:
```bash
PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig make
```

## Usage

```bash
./qtdemuxAnalyzer <init_file.mp4> <fragment_file.mp4>
```

### Examples

**Basic MP4 Fragment Analysis:**
```bash
./qtdemuxAnalyzer audio_init.mp4 audio_frag.mp4
```

**Video Fragment Analysis:**
```bash
./qtdemuxAnalyzer video_init.mp4 video_frag.mp4
```

**Encrypted Content Analysis:**
```bash
./qtdemuxAnalyzer encrypted_init.mp4 encrypted_frag.mp4
```

## Output Analysis

### Sample Output

```
=== qtdemux MP4 Fragment Analysis ===
Initialization file: audio_init.mp4
Fragment file: audio_frag.mp4
==========================================

Registered dummy Widevine decryptor plugin
Successfully loaded file audio_init.mp4 (1847 bytes)
Pipeline state changed: NULL -> READY
Pipeline state changed: READY -> PAUSED
New pad created: audio_0 with caps: audio/mpeg

=== Protection Metadata Probe ===
Buffer size: 4096 bytes
Buffer PTS: 0:00:02.000000000
Buffer DTS: 0:00:02.000000000
*** PROTECTION METADATA FOUND ***
Protection info: application/x-cenc, cipher-mode=(string)cbc, iv=(buffer)..., kid=(buffer)...
=====================================
```

### Protection Metadata Fields

The analyzer extracts and displays:

- **Protection System ID**: DRM system UUID identification
  - Widevine: `edef8ba9-79d6-4ace-a3c8-27dcd51d21ed`
  - PlayReady: `9a04f079-9840-4286-ab92-e65be0885f95`
  - FairPlay: `94ce86fb-07ff-4f43-adb8-93d2fa968ca2`

- **Initialization Vector (IV)**: 16-byte encryption initialization vector
- **Key ID**: Unique identifier for content encryption key
- **Subsample Information**: Partial encryption patterns (clear/encrypted byte ranges)
- **Cipher Mode**: Encryption mode (CBC, CTR, etc.)

## Program Flow

1. **Initialization**: Creates GStreamer pipeline with all elements
2. **Plugin Registration**: Registers dummy Widevine decryptor with high priority
3. **File Loading**: Reads MP4 init and fragment files into memory buffers
4. **Pipeline Start**: Sets pipeline to PLAYING state
5. **Data Injection**: 
   - Injects initialization segment first
   - Follows with media fragment
6. **Protection Analysis**: 
   - Probes all buffers passing through qtdemux
   - Extracts and displays protection metadata
7. **Context Handling**: Responds to DRM context requests synchronously
8. **Cleanup**: Properly tears down pipeline and resources

## Dummy Widevine Decryptor

The built-in dummy decryptor:
- **Purpose**: Bypasses qtdemux protection checks for analysis
- **Capability**: Accepts `application/x-cenc` caps with Widevine system ID
- **Functionality**: Pass-through with metadata logging
- **Registration**: High priority (`GST_RANK_PRIMARY`) for qtdemux selection

## File Requirements

### Initialization Segment
- **Format**: MP4 container with `moov` box
- **Content**: Track definitions, codec information, encryption metadata
- **Size**: Typically 1-5KB for DASH content

### Media Fragment
- **Format**: MP4 container with `moof` + `mdat` boxes
- **Content**: Actual media samples (audio/video)
- **Encryption**: May contain protection metadata for encrypted samples

## Integration with AAMP

This tool is designed to:
- **Debug MP4 Processing**: Validate fragment parsing in AAMP pipeline
- **Test Protection Flow**: Verify DRM metadata propagation
- **Analyze Encryption**: Understand protection patterns in content
- **Troubleshoot Issues**: Debug qtdemux and decryptor integration

## Troubleshooting

### Common Issues

**"Failed to create pipeline elements"**
- Ensure GStreamer development packages are installed
- Check qtdemux plugin availability: `gst-inspect-1.0 qtdemux`

**"No pads created from qtdemux"**
- Verify MP4 files are valid containers
- Check file format with: `file <filename>.mp4`
- Ensure initialization segment contains track definitions

**"Stream is protected but no decryptor found"**
- This should not occur with the dummy decryptor
- Check plugin registration output in logs

### Debug Tips

1. **Increase Verbosity**: Set `GST_DEBUG=qtdemux:5` for detailed qtdemux logs
2. **Inspect Elements**: Use `gst-inspect-1.0 <element>` to check capabilities
3. **Pipeline Visualization**: Enable `GST_DEBUG_DUMP_DOT_DIR` for pipeline graphs
4. **File Validation**: Use `mp4box -info <file>` to validate MP4 structure

## Development Notes

- **Memory Management**: All buffers are properly wrapped and freed
- **Thread Safety**: Uses GStreamer's built-in threading model
- **Error Handling**: Comprehensive error checking and reporting
- **Extensibility**: Easy to add additional probe points or analysis

## Use Cases

- **DASH Content Analysis**: Analyze fragmented MP4 streams
- **DRM Integration Testing**: Validate protection metadata handling
- **Pipeline Debugging**: Test qtdemux behavior with various content
- **Fragment Validation**: Verify MP4 fragment structure and timing
- **Encryption Analysis**: Study CENC protection patterns