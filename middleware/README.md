# ⚠️ DEPRECATION NOTICE ⚠️

**This `aamp/middleware` folder is deprecated and scheduled for removal.**

## Background

The middleware player interface components have been separated into an external repository:
- **External repo**: https://github.com/rdkcentral/middleware-player-interface

## Current Status (as of VPAAMP-725)

- ✅ **OSX/macOS simulator builds**: Now use external middleware repo by default
- ✅ **Ubuntu/Linux simulator builds**: Now use external middleware repo by default
- ⚠️ **Production Yocto builds**: Still use this folder when `CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES` is not set
- ⚠️ **Unit tests (L1)**: Still reference this folder directly in CMakeLists.txt files

## Migration Path

This folder will be **deleted** once:
1. All production Yocto recipes set `CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES=ON`
2. Unit tests are refactored to use external middleware headers/libs
3. CI/CD pipeline validates builds without this folder

## For Developers

**If you are working on middleware code:**
- Make changes in the external `middleware-player-interface` repository
- Do NOT modify files in this `aamp/middleware` folder
- Simulator builds will automatically use the external repo

**If you need to use a local middleware repo for development:**
```bash
# Option 1: Place middleware-player-interface as sibling directory (auto-detected)
cd /path/to/rdke
git clone https://github.com/rdkcentral/middleware-player-interface.git

# Option 2: Specify explicit path
./scripts/install-aamp.sh --middleware-player-interface-local-path=/path/to/middleware-player-interface

# Option 3: Use specific commit from GitHub
./scripts/install-aamp.sh --middleware-player-interface-commit-id=<commit-hash>
```

## Timeline

- **Phase 1 (VPAAMP-725)**: ✅ Simulator builds migrated to external repo
- **Phase 2 (TBD)**: Refactor unit tests to use external middleware
- **Phase 3 (TBD)**: Coordinate Yocto recipe updates
- **Phase 4 (TBD)**: Delete this folder

## Questions?

See VPAAMP-725 for implementation details or contact the AAMP team.
