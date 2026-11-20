# Fixtures

- `catalog/mediakind-test-20261120.json`: the MSF catalog published on the reference relay
  (namespace `mediakind-test`), fetched 2026-11-20. Committed: it carries codec headers only.
- `captures/reference.nmqecap`: a capture of the reference publication's catalog, video and
  audio Objects. **Not committed** (it contains broadcast media). Record one on the
  Enterprise network with:

  ```bash
  NMQE_RELAY_URL=<reference relay URL> ./build/nmqe-player --pipeline null --duration 15 --telemetry /dev/null \
      --capture tests/fixtures/captures/reference.nmqecap
  ```

  Tests that need it (`CmafTrackParser.ReferenceFragmentsHaveContinuousTiming`,
  `ReplayNull.PlaysReferenceCapture`) skip when it is absent. The format is described in
  `src/replay/Capture.h`.
