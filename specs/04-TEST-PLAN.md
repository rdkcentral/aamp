# 04 - Test Plan

## Overview
Test cases in Given/When/Then format for each requirement.

---

## REQ-001: Stream Playback Initialization

### TC-001.1: HLS Tune
- **Given:** A valid HLS URL with .m3u8 extension
- **When:** Tune() is called with the URL
- **Then:** TuneHelper creates StreamAbstractionAAMP_HLS, Init() parses master playlist, Start() begins fetching

### TC-001.2: DASH Tune
- **Given:** A valid DASH URL with .mpd extension
- **When:** Tune() is called with the URL
- **Then:** TuneHelper creates StreamAbstractionAAMP_MPD, Init() parses manifest, periods enumerated

### TC-001.3: Tune with active stream (retune)
- **Given:** A stream is currently playing
- **When:** Tune() is called with a new URL
- **Then:** TeardownStream() stops current stream, then TuneHelper initializes new stream

### TC-001.4: Invalid URL
- **Given:** A malformed or unsupported URL
- **When:** Tune() is called
- **Then:** Error event dispatched (AAMP_EVENT_TUNE_FAILED), state set to eSTATE_ERROR

---

## REQ-002: Adaptive Bitrate Switching

### TC-002.1: Ramp up on stable bandwidth
- **Given:** Player is playing at a lower profile, bandwidth is stable above next profile threshold
- **When:** ABR check triggers (fragment cached)
- **Then:** GetDesiredProfileOnSteadyState returns higher profile, switch occurs

### TC-002.2: Ramp down on buffer underrun
- **Given:** Buffer occupancy drops below minimum threshold
- **When:** ABR check triggers
- **Then:** GetDesiredProfileOnBuffer returns lower profile, switch occurs immediately

### TC-002.3: No switch during ramp-up cooldown
- **Given:** A ramp-up just occurred
- **When:** ABR check triggers within cooldown period
- **Then:** No profile change (hysteresis enforced)

---

## REQ-003: DRM License Acquisition

### TC-003.1: First-time license acquisition
- **Given:** Encrypted content with PSSH in init segment
- **When:** Init segment is parsed
- **Then:** DRM helper selected, license request sent, key received before first frame

### TC-003.2: Key rotation
- **Given:** Content with periodic key rotation
- **When:** New key ID encountered during playback
- **Then:** Pre-fetcher acquires new license, no playback interruption

### TC-003.3: License failure
- **Given:** DRM server returns error
- **When:** License request fails
- **Then:** Retry attempted; if all retries fail, AAMP_EVENT_DRM_ERROR dispatched

---

## REQ-004: GStreamer Pipeline Management

### TC-004.1: Pipeline creation on tune
- **Given:** No active pipeline
- **When:** First buffer ready for injection
- **Then:** Configure() creates pipeline with appsrc, decoder, sink elements

### TC-004.2: Flush on seek
- **Given:** Active pipeline playing content
- **When:** Seek requested
- **Then:** Flush() sends flush-start/stop, resets position, resumes from new position

### TC-004.3: Pipeline stop
- **Given:** Active pipeline
- **When:** Stop() called
- **Then:** Pipeline set to NULL state, elements unreferenced, resources freed

---

## REQ-005: Time-Shift Buffer

### TC-005.1: Segment storage
- **Given:** Live stream with TSB enabled
- **When:** New segment downloaded
- **Then:** Segment stored in TSB with metadata, retrievable by position

### TC-005.2: Seek within TSB window
- **Given:** TSB contains 30 minutes of content
- **When:** User seeks back 10 minutes
- **Then:** TsbReader returns correct segment, playback resumes from seek position

### TC-005.3: Eviction of old segments
- **Given:** TSB at maximum capacity
- **When:** New segment arrives
- **Then:** Oldest segment evicted, new segment stored

---

## REQ-006: Event Notification

### TC-006.1: Tune success event
- **Given:** Tune completes successfully
- **When:** First frame rendered
- **Then:** AAMP_EVENT_TUNED dispatched to all registered listeners

### TC-006.2: Listener registration
- **Given:** Application registers listener for AAMP_EVENT_BITRATE_CHANGED
- **When:** ABR switch occurs
- **Then:** Only bitrate-change listeners notified, not all listeners

---

## REQ-007: Configuration Management

### TC-007.1: File config loading
- **Given:** aamp.cfg file exists with custom settings
- **When:** Player initializes
- **Then:** Config values loaded from file with AAMP_CFG owner priority

### TC-007.2: App override
- **Given:** Config value set by file (AAMP_CFG priority)
- **When:** Application sets same value (APP priority)
- **Then:** APP value takes effect (higher priority)

---

## REQ-008: Scheduled Task Execution

### TC-008.1: Task scheduling
- **Given:** Scheduler is running
- **When:** ScheduleTask() called with function and delay
- **Then:** Task executes after specified delay on scheduler thread

### TC-008.2: Task removal
- **Given:** Task is queued
- **When:** RemoveAllTasks() called
- **Then:** Pending tasks cancelled, no execution occurs

---

## REQ-009: Network Download Management

### TC-009.1: Successful download
- **Given:** Valid segment URL
- **When:** Download initiated
- **Then:** Segment data returned, CMCD headers attached to request

### TC-009.2: Retry on failure
- **Given:** Server returns 503
- **When:** Download fails
- **Then:** Retry attempted with backoff; success on retry returns data

### TC-009.3: Timeout enforcement
- **Given:** Server is unresponsive
- **When:** Download timeout expires
- **Then:** Download aborted, error returned to caller

---

## REQ-010-015: Fragment Collectors, Sinks, Shims, Middleware

### TC-010.1: HLS playlist parsing
- **Given:** Valid M3U8 playlist content
- **When:** IndexPlaylist() called
- **Then:** All segments parsed with duration, URI, sequence number

### TC-011.1: MPD period transition
- **Given:** Multi-period MPD manifest
- **When:** Current period ends
- **Then:** Next period initialized, segments fetched from new adaptation sets

### TC-013.1: HDMI-in shim
- **Given:** HDMI source connected
- **When:** Tune to hdmiin:// URL
- **Then:** StreamAbstractionAAMP_HDMIIN created, video frames captured and injected
