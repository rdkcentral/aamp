<!--
If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Direct Rialto

This folder holds the Direct Rialto playback back end. `AampRialtoPlayer`
is a `StreamSink` that calls the Rialto client API directly, instead of
building a GStreamer pipeline. It is selected with the `useDirectRialto`
configuration.

Start with the [overview](../docs/direct-rialto/README.md), which takes about
5 to 10 minutes to read.

## Documentation

| Question | Read |
|---|---|
| What is it and how does it fit in? | [Overview](../docs/direct-rialto/README.md) |
| Which class does what, and who owns it? | [Architecture](../docs/direct-rialto/architecture.md) |
| How does it differ from the GStreamer path? | [Comparison](../docs/direct-rialto/comparison.md) |
| What are the states and gates? | [Lifecycle](../docs/direct-rialto/lifecycle.md) |
| How do startup, data, seek and DRM work? | [Playback flow](../docs/direct-rialto/playback-flow.md) |
| Which thread runs what? Is it safe to change? | [Threading](../docs/direct-rialto/threading.md) |
| How are errors handled? | [Error handling](../docs/direct-rialto/error-handling.md) |
| How is it enabled, built and tested? | [Integration](../docs/direct-rialto/integration.md) |
| What are the known limitations and risks? | [Limitations](../docs/direct-rialto/limitations.md) |

## Key files

| File | Role |
|---|---|
| [`AampRialtoPlayer.cpp`](AampRialtoPlayer.cpp) | The sink and its policy |
| [`AampPlayerStateMachine.cpp`](AampPlayerStateMachine.cpp) | Player lifecycle states |
| [`AampRialtoMediaSource.cpp`](AampRialtoMediaSource.cpp) | Per-track `needData` / `haveData` handshake |
| [`PrivateInstanceAAMPNotifiable.cpp`](PrivateInstanceAAMPNotifiable.cpp) | Posts Rialto events to AAMP's scheduler |
| [`RialtoMediaKeySessionAdapter.cpp`](RialtoMediaKeySessionAdapter.cpp) | DRM sessions through Rialto |

Please update the documentation when you change behaviour described there,
especially the threading and lifecycle pages.
