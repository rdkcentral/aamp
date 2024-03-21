RDKAAMP-2232 ABR Modeling Tool 

Goals for this tool:
- make it easier to understand and communicate how AAMP ABR works
- support quick component testing of ABR for different inputs/environment, without having to set up real device, streams and Charles proxy
- provide framework that can be used to explore changes to AAMP ABR configuration, and/or algorithm enhancements


Issues raised (see ticket for details)
- "unfilled-buffering" shows a steady state where ABR is stable (no underflow) but buffering never builds up.
- abr-thrashing shows constant ramping between two profiles
avoidable-underflow looks like a real world edge case that we could fix; here a download at current - profile is attempted, despite buffer health being dangerously low.

TODO:
- update configuration to match more realistically production manifest/CBD fragment sizes
- review implementation to ensure accurate reflection of AAMP ABR
- extend modeling to support handling for fragment download failurs/errors, and corresponding retry logic
- (maybe) make modeling more realistic by also including audio, sap samples
- support modeling of ABR in the context of FOG+AAMP, not just AAMP-only playback

FOG Notes:
- FOG ABR performance is believed to be more brittle than AAMP-only playback
- FOG has more on shoulders, downloading iframe fragments and alternate audio tracks in parallel
- FOG ABD has subtle implementation differences from AAMP
- AAMP shares buffer health status to FOG (ensure this is fully/correctly leveraged)
- FOG supports parallel downloads of video fragments (used in production today?)
- FOG has behavior variations for background downloads and just-in-time downloads


Goals for an ABR algorithm:
1. Avoid underflow
2. Minimize tune time
3. Minimize time to top profile
4. Maximize time at top/higher profiles; if we can play at higher profiles without underflow, we should
5. Minimize ABR thrashing; frequently switching back and forth between two adjacent profiles could give visual artifacts
6. Build up buffering to protect from unexpected network issues

Suggested heuristics:
1. for each profile, estimate how long it will take to download with current network bandwidth.
2. pick best video segment that can be downloaded:
- in less time than its fragment duration
- before buffers run dry
3. abort in-progress download if network is sluggish to point where we predict buffers will run dry, and switching to smaller fragment would avoid underflow
