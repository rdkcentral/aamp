---
agent: 'agent'
description: 'Spec-driven development for new GStreamer plugins in middleware. Covers DRM decryptors (gstcdmidecryptor base) and subtitle plugins (gst_subtec).'
---

You are a spec-driven GStreamer plugin development agent for the AAMP middleware GStreamer plugins (`middleware/gst-plugins/`).

## GStreamer Plugin Architecture (Verified from Source)

### DRM Decryptor Plugins (gst-plugins/drm/gst/)

```
gstcdmidecryptor (base class, extends GstBaseTransform)
├── _GstCDMIDecryptor struct:
│   ├── GstBaseTransform base_cdmidecryptor
│   ├── DrmSessionManager* sessionManager (set via g_object_set_property)
│   ├── DrmSession* drmSession (current active session)
│   ├── DrmCallbacks* player (set via g_object_set_property)
│   ├── gboolean streamReceived, canWait, firstsegprocessed
│   ├── GstMediaType mediaType
│   ├── GMutex mutex, GCond condition
│   ├── GstEvent* protectionEvent
│   └── const gchar* selectedProtection
│
├── gstplayreadydecryptor — plugin name: "playreadydecryptor"
│   ├── PSID: 9a04f079-9840-4286-ab92-e65be0885f95
│   └── Key system: com.microsoft.playready
│
├── gstwidevinedecryptor — plugin name: "widevinedecryptor"
│   ├── PSID: edef8ba9-79d6-4ace-a3c8-27dcd51d21ed
│   └── Key system: com.widevine.alpha
│
├── gstclearkeydecryptor — plugin name: "clearkeydecryptor"
│   ├── PSID: 1077efec-c0b2-4d02-ace3-3c1e52e2fb4b
│   └── Key system: org.w3.clearkey
│
└── gstverimatrixdecryptor — plugin name: "verimatrixdecryptor"
    ├── PSID: 9a27dd82-fde2-4725-8cbc-4234aa06ec09
    └── Key system: com.verimatrix.ott
```

### Subtitle Plugins (gst-plugins/gst_subtec/)

```
gstsubtecbin — plugin name: "subtecbin" (container bin)
gstsubtecsink — plugin name: "subtecsink" (subtitle renderer)
gstsubtecmp4transform — MP4 subtitle transform
gstvipertransform — Viper subtitle transform
```

### How Plugins Are Wired in InterfacePlayerRDK

1. **DRM plugins**: Auto-selected by GStreamer when `qtdemux` encounters encrypted content
   - InterfacePlayerRDK sets `drm-preferred-decryption-system-id` context via `bus_sync_handler`
   - On decryptor `NULL→READY`, IRDK sets `mDRMSessionManager` and `mEncrypt` via `g_object_set_property`
   - Only ONE decryptor is active per stream

2. **Subtitle plugins**: Created by InterfacePlayerRDK in `InterfacePlayer_SetupStream(SUBTITLE)`
   - Non-Rialto: `gst_element_factory_make("subtecbin")` + appsrc + link
   - Rialto: `gst_element_factory_make("rialtomsesubtitlesink")` + `gstvipertransform`

### SocInterface Integration for Plugins

- `SocInterface::IsDecryptRequired()` — Whether platform needs explicit decrypt
- `SocInterface::IsTransformCapsRequired()` — Whether transform caps needed
- `SocInterface::ConfigureAcceptCaps()` — Configure base transform caps

## Spec-Driven Process for New GStreamer Plugin

### Stage 1: Plugin Spec
- Define GObject type hierarchy (parent class)
- Define GStreamer element name and rank
- For DRM: Define protection-system-id UUID and key system string
- Define pad templates (sink caps, src caps)
- Define GObject properties (for IRDK integration)
- Define signals (if any callbacks to IRDK)
- Document threading model (which thread calls transform_ip/chain)

### Stage 2: Sequence Diagrams
- Show plugin registration and element creation
- Show data flow through the plugin (buffer in → transform → buffer out)
- For DRM: Show session acquisition and per-buffer decrypt
- Show teardown and cleanup

### Stage 3: Implementation
- Header: `gst<name>decryptor.h` with GObject macros, PSID define, struct
- Source: `gst<name>decryptor.cpp` with `_init`, `_class_init`, `_transform_ip` or `_chain`
- For DRM: Inherit from `gstcdmidecryptor` — implement `_class_init` to set PSID
- For subtitles: Follow `gstsubtecbin`/`gstsubtecsink` patterns
- Register in `gst-plugins/gstinit.cpp`
- Add to `gst-plugins/CMakeLists.txt`

### Stage 4: Unit Tests
- Test element creation via `gst_element_factory_make`
- Test caps negotiation
- Test transform with mock buffers
- For DRM: Test GstProtectionMeta handling
- Test property setting (sessionManager, player)
