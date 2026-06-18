## Plan: Inject Direct-Rialto DRM Session Creation

Move Rialto media key session construction out of middleware/drm/rialto by introducing an injection seam so middleware/drm no longer directly instantiates RialtoMediaKeySessionAdapter. Keep DrmSessionManager ownership/lifecycle intact while allowing AAMP/direct-rialto to provide a DrmSession implementation when direct-rialto mode is enabled.

**Steps**
1. Baseline dependency confirmation (complete): verify only one production call path creates sessions via factory.
2. Phase 1 - Add creation seam in factory (*blocks phase 2*):
   1. Extend DrmSessionFactory with optional creator registration (function object) that returns DrmSession* (or unique_ptr released to raw pointer at boundary).
   2. In GetDrmSession(), if useDirectRialto=true and creator is registered, call injected creator first; fallback to existing logic when absent.
   3. Keep existing non-direct-rialto behavior untouched.
3. Phase 2 - Shift direct-rialto construction to AAMP (*depends on phase 1*):
   1. In direct-rialto startup path (AampDrmBridge or AampRialtoPlayer initialization), register creator callback.
   2. Creator builds RialtoMediaKeySystem and RialtoMediaKeySessionAdapter (now located under direct-rialto) and returns DrmSession*.
   3. Ensure creator registration happens before any createDrmSession() calls.
4. Phase 3 - Decouple middleware from Rialto adapter files (*depends on phase 1 and 2*):
   1. Remove direct include/instantiation of RialtoMediaKeySessionAdapter from middleware/drm/DrmSessionFactory.cpp.
   2. Relocate Rialto adapter/system/session files from middleware/drm/rialto into direct-rialto (or direct-rialto/drm) and update includes.
   3. Update CMake: remove moved files from middleware target source list, add to aamp target source list, preserve Rialto include/lib linkage.
5. Phase 4 - Remove useDirectRialto config plumbing from middleware/drm (*depends on phase 1 and 2*):
   1. Remove `useDirectRialto` parameter from DrmSessionManager::UpdateDRMConfig declaration/definition and from `configs::mUseDirectRialto`.
   2. Remove `eAAMPConfig_useDirectRialto` argument passing from drm/AampDRMLicManager.cpp getConfigs() to UpdateDRMConfig().
   3. Replace DrmSessionFactory::GetDrmSession call sites in middleware/drm to use creator-seam driven selection (not m_drmConfigParam state).
   4. Delete remaining middleware/drm references to `mUseDirectRialto` and validate no behavior change for non-direct-rialto flows.
6. Phase 5 - Optional manager-level seam alternative (only if factory seam rejected) (*parallel design branch*):
   1. Add optional injected DrmSession parameter/provider into DrmSessionManager::getDrmSession().
   2. If provided, manager uses injected session instead of factory for new slot creation.
   3. Keep fallback path through DrmSessionFactory for legacy callers.
7. Phase 6 - Move or recreate RialtoMediaKey unit tests in top-level test/utests (*depends on phase 3 file move*):
   1. Migrate test suites from middleware/test/utests/tests/RialtoMediaKeySessionTests, middleware/test/utests/tests/RialtoMediaKeySystemTests, and middleware/test/utests/tests/RialtoMediaKeySessionAdapterTests into test/utests/tests/ (either copy and adapt, or recreate with equivalent coverage).
   2. Update migrated test CMake files to reference new production source paths under direct-rialto (instead of middleware/drm/rialto paths).
   3. Register new test subdirectories in test/utests/tests/CMakeLists.txt and remove/disable old registrations in middleware/test/utests/tests/CMakeLists.txt to avoid duplicate targets.
   4. Ensure test include ordering keeps rialto stubs/mocks precedence where needed (current adapter test uses BEFORE include ordering).
8. Phase 7 - Remaining tests and verification updates (*depends on chosen seam and move*):
   1. Update unit tests around DrmSessionFactory and DrmSessionManager to cover injected creator path, fallback path, and null creator failure handling.
   2. Add/adjust direct-rialto tests around AampDrmBridge registration timing and successful mksId propagation.
   3. Verify no regressions for non-direct-rialto and ClearKey/OCDM flows.

**Relevant files**
- /home/anshephe/github/aamp/middleware/drm/DrmSessionFactory.h - add creator registration API and optionally accessor/reset for tests.
- /home/anshephe/github/aamp/middleware/drm/DrmSessionFactory.cpp - injected-creator path and fallback behavior.
- /home/anshephe/github/aamp/middleware/drm/DrmSessionManager.cpp - currently sole factory caller in getDrmSession; only touch if manager-level seam chosen.
- /home/anshephe/github/aamp/middleware/drm/DrmSessionManager.h - remove `mUseDirectRialto` config field and UpdateDRMConfig parameter.
- /home/anshephe/github/aamp/direct-rialto/AampDrmBridge.h - registration responsibility documentation and ordering contract.
- /home/anshephe/github/aamp/direct-rialto/AampDrmBridge.cpp - register creator callback during bridge/player initialization.
- /home/anshephe/github/aamp/drm/AampDRMLicManager.cpp - simplify getConfigs()/UpdateDRMConfig call by removing direct-rialto config argument.
- /home/anshephe/github/aamp/middleware/drm/rialto/RialtoMediaKeySessionAdapter.h - move target or split interface as needed.
- /home/anshephe/github/aamp/middleware/drm/rialto/RialtoMediaKeySessionAdapter.cpp - move implementation.
- /home/anshephe/github/aamp/middleware/drm/rialto/RialtoMediaKeySystem.h - move implementation dependency.
- /home/anshephe/github/aamp/middleware/drm/rialto/RialtoMediaKeySystem.cpp - move implementation dependency.
- /home/anshephe/github/aamp/middleware/drm/rialto/RialtoMediaKeySession.h - move implementation dependency.
- /home/anshephe/github/aamp/middleware/drm/rialto/RialtoMediaKeySession.cpp - move implementation dependency.
- /home/anshephe/github/aamp/middleware/CMakeLists.txt - remove moved Rialto DRM sources from playergstinterface list.
- /home/anshephe/github/aamp/CMakeLists.txt - add moved sources to aamp target and ensure required includes/libs remain.
- /home/anshephe/github/aamp/direct-rialto/IDrmBridge.h - no mandatory signature change for factory-seam approach; keep stable unless manager-seam selected.
- /home/anshephe/github/aamp/middleware/test/utests/tests/CMakeLists.txt - remove/disable RialtoMediaKey* test suite subdirectories after migration.
- /home/anshephe/github/aamp/middleware/test/utests/tests/RialtoMediaKeySessionTests/CMakeLists.txt - source test blueprint to migrate or deprecate.
- /home/anshephe/github/aamp/middleware/test/utests/tests/RialtoMediaKeySystemTests/CMakeLists.txt - source test blueprint to migrate or deprecate.
- /home/anshephe/github/aamp/middleware/test/utests/tests/RialtoMediaKeySessionAdapterTests/CMakeLists.txt - source test blueprint to migrate or deprecate.
- /home/anshephe/github/aamp/test/utests/tests/CMakeLists.txt - add top-level registration for migrated RialtoMediaKey* test suites.
- /home/anshephe/github/aamp/test/utests/tests/RialtoMediaKeySessionTests/CMakeLists.txt - new or migrated test target.
- /home/anshephe/github/aamp/test/utests/tests/RialtoMediaKeySystemTests/CMakeLists.txt - new or migrated test target.
- /home/anshephe/github/aamp/test/utests/tests/RialtoMediaKeySessionAdapterTests/CMakeLists.txt - new or migrated test target.

**Verification**
1. Static dependency check:
   1. Confirm only one production factory call remains at middleware/drm/DrmSessionManager.cpp (getDrmSession path).
   2. Confirm no direct references to RialtoMediaKeySessionAdapter remain in middleware after move.
   3. Confirm no middleware/drm references remain to `mUseDirectRialto` or UpdateDRMConfig `useDirectRialto` parameter.
2. Build verification:
   1. Build middleware target and aamp target with direct-rialto enabled.
   2. Build with direct-rialto disabled (if supported in your environment) to verify fallback compile path.
3. Runtime/behavior verification:
   1. Run encrypted direct-rialto playback and verify mksId is non-negative and propagated to segment metadata.
   2. Run non-direct-rialto playback to confirm legacy OCDM/ClearKey behavior unchanged.
4. Unit tests:
   1. Build and run migrated top-level suites: RialtoMediaKeySessionTests, RialtoMediaKeySystemTests, RialtoMediaKeySessionAdapterTests under test/utests.
   2. Execute affected DrmSessionFactory/DrmSessionManager test suites.
   3. Execute affected direct-rialto test suites (AampRialto* tests touching DRM bridge path).
5. Duplicate/legacy guard:
   1. Confirm middleware/test/utests no longer registers competing RialtoMediaKey* suite names.
   2. Confirm CI/ctest discovery lists only one instance of each RialtoMediaKey* suite.

**Decisions**
- Confirmed dependency statement: DrmSessionFactory::GetDrmSession has one production call site in DrmSessionManager::getDrmSession.
- Recommended design: factory-level injected creator (minimal API ripple) rather than manager signature expansion.
- Alignment decision: implement factory seam first (manager seam deferred unless factory seam proves insufficient).
- Alignment decision: remove middleware/drm useDirectRialto config plumbing once factory seam is in place.
- Scope included: moving Rialto adapter/session/system construction ownership to direct-rialto while preserving DrmSessionManager slot/cache/lifecycle behavior.
- Scope excluded: broad DRM architecture refactor, non-direct-rialto modernization, unrelated direct-rialto state-machine/injection changes.
- Constraint note: direct-rialto instruction file warns against broad cross-directory refactors; this effort intentionally spans middleware + direct-rialto and should be treated as an explicitly approved cross-boundary change.

**Further Considerations**
1. Ownership boundary for returned session: prefer factory API internally using unique_ptr at creation seam, but preserve existing raw-pointer storage in DrmSessionManager until a later cleanup to avoid wide churn.
2. Registration lifecycle: define whether creator registration is one-time global at startup or per-player instance; one-time global is simpler but must be made thread-safe and test-resettable.
3. CMake linkage direction: avoid making middleware depend on aamp; middleware must only know base DrmSession and callback seam, while concrete direct-rialto implementation lives in aamp target.