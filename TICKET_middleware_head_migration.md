# Ticket: Migrate to HEAD of middleware-player-interface

## Summary
Remove dependency on patched middleware commit bd2b3b1 and migrate to using HEAD of middleware-player-interface repository in all build environments (macOS, GitHub CI for aamp, GitHub CI for aamp_test_internal).

## Background
Currently, all builds use middleware-player-interface commit `bd2b3b1` (June 1, 2025) with patches applied from `OSX/patches/middleware-fixes-4c1d90a.patch`. This patch adds:
- `setPtsOffset()` method to `WebVttSubtecParser` and base class `SubtitleParser`
- GStreamer base-1.0 dependency fixes for macOS builds

This was necessary because commit bd2b3b1 was missing these features, but using an old commit with patches is not sustainable long-term.

## Current State
**Files that reference bd2b3b1 commit:**
- `aamp/scripts/install_options.sh` (line 22): `OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID="bd2b3b1"`
- `aamp/scripts/install_middleware_interfaces.sh` (lines 131-143, 197-202): Checkout logic
- `aamp/scripts/install_middleware_interfaces.sh` (lines 250-282): Patch application
- `aamp/.github/workflows/L1-tests.yml` (lines 25-27): CI checkout and patch
- `aamp_test_internal/.github/workflows/manual-l1-l2-tests.yml` (lines 132-134): CI checkout and patch

**Patch file:**
- `aamp/OSX/patches/middleware-fixes-4c1d90a.patch`

## Goal
Update middleware-player-interface repository to include all necessary features at HEAD, then remove all references to the specific commit and patch.

## Tasks

### 1. Verify middleware-player-interface HEAD has required features
- [ ] Check that `setPtsOffset()` method exists in HEAD of middleware-player-interface
  - In `subtec/subtecparser/WebVttSubtecParser.cpp`
  - In `subtec/subtecparser/WebVttSubtecParser.hpp`
  - In `subtitle/subtitleParser.h` (base class)
- [ ] Verify GStreamer base-1.0 dependency fixes are in HEAD
- [ ] If missing, submit PR to middleware-player-interface to add these features

### 2. Test with HEAD (before removing patches)
- [ ] Locally test macOS build with HEAD of middleware-player-interface (no patch)
  - Run: `./install-aamp.sh --middleware-player-interface-commit-id=HEAD`
  - Verify L1 tests pass: `cd test/utests && ./run.sh`
- [ ] Test GitHub CI with HEAD (create test branch)
  - Modify `.github/workflows/L1-tests.yml` to use HEAD instead of bd2b3b1
  - Remove patch application step
  - Verify all L1 tests pass

### 3. Remove commit pinning and patches
Once HEAD is verified to work:
- [ ] Remove commit ID from `aamp/scripts/install_options.sh`
- [ ] Simplify `aamp/scripts/install_middleware_interfaces.sh`:
  - Remove commit checkout logic (lines 131-143, 197-202)
  - Remove patch application logic (lines 250-282)
- [ ] Update `aamp/.github/workflows/L1-tests.yml`:
  - Remove `git checkout bd2b3b1` line
  - Remove `patch -p1 < ...` line
- [ ] Update `aamp_test_internal/.github/workflows/manual-l1-l2-tests.yml`:
  - Remove `git checkout bd2b3b1` line
  - Remove `patch -p1 < ...` line
- [ ] Archive or delete `aamp/OSX/patches/middleware-fixes-4c1d90a.patch`
  - Consider keeping with a README explaining it's historical

### 4. Update documentation
- [ ] Update `aamp/OSX/patches/middleware-build-fixes-summary.md` to note the patch is no longer needed
- [ ] Add comment in install scripts explaining middleware is now used at HEAD
- [ ] Update any developer documentation that references the specific commit

## Success Criteria
- [ ] All L1 tests pass on macOS with HEAD of middleware-player-interface (no patches)
- [ ] All L1 tests pass on GitHub CI (aamp repo) with HEAD of middleware-player-interface
- [ ] All L1 tests pass on GitHub CI (aamp_test_internal repo) with HEAD of middleware-player-interface
- [ ] No references to commit bd2b3b1 remain in the codebase
- [ ] No patch files are being applied during build

## Dependencies
- Requires middleware-player-interface repository to have all necessary features at HEAD
- May require coordination with middleware-player-interface maintainers

## Risks
- If middleware-player-interface HEAD has breaking API changes, tests may fail
- May need to update fakes/mocks to match new middleware API

## Estimated Effort
- Small (1-2 days) if middleware HEAD already has all features
- Medium (3-5 days) if middleware PR is needed first
