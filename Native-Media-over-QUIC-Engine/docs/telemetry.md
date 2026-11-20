# Telemetry (JSON Lines, schema 1)

Every line is one JSON object:

```json
{"schema":1,"run":"<run id>","mono_ns":<steady clock>,"wall_ms":<unix ms>,"cat":"<category>","ev":"<event>", ...fields}
```

The categories and events below are always emitted. Entries marked *(trace)* need `--trace`.

| cat / ev | Meaning |
|---|---|
| `session/run_config` | Effective configuration (including the platform profile) and source identity |
| `tune/milestone` | First occurrence of a tune milestone, `t_ms` since `tune_requested` (see below) |
| `transport/<kind>` | Source events: `connecting`, `connected`, `broadcast_ready`, `subscribed`, `subscription_ended`, `error`, `closed`; `group_begin`/`group_end` *(trace)* unless a group ends with an error |
| `object/receive` *(trace)* | Track, Group, Object, size, receive wallclock |
| `catalog/received`, `catalog/changed`, `catalog/parse_error` | Version, track declarations, unknown fields |
| `player/track_selection` | Selected video/audio tracks and effective target latency |
| `media/source_attached`, `media/pipeline` | Codec configuration; backend description (elements, caps, sinks, clock) |
| `media/fragment` *(trace)* | Samples, key frames, start/end PTS, duration, tfdt, `prft` |
| `media/submit` *(trace)* | Access unit submitted: PTS, key, size, Group, time queued since receipt |
| `media/playback_state`, `media/buffer_underflow`, `media/error`, `media/qos` *(trace)* | Backend observations |
| `queue/overflow_drop`, `queue/duration_drop` | Bounded-queue drops (Object queue by bytes; demuxed queue by duration) |
| `discontinuity/group_gap`, `discontinuity/stale_object`, `discontinuity/codec_changed_unsupported` | Discontinuity evidence |
| `media/fragment_without_leading_sap` | A video Group that did not start at a sync sample (random-access contract check) |
| `stall/start`, `stall/end` | No position progress for 500 ms while playing; queue state at start, duration at end |
| `catch_up/state` | Live-edge distance crossed `--catch-up-threshold-ms` (`action: none`: the POC measures, it does not act) |
| `recovery/action` | Resubscribe after a failed subscription |
| `latency/fragment_presented` *(trace)* | Receive-to-present per fragment start |
| `status/periodic` | Position, live-edge distance, per-track queue/backend/render state, RSS, CPU |
| `summary/run` | End-of-run measures (below) |

## Tune milestones

`tune_requested` → `transport_connecting` → `setup_complete` (QUIC + WebTransport + MOQT SETUP)
→ `broadcast_ready` → `subscribed_catalog` → `catalog_received` → `init_video`/`init_audio`
→ `subscribe_*_sent` → `subscribed_*` → `first_object_*` → `join_*` (first usable random access)
→ `play_requested` → `first_*_submitted` → `first_*_at_sink` → `first_frame` → `pipeline_playing`
→ `stable_playback` → `stable_live_edge`.

## Measurement definitions

- **Live-edge distance**: the newest media end time known to the player (end PTS of the newest
  parsed video fragment) minus the current presentation position. This is separate from
  end-to-end (capture-to-display) latency. With whole-fragment Objects it saw-tooths by one fragment duration.
- **Stable playback**: one `--stable-window-ms` window of position progress with no stall, measured
  since the first frame or the last stall.
- **Stable live edge**: the floor (minimum) of the live-edge distance in two consecutive windows
  agrees within 250 ms, and the distance is under the catch-up threshold.
- **Post-tune catch-up** = `stable_live_edge` − `first_frame`.
- **Receive-to-present**: from the Object's receipt in the player to the presentation position
  passing the fragment's first PTS, at 100 ms tick resolution.
- **A/V offset (sink input)**: video minus audio PTS of the latest buffer to reach each sink pad.
  This is not a presentation measurement; use camera measurement for lip-sync claims.
- **prft latency**: wallclock now minus the producer time implied by the latest `prft` for the
  rendered position. Only emitted when `prft` is present; it is absent from the current
  publication.
- **End-to-end (capture-to-display)**: not measured by the client. Use the camera and a burned-in source timestamp.

Summaries report distributions as `{n, min, p50, p95, max, mean}`. Use `tools/summarize_runs.py` to
aggregate repeated runs.
