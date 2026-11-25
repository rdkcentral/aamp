# Introduction

ABR (Automatic BitRate) Library is an independent library for automatic bitrate switching. ABR will start at a reasonable bitrate and will ramp up or down, depending on network conditions.  Only one bitrate will be recorded at any one time, but to the player, it will look like a single, consistent video playback.

Here is a brief introduction to the workflow of ABR library:

(1) ABR library requires a list of profiles (tracks) as input, each profile has a bandwidth.

(2) ABR library will choose a profile whose bandwidth best matches with the current network bandwidth. That is, the library will output the profile index with the desired bitrate base on the network condition.

A HLS/DASH stream may contain several profiles in different bandwidths, for adapting to different network conditions.

The ABR library supports both HLS and DASH.

# Library design principle

The input of the ABR library includes

- A list of profile (track) info
- The currently available network bandwidth
- The currently chosen profile index.

The output of the ABR library is
- The chosen (desired) profile index.

ABR library is encapsulated as a class `ABRManager`. `HybridABRManager` extends `ABRManager` with additional AAMP-specific ABR configuration (cache life/length, buffer thresholds, etc.) used by the player. The `ABRManager` provides the following functions

## Constructor

It provides a void-parameter constructor `ABRManager::ABRManager()`, so it's very easy to initialize it.

## Input

`struct ABRManager::ProfileInfo` is the data structure of the required profile info, it contains the following fields.

- `isIframeTrack`. A flag to denote if it is the iframe track.
- `bandwidthBitsPerSecond`. Bandwidth per second, i.e, bitrate.
- `width`. The width of resolution
- `height`. The height of resolution.
- `periodId` (optional). The period-Id the profile belongs to, used to keep multiple DASH periods' profile lists separate.
- `userData` (optional). Caller-defined profile index / period index, returned by `getUserDataOfProfile`.

ABR library provides the following function to add profile info into the manager

- `void ABRManager::addProfile(ABRManager::ProfileInfo profile)`

  This method is used to add a profile into the manager.

## Output

ABR library provides several functions for choosing profiles in different ways, so it provides flexible way for user to use in different scenarios. 

- `int ABRManager::getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId = "")`

  Get a reasonable bitrate at startup - get initial profile index, choose the medium profile or the profile whose bitrate >= the default bitrate.

- `int ABRManager::getBestMatchedProfileIndexByBandWidth(int bandwidth)`

  According to the given bandwidth, return the best matched profile index, or `INVALID_PROFILE` (-1) if no non-iframe profiles are registered. Callers must check for `INVALID_PROFILE` before using the result as a container index.

- `int ABRManager::getDesiredIframeProfile()`

  Get the current desired iframe profile index.

- `int ABRManager::getLowestIframeProfile()`

  Get the current available lowest iframe profile index.

- `int ABRManager::getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId = "")`

  Ramps down the profile one step to get the profile index of a lower bitrate.

- `int ABRManager::getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId = "")`

  Ramps up the profile one step to get the profile index of a higher bitrate.

- `int ABRManager::getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, long currentBandwidth, long networkBandwidth, int nwConsistencyCnt = 2, const std::string& periodId = "")`

  According to the current bandwidth, current available network bandwidth and current chosen profile index, do ABR by ramping bitrate up/down. Returns the profile index with the bitrate matched with the current bitrate.

- `int ABRManager::getProfileOfBandwidth(long bandwidth)`

  Get the profile index matching an exact bandwidth.

- `int ABRManager::getMaxBandwidthProfile(const std::string& periodId = "")`

  Get the profile index with the maximum bandwidth.

- `int ABRManager::getProfileIndexForLowestBandwidth()`

  Get the profile index with the lowest bandwidth.

- `int ABRManager::getClosestProfileIndexByBandwidth(long inputBandwidth)`

  Get the profile index whose bandwidth most closely matches the given bandwidth.

## Update

ABR library provides the following functions to update the internal state of the manager.

- `void ABRManager::updateProfile()`

  If the profiles are changed, ABR library provides this function to update the profiles. Concretely, it will update the lowest / desired profile index according to the profile info, the lowest / desired profile index will used in the output functions.

- `void ABRManager::clearProfiles()`

  Remove all profiles.

- `int ABRManager::removeProfiles(std::vector<long> profileBPS, int currentProfileIndex, const std::string& periodId = "")`

  Remove the given profile bitrates from the ABR data and return the adjusted current profile index.

## Auxiliary functions

ABR library provides the following auxiliary functions to make the library easier to use.
- `int ABRManager::getProfileCount()`

  Get the number of profiles.

- `long ABRManager::getBandwidthOfProfile(int profileIndex)`

  Get the bandwidth of a profile

- `void ABRManager::setDefaultInitBitrate(long defaultInitBitrate)`

  Change the default initialize bitrate for `ABRManager::getInitialProfileIndex`, if you don't change it, the default value is 1000000

- `bool ABRManager::isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId = "")`
  Check if the bitrate of currentProfileIndex reaches to the lowest.

- `void ABRManager::setDefaultIframeBitrate(long defaultIframeBitrate)`

  Change the default iframe bitrate.

- `int ABRManager::getUserDataOfProfile(int profileIndex)`

  Get the caller-defined `userData` (profile/period index) stored for a profile.

- `void ABRManager::setLogDirectory(char driveName)`

  Change the drive letter used for the ABR log file path.

- `static void ABRManager::setPersistBandwidth(long bitrate)` / `static long ABRManager::getPersistBandwidth()`

  Set/get a network bandwidth value persisted across `ABRManager` instances.

Logging is not pluggable via a caller-supplied function pointer. Internally, log
output goes through `ABRManager::logprintf` (used by `ABRManager`/`HybridABRManager`)
and the free function `ABRLogger`, both of which write to `printf`, systemd-journal,
or syslog depending on build-time flags (`USE_SYSTEMD_JOURNAL_PRINT` /
`USE_SYSLOG_HELPER_PRINT`).

# Detailed Documentation

For the detailed documentation for each member function, please see

[ABRManager](html/class_a_b_r_manager.html)

[ABRManager::ProfileInfo](html/struct_a_b_r_manager_1_1_profile_info.html)

# Build steps

## Prerequisite

- cmake 3.5+ https://cmake.org/download/
- g++ with C++17 support https://gcc.gnu.org/

## Build

Assume `abr/` is the root directory of this library. The build steps are

```sh
cd abr/
mkdir build
cd build
cmake ..
make
sudo make install
```

Then a shared library `libabr.so` will be generated in `abr/build`, and it will be installed
```
-- Installing: /usr/local/lib/libabr.so
-- Installing: /usr/local/include/ABRManager.h
-- Installing: /usr/local/include/HybridABRManager.h
```

# Sample Usage

```cpp
#include <ABRManager.h>

// Construct the manager object
ABRManager abrManager; 

// Add profiles
abrManager.addProfile({
  .isIframeTrack = ...,
  .bandwidthBitsPerSecond = ...,
  .width = ...,
  .height = ...,
});
...
abrManager.addProfile({
  .isIframeTrack = true,
  .bandwidthBitsPerSecond = 36000,
  .width = 1024,
  .height = 768,
});

// After adding all profiles, update it
abrManager.updateProfile();

// Get the initial profile file index (starts at a reasonable bitrate)
currentProfileIndex = abrManager.getInitialProfileIndex(true);

// Then choose one of the following function to switch the bitrate (get the profile index).
// abrManager.getBestMatchedProfileIndexByBandWidth
// abrManager.getDesiredIframeProfile
// abrManager.getLowestIframeProfile
// abrManager.getRampedDownProfileIndex
// abrManager.getProfileIndexByBitrateRampUpOrDown
```
