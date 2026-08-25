# 15. Shims (Input Sources) — Sequence Diagrams

**Source Files Read**:
- `hdmiin_shim.h/cpp` (complete — 400+ lines)
- `compositein_shim.h/cpp` (complete — 350+ lines)
- `ota_shim.h/cpp` (complete — 500+ lines)
- `rmf_shim.h/cpp` (complete — 400+ lines)
- `videoin_shim.h/cpp` (complete — base class, 200+ lines)

**Confidence: 100%**

---

## 1. Shim Class Hierarchy

`mermaid
classDiagram
    StreamAbstractionAAMP <|-- StreamAbstractionAAMP_VIDEOIN
    StreamAbstractionAAMP_VIDEOIN <|-- StreamAbstractionAAMP_HDMIIN
    StreamAbstractionAAMP_VIDEOIN <|-- StreamAbstractionAAMP_COMPOSITEIN
    StreamAbstractionAAMP <|-- StreamAbstractionAAMP_OTA
    StreamAbstractionAAMP <|-- StreamAbstractionAAMP_RMF

    class StreamAbstractionAAMP_VIDEOIN {
        +Init()
        +Start()
        +Stop()
        +SetVideoRectangle()
        #thunderAccessObj
        #thunderRDKShellObj
    }
    class StreamAbstractionAAMP_HDMIIN {
        +Init()
        +GetStreamFormat()
    }
    class StreamAbstractionAAMP_OTA {
        +Init()
        +Start()
        +Stop()
        +SetVideoRectangle()
        +EnableContentRestrictions()
        +DisableContentRestrictions()
        -thunderObj (MediaSettings)
    }
    class StreamAbstractionAAMP_RMF {
        +Init()
        +Start()
        +Stop()
        -rmfPlayer
    }
`

## 2. HDMI-In Shim Lifecycle

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant Shim as StreamAbstractionAAMP_HDMIIN
    participant Thunder as ThunderAccess
    participant Shell as RDKShell

    Priv->>Shim: Init()
    Shim->>Thunder: ActivatePlugin("org.rdk.HdmiInput")
    Shim->>Thunder: InvokeJSONRPC("startHdmiInput", {portId})
    Thunder-->>Shim: Success + resolution info
    Shim->>Shell: InvokeJSONRPC("setVisibility", {visible:true})
    Shim->>Shim: Set state = PLAYING
    Shim-->>Priv: eAAMPSTATUS_OK

    Note over Priv: On SetVideoRectangle:
    Priv->>Shim: SetVideoRectangle(x, y, w, h)
    Shim->>Thunder: InvokeJSONRPC("setVideoRectangle", {x,y,w,h})

    Note over Priv: On Stop:
    Priv->>Shim: Stop(clearChannelData)
    Shim->>Thunder: InvokeJSONRPC("stopHdmiInput")
    Shim->>Shell: InvokeJSONRPC("setVisibility", {visible:false})
`

## 3. OTA/ATSC Shim Lifecycle

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant Shim as StreamAbstractionAAMP_OTA
    participant Thunder as ThunderAccess(MediaSettings)
    participant Tuner as ATSCTuner

    Priv->>Shim: Init()
    Shim->>Thunder: ActivatePlugin("org.rdk.MediaSettings")
    Shim->>Shim: Parse URL (live:frequency/program or tune:ocap://...)
    Shim->>Thunder: InvokeJSONRPC("setVideoRectangle", {x,y,w,h})
    Shim->>Thunder: InvokeJSONRPC("startPlayback", {url, audioLang})
    Thunder->>Tuner: Tune to frequency
    Tuner-->>Thunder: Locked + AV started
    Thunder-->>Shim: Playing
    Shim-->>Priv: eAAMPSTATUS_OK

    Note over Shim: Content Restrictions (parental control):
    Priv->>Shim: EnableContentRestrictions()
    Shim->>Thunder: InvokeJSONRPC("enableContentRestrictions")

    Priv->>Shim: DisableContentRestrictions(grace, time, eventChange)
    Shim->>Thunder: InvokeJSONRPC("disableContentRestrictions", {grace, time})

    Note over Priv: On Stop:
    Priv->>Shim: Stop()
    Shim->>Thunder: InvokeJSONRPC("stopPlayback")
`

## 4. RMF Shim Lifecycle

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant Shim as StreamAbstractionAAMP_RMF
    participant RMF as RMFMediaSource

    Priv->>Shim: Init()
    Shim->>Shim: Parse ocap:// URL
    Shim->>RMF: Open(sourceUrl)
    RMF-->>Shim: Source ready
    Shim->>RMF: Play()
    RMF-->>Shim: Playing
    Shim-->>Priv: eAAMPSTATUS_OK

    Note over Priv: On Stop:
    Priv->>Shim: Stop()
    Shim->>RMF: Stop()
    Shim->>RMF: Close()
`

## 5. Composite-In Shim

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant Shim as StreamAbstractionAAMP_COMPOSITEIN
    participant Thunder as ThunderAccess

    Priv->>Shim: Init()
    Shim->>Thunder: ActivatePlugin("org.rdk.CompositeInput")
    Shim->>Thunder: InvokeJSONRPC("startCompositeInput", {portId})
    Thunder-->>Shim: Success
    Shim-->>Priv: eAAMPSTATUS_OK

    Priv->>Shim: Stop()
    Shim->>Thunder: InvokeJSONRPC("stopCompositeInput")
`

---

## Key Implementation Details

| Shim | Input Source | Interface | Key Feature |
|------|-------------|-----------|-------------|
| **HDMIIN** | HDMI port | Thunder (HdmiInput plugin) | Resolution detection, port selection |
| **COMPOSITEIN** | Composite/CVBS | Thunder (CompositeInput plugin) | Port selection |
| **OTA** | ATSC antenna | Thunder (MediaSettings) | Parental controls, channel tuning |
| **RMF** | QAM cable | RMFMediaSource | ocap:// URL scheme |
| **VIDEOIN** | Base class | Thunder + RDKShell | Video rectangle, visibility |
