/*
 *   Copyright 2018 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
*/

/***************************************************
 * @file ABRManager.cpp
 * @brief This file handles ABR functionalities
 ***************************************************/

#include "ABRManager.h"
#include <cstdio>
#include <cstdarg>
#include <sys/time.h>
#include <cstring>

#if !defined(__APPLE__)
#if defined(USE_SYSTEMD_JOURNAL_PRINT)
#define ENABLE_RDK_LOGGER true
#include <systemd/sd-journal.h>
#elif defined(USE_SYSLOG_HELPER_PRINT)
#define ENABLE_RDK_LOGGER true
#include "syslog_helper_ifc.h"
#endif
#endif
//#define DEBUG_ENABLED 1

#define PROFILE_IDX_RANGE_CHECK(profileIdx, profileCount) \
    do { \
      if (profileIdx >= profileCount) { \
        logprintf("%s:%d Invalid profileIndex %d exceeds the current profile count %d\n", __FUNCTION__, __LINE__, profileIdx, profileCount); \
        profileIdx = profileCount - 1; \
      } \
    } while(0)

#define PROFILES_EMPTY_CHECK_RET(profileCount, retValue) \
    do { \
      if (profileCount == 0) { \
        logprintf("%s:%d No profiles found\n", __FUNCTION__, __LINE__); \
        return retValue; \
      } \
    } while(0)

#define PROFILES_EMPTY_RANGE_CHECK_RET(currentProfileIndex, profileCount, retValue) \
    do { \
      if (profileCount == 0 || currentProfileIndex >= profileCount) { \
        logprintf("%s:%d No profiles/input profile %d more than profileCount %d\n", __FUNCTION__, __LINE__, currentProfileIndex, profileCount); \
        return retValue; \
      } \
    } while(0)

// Loggers
/**
 * @brief Max log buffer size
 */
static const int MAX_LOG_BUFF_SIZE = 1024;

/**
 * @brief Log file directory index - To support dynamic directory configuration for abr logging 
 */
static char gsLogDirectory[] = "c:/tmp/aampabr.log";

/**
 * @brief Module name
 */
static const char *moduleName = "[ABRManager] ";

/**
 * @brief Module name string size
 */
static const int MODULE_NAME_SIZE = 13;

/**
 * @brief Default logger
 * @param fmt The format string
 * @param ... Variadic parameters
 */
/**
 * @brief Initialize the logger to printf
 */
void ABRManager::logprintf(const char* fmt, ...)
{
	char logBuf[MAX_LOG_BUFF_SIZE] = {0};

	strcpy(logBuf, moduleName);
	va_list args;
	va_start(args, fmt);
	vsnprintf(logBuf + MODULE_NAME_SIZE, (MAX_LOG_BUFF_SIZE - 1 - MODULE_NAME_SIZE), fmt, args);
	va_end(args);

#if defined(ENABLE_RDK_LOGGER)
#if defined(USE_SYSTEMD_JOURNAL_PRINT)
	sd_journal_print(LOG_NOTICE, "%s\n", logBuf);
#elif defined(USE_SYSLOG_HELPER_PRINT)
	send_logs_to_syslog(logBuf);
#endif
#else // ENABLE_RDK_LOGGER
	struct timeval t;
	gettimeofday(&t, NULL);
	printf("%ld:%3ld : %s\n", (long int)t.tv_sec, (long int)t.tv_usec / 1000, logBuf);
#endif
}

void ABRLogger(const char* levelstr,const char* file, int line,const char *fmt, ...) {
  int len = 0;
  char logBuf[MAX_LOG_BUFF_SIZE] = {0};

  
  len = snprintf(logBuf, sizeof(logBuf), "[AAMP-ABR][%s][%s][%d]",levelstr,file,line);
  va_list args;
  va_start(args, fmt);
  //t = vsnprintf(logBuf + MODULE_NAME_SIZE, (MAX_LOG_BUFF_SIZE - 1 - MODULE_NAME_SIZE), fmt, args);
  vsnprintf(logBuf+len, MAX_LOG_BUFF_SIZE-len, fmt, args);
  va_end(args);

#if defined(ENABLE_RDK_LOGGER)
#if defined(USE_SYSTEMD_JOURNAL_PRINT)
  sd_journal_print(LOG_NOTICE, "%s\n", logBuf);
#elif defined(USE_SYSLOG_HELPER_PRINT)
  send_logs_to_syslog(logBuf);
#endif
  return ;
#else // ENABLE_RDK_LOGGER
  struct timeval t;
  gettimeofday(&t, NULL);
  printf("%ld:%3ld : %s\n", (long int)t.tv_sec, (long int)t.tv_usec / 1000, logBuf);
#endif
}

long ABRManager::mPersistBandwidth = 0;
long long ABRManager::mPersistBandwidthUpdatedTime = 0;
/**
 * @brief Constructor of ABRManager
 */
ABRManager::ABRManager() : 
  mDefaultInitBitrate(DEFAULT_BITRATE),
  mDesiredIframeProfile(0),
  mAbrProfileChangeUpCount(0),
  mAbrProfileChangeDownCount(0),
  mLowestIframeProfile(INVALID_PROFILE),
  mDefaultIframeBitrate(0),
  mProfileLock() {

}

/**
 * @brief Get initial profile index, choose the medium profile or
 * the profile whose bitrate >= the default bitrate.
 */
int ABRManager::getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  int profileCount = getProfileCountUnlocked();
  int desiredProfileIndex = INVALID_PROFILE;

  PROFILES_EMPTY_CHECK_RET(profileCount, desiredProfileIndex);

  if (chooseMediumProfile && profileCount > 1) {
    // get the mid profile from the sorted list
    SortedBWProfileListIter iter = mSortedBWProfileList[periodId].begin();
    std::advance(iter, static_cast<int>(mSortedBWProfileList[periodId].size() / 2));
    desiredProfileIndex = iter->second;
  } else {
    SortedBWProfileListIter iter;
    desiredProfileIndex = mSortedBWProfileList[periodId].begin()->second;
    for (iter = mSortedBWProfileList[periodId].begin(); iter != mSortedBWProfileList[periodId].end(); ++iter) {
      if (iter->first > mDefaultInitBitrate) {
        break;
      }
      // Choose the profile whose bitrate < default bitrate
      desiredProfileIndex = iter->second;
    }
  }
  if (INVALID_PROFILE == desiredProfileIndex) {
    desiredProfileIndex = mSortedBWProfileList[periodId].begin()->second;
    logprintf("%s:%d Got invalid profile index, choose the first index = %d and profileCount = %d and defaultBitrate = %ld\n",
      __FUNCTION__, __LINE__, desiredProfileIndex, profileCount, mDefaultInitBitrate);
  } else {
    logprintf("%s:%d Get initial profile index = %d, bitrate = %ld and defaultBitrate = %ld\n",
      __FUNCTION__, __LINE__, desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond, mDefaultInitBitrate);
  }
  return desiredProfileIndex;
}

/**
 * @brief Update the lowest / desired profile index
 *    by the profile info. 
 */
void ABRManager::updateProfile() {
  /**
   * @brief A temporary structure of iframe track info
   */
  struct IframeTrackInfo {
    long bandwidth;
    int idx;
  };

  std::unique_lock<std::mutex> lock(mProfileLock);
  int profileCount = getProfileCountUnlocked();
  
  struct IframeTrackInfo *iframeTrackInfo = new struct IframeTrackInfo[profileCount];
  bool is4K = false;

  int iframeTrackIdx = -1;
  // Construct iframe track info
  for (int i = 0; i < profileCount; i++) {
    if (mProfiles[i].isIframeTrack) {
      iframeTrackIdx++;
      iframeTrackInfo[iframeTrackIdx].bandwidth = mProfiles[i].bandwidthBitsPerSecond;
      iframeTrackInfo[iframeTrackIdx].idx = i;
    }
  }
  lock.unlock();

  // Exists iframe track
  if(iframeTrackIdx >= 0) {
    // Sort the iframe track array by bandwidth ascendingly
    for (int i = 0; i < iframeTrackIdx; i++) {
      for (int j = 0; j < iframeTrackIdx - i; j++) {
        if (iframeTrackInfo[j].bandwidth > iframeTrackInfo[j+1].bandwidth) {
          struct IframeTrackInfo temp = iframeTrackInfo[j];
          iframeTrackInfo[j] = iframeTrackInfo[j+1];
          iframeTrackInfo[j+1] = temp;
        }
      }
    }

    // Exist 4K video?
    int highestProfileIdx = iframeTrackInfo[iframeTrackIdx].idx;
    if(mProfiles[highestProfileIdx].height > HEIGHT_4K
      || mProfiles[highestProfileIdx].width > WIDTH_4K) {
      is4K = true;
    }

    if (mDefaultIframeBitrate > 0) {
      mLowestIframeProfile = mDesiredIframeProfile = iframeTrackInfo[0].idx;
      for (int cnt = 0; cnt <= iframeTrackIdx; cnt++) {
        // find the track less than default bw set, apply to both desired and lower ( for all speed of trick)
        if(iframeTrackInfo[cnt].bandwidth >= mDefaultIframeBitrate) {
          break;
        }
        mDesiredIframeProfile = iframeTrackInfo[cnt].idx;
      }
    } else {
      if(is4K) {
        // Get the default profile of 4k video , apply same bandwidth of video to iframe also
        int desiredProfileIndexNonIframe = profileCount / 2;
        int desiredProfileNonIframeBW = (int)mProfiles[desiredProfileIndexNonIframe].bandwidthBitsPerSecond ;
        mDesiredIframeProfile = mLowestIframeProfile = 0;
        for (int cnt = 0; cnt <= iframeTrackIdx; cnt++) {
          // if bandwidth matches , apply to both desired and lower ( for all speed of trick)
          if(iframeTrackInfo[cnt].bandwidth == desiredProfileNonIframeBW) {
            mDesiredIframeProfile = mLowestIframeProfile = iframeTrackInfo[cnt].idx;
            break;
          }
        }
        // if matching bandwidth not found with video , then pick the middle profile for iframe
        if((!mDesiredIframeProfile) && (iframeTrackIdx >= 1)) {
          int desiredTrackIdx = (int) (iframeTrackIdx / 2) + (iframeTrackIdx % 2);
          mDesiredIframeProfile = mLowestIframeProfile = iframeTrackInfo[desiredTrackIdx].idx;
        }
      } else {
        //Keeping old logic for non 4K streams
        for (int cnt = 0; cnt <= iframeTrackIdx; cnt++) {
            if (mLowestIframeProfile == INVALID_PROFILE) {
              // first pick the lowest profile available
              mLowestIframeProfile = mDesiredIframeProfile = iframeTrackInfo[cnt].idx;
              continue;
            }
            // if more profiles available , stored second best to desired profile
            mDesiredIframeProfile = iframeTrackInfo[cnt].idx;
            break; // select first-advertised
        }
      }
    }
  }
  delete[] iframeTrackInfo;

#if defined(DEBUG_ENABLED)
  logprintf("%s:%d Update profile info, mDesiredIframeProfile = %d, mLowestIframeProfile = %d\n",
    __FUNCTION__, __LINE__, mDesiredIframeProfile, mLowestIframeProfile);
#endif
}

/**
 *  @brief According to the given bandwidth, return the best matched
 *  profile index.
 */
int ABRManager::getBestMatchedProfileIndexByBandWidth(int bandwidth) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  // a) Check if network bandwidth changed from starting bandwidth
  // b) Check if netwwork bandwidth is different from persisted bandwidth( needed for first time reporting)
  // find the profile for the newbandwidth
  int desiredProfileIndex = 0;
  int profileCount = getProfileCountUnlocked();
  for (int i = 0; i < profileCount; i++) {
    const ProfileInfo& profile = mProfiles[i];
    if (!profile.isIframeTrack) {
        if (profile.bandwidthBitsPerSecond == bandwidth) {
            // Good case ,most manifest url will have same bandwidth in fragment file with configured profile bandwidth
            desiredProfileIndex = i;
            break;
        } else if (profile.bandwidthBitsPerSecond < bandwidth) {
            // fragment file name bandwidth doesn't match the profile bandwidth, will be always less
            if((i+1) == profileCount) {
                desiredProfileIndex = i;
                break;
            }
            else
                desiredProfileIndex = (i + 1);
        }
    }
  }
#if defined(DEBUG_ENABLED)
  logprintf("%s:%d Get best matched profile index = %d bitrate = %ld\n",
    __FUNCTION__, __LINE__, desiredProfileIndex,
    (profileCount > desiredProfileIndex && desiredProfileIndex != INVALID_PROFILE) ? mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0);
#endif
  return desiredProfileIndex;
}

/**
 *  @brief Ramp down the profile one step to get the profile index of a lower bitrate.
 */
int ABRManager::getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  // Clamp the param to avoid overflow
  int profileCount = getProfileCountUnlocked();
  PROFILE_IDX_RANGE_CHECK(currentProfileIndex, profileCount);
  
  int desiredProfileIndex = currentProfileIndex;
  PROFILES_EMPTY_CHECK_RET(profileCount, desiredProfileIndex);

  long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
  SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
  if (iter == mSortedBWProfileList[periodId].end()) {
    logprintf("%s:%d The current bitrate %ld is not in the profile list\n",
       __FUNCTION__, __LINE__, currentBandwidth);
    return desiredProfileIndex;
  }
  if (iter == mSortedBWProfileList[periodId].begin()) {
    desiredProfileIndex = iter->second;
  } else {
    // get the prev profile . This is sorted list , so no worry of getting wrong profile 
    std::advance(iter, -1);
    desiredProfileIndex = iter->second;
  }

#if defined(DEBUG_ENABLED)
  logprintf("%s:%d Ramped down profile index = %d bitrate = %ld\n",
    __FUNCTION__, __LINE__, desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond);
#endif
  return desiredProfileIndex;
}

/**
 *  @brief Ramp Up the profile one step to get the profile index of a upper bitrate.
 */
int ABRManager::getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  // Clamp the param to avoid overflow
  int profileCount = getProfileCountUnlocked();
  int desiredProfileIndex = currentProfileIndex;
  PROFILES_EMPTY_RANGE_CHECK_RET(currentProfileIndex, profileCount, desiredProfileIndex);

  long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
  SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
  if (iter == mSortedBWProfileList[periodId].end()) {
    logprintf("%s:%d The current bitrate %ld is not in the profile list\n",
       __FUNCTION__, __LINE__, currentBandwidth);
    return desiredProfileIndex;
  }

  if(std::next(iter) != mSortedBWProfileList[periodId].end())
  {
	std::advance(iter, 1);
	desiredProfileIndex = iter->second;
  }

#if defined(DEBUG_ENABLED)
  logprintf("%s:%d Ramped up profile index = %d bitrate = %ld\n",
    __FUNCTION__, __LINE__, desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond);
#endif
  return desiredProfileIndex;
}


/**
 *  @brief Get UserData of profile
 */
int ABRManager::getUserDataOfProfile(int currentProfileIndex) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  int userData = -1;
  int profileCount = getProfileCountUnlocked();

  PROFILES_EMPTY_RANGE_CHECK_RET(currentProfileIndex, profileCount, userData);

  userData = mProfiles[currentProfileIndex].userData;
  return userData;
}


/**
 *  @brief Check if the bitrate of currentProfileIndex reaches to the lowest.
 */
bool ABRManager::isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  // Clamp the param to avoid overflow
  int profileCount = getProfileCountUnlocked();
  PROFILE_IDX_RANGE_CHECK(currentProfileIndex, profileCount);
  
  // If there is no profiles list, then it means `currentProfileIndex` always reaches to
  // the lowest.
  PROFILES_EMPTY_CHECK_RET(profileCount, true);

  long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
  SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
  return iter == mSortedBWProfileList[periodId].begin();
}

/**
 *  @brief Do ABR by ramping bitrate up/down according to the current
 *         network status. Returns the profile index with the bitrate matched with
 *         the current bitrate.
 */
int ABRManager::getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, long currentBandwidth, long networkBandwidth, int nwConsistencyCnt, const std::string& periodId) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  // Clamp the param to avoid overflow
  int profileCount = getProfileCountUnlocked();
  PROFILE_IDX_RANGE_CHECK(currentProfileIndex, profileCount);

  int desiredProfileIndex = currentProfileIndex;
  if (networkBandwidth == -1) {
    // If the network bandwidth is not available, just reset the profile change up/down count.
#if defined(DEBUG_ENABLED)
    logprintf("%s:%d No network bandwidth info available , not changing profile[%d]\n",
      __FUNCTION__, __LINE__, currentProfileIndex);
#endif
    mAbrProfileChangeUpCount = 0;
    mAbrProfileChangeDownCount = 0;
    return desiredProfileIndex;
  }
  if(networkBandwidth > currentBandwidth) {
    // if networkBandwidth > is more than current bandwidth
    SortedBWProfileListIter iter;
    SortedBWProfileListIter currIter = mSortedBWProfileList[periodId].find(currentBandwidth);
    SortedBWProfileListIter storedIter = mSortedBWProfileList[periodId].end();
    for (iter = currIter; iter != mSortedBWProfileList[periodId].end(); ++iter) {
      // This is sort List 
      if (networkBandwidth >= iter->first) {
        desiredProfileIndex = iter->second;
        storedIter = iter;
      } else {
        break;
      }
    }

    // No need to jump one profile for one network bw increase
    if (storedIter != mSortedBWProfileList[periodId].end() && (currIter->first < storedIter->first) && std::distance(currIter, storedIter) == 1) {
      mAbrProfileChangeUpCount++;
      // if same profile holds good for next 3*2 fragments
      if (mAbrProfileChangeUpCount < nwConsistencyCnt) {
        desiredProfileIndex = currentProfileIndex;
      } else {
        mAbrProfileChangeUpCount = 0;
      }
    } else {
      mAbrProfileChangeUpCount = 0;
    }
    mAbrProfileChangeDownCount = 0;
#if defined(DEBUG_ENABLED)
    logprintf("%s:%d Ramp up profile index = %d, bitrate = %ld networkBandwidth = %ld\n",
      __FUNCTION__, __LINE__, desiredProfileIndex,
        (profileCount > desiredProfileIndex && desiredProfileIndex != INVALID_PROFILE) ? mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0, networkBandwidth);
#endif
  } else {
    // if networkBandwidth < than current bandwidth
    SortedBWProfileListRevIter revIter;
    SortedBWProfileListIter currIter = mSortedBWProfileList[periodId].find(currentBandwidth);
    SortedBWProfileListIter storedIter = mSortedBWProfileList[periodId].end();
    for (revIter = mSortedBWProfileList[periodId].rbegin(); revIter != mSortedBWProfileList[periodId].rend(); ++revIter) {
      // This is sorted List
      if (networkBandwidth >= revIter->first) {
        desiredProfileIndex = revIter->second;
        // convert from reverse iter to forward iter
        storedIter = revIter.base();
        storedIter--;
        break;
      }
    }

    // we didn't find a profile which can be supported in this bandwidth
    if (revIter == mSortedBWProfileList[periodId].rend()) {
	desiredProfileIndex = mSortedBWProfileList[periodId].begin()->second;
        logprintf("%s:%d Didn't find a profile which supports bandwidth[%ld], min bandwidth available [%ld]. Set profile to lowest!\n", __FUNCTION__, __LINE__, networkBandwidth, mSortedBWProfileList[periodId].begin()->first);
    }

    // No need to jump one profile for small  network change
    if (storedIter != mSortedBWProfileList[periodId].end() && (currIter->first > storedIter->first) && std::distance(storedIter, currIter) == 1) {
      mAbrProfileChangeDownCount++;
      // if same profile holds good for next 3*2 fragments
      if(mAbrProfileChangeDownCount < nwConsistencyCnt) {
        desiredProfileIndex = currentProfileIndex;
      } else {
        mAbrProfileChangeDownCount = 0;
      }
    } else {
      mAbrProfileChangeDownCount = 0;
    }
    mAbrProfileChangeUpCount = 0;
#if defined(DEBUG_ENABLED)
    logprintf("%s:%d Ramp down profile index = %d, bitrate = %ld networkBandwidth = %ld\n",
      __FUNCTION__, __LINE__, desiredProfileIndex,
      (profileCount > desiredProfileIndex && desiredProfileIndex != INVALID_PROFILE) ? mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0, networkBandwidth);
#endif
  }

  if (currentProfileIndex != desiredProfileIndex) {
    logprintf("%s:%d currBW:%ld NwBW=%ld currProf:%d desiredProf:%d Period ID:%s\n",
      __FUNCTION__, __LINE__, currentBandwidth, networkBandwidth,
      currentProfileIndex, desiredProfileIndex, periodId.c_str());
  }

  return desiredProfileIndex;
}

/**
 *  @brief Get bandwidth of profile
 */
long ABRManager::getBandwidthOfProfile(int profileIndex) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  // Clamp the param to avoid overflow
  int profileCount = getProfileCountUnlocked();
  PROFILES_EMPTY_CHECK_RET(profileCount, 0);
  PROFILE_IDX_RANGE_CHECK(profileIndex, profileCount);

  return mProfiles[profileIndex].bandwidthBitsPerSecond;
}

/**
 *  @brief Get the index of max bandwidth
 */
int ABRManager::getMaxBandwidthProfile(const std::string& periodId) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  int profileCount = getProfileCountUnlocked();
  PROFILES_EMPTY_CHECK_RET(profileCount, 0);

  return mSortedBWProfileList[periodId].size()?mSortedBWProfileList[periodId].rbegin()->second:0;
}

// Getters/Setters
/**
 * @fn getProfileCountUnlocked
 *
 * @return The number of profiles
 */
int ABRManager::getProfileCountUnlocked() const {
  return static_cast<int>(mProfiles.size());
}
/**
 *  @brief Get the number of profiles
 */
int ABRManager::getProfileCount() {
  std::lock_guard<std::mutex> lock(mProfileLock);
  return getProfileCountUnlocked();
}

/**
 *  @brief Set the default init bitrate
 */
void ABRManager::setDefaultInitBitrate(long defaultInitBitrate) {
  mDefaultInitBitrate = defaultInitBitrate;
}


/**
 *  @brief Get the lowest iframe profile index.
 */
int ABRManager::getLowestIframeProfile() const {
  return mLowestIframeProfile;
}

/**
 *  @brief Get the desired iframe profile index.
 */
int ABRManager::getDesiredIframeProfile() const {
  return mDesiredIframeProfile;
}

/**
 *  @brief Add new profile info into the manager
 */
void ABRManager::addProfile(ABRManager::ProfileInfo profile) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  mProfiles.push_back(profile);
  int profileCount = getProfileCountUnlocked();
  int idx = profileCount - 1;
  addSortedBWProfileListUnlocked(mProfiles[idx], idx);
}

/**
 * @brief Add new profile info to sorted BW list
 * @param[in] profileInfo profile info
 * @param[in] idx profile index in list
 */
void ABRManager::addSortedBWProfileListUnlocked(const ABRManager::ProfileInfo &profileInfo, int idx)
{
  if (!profileInfo.isIframeTrack) {
	mSortedBWProfileList[profileInfo.periodId][profileInfo.bandwidthBitsPerSecond] = idx;
#if defined(DEBUG_ENABLED)
	logprintf("%s: Period ID: %s\n", __FUNCTION__, profileInfo.periodId.c_str());
	logprintf("%s: bw:%ld idx:%d\n", __FUNCTION__, profileInfo.bandwidthBitsPerSecond, idx);
#endif
  }

}

/**
* @fn removeProfiles
* @param[in] vector of profile bitrates to remove from ABR data
* @param[in] currentProfileIndex
* @param[in] period Id empty string by default, Period-Id of profiles
* @return modified profileIndex
*/
int ABRManager::removeProfiles(std::vector<long> profileBPS, int currentProfileIndex, const std::string& periodId) {

  std::lock_guard<std::mutex> lock(mProfileLock);
  int modifiedProfileIndex = INVALID_PROFILE;
  int profileCount = getProfileCountUnlocked();

  PROFILES_EMPTY_CHECK_RET(profileCount, modifiedProfileIndex);
  PROFILE_IDX_RANGE_CHECK(currentProfileIndex, profileCount);

  long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
  for (auto &profileBWToRemove : profileBPS)
  {
    for(auto profile = mProfiles.begin(); profile != mProfiles.end();)
    {
      if(profile->periodId == periodId && profile->bandwidthBitsPerSecond == profileBWToRemove) {
        profile = mProfiles.erase(profile);
        // We expect only unique BW entries
        break;
      } else {
        profile++;
      }
    }
  }
#if defined(DEBUG_ENABLED)
  logprintf("%s:%d profileCount after removing profiles orig:%d and new:%d", __FUNCTION__, __LINE__, profileCount, getProfileCountUnlocked());
#endif

  mSortedBWProfileList.clear();
  // Get new profile count
  profileCount = getProfileCountUnlocked();
  for(int idx = 0; idx < profileCount; idx++) {
    addSortedBWProfileListUnlocked(mProfiles[idx], idx);
    if(currentBandwidth == mProfiles[idx].bandwidthBitsPerSecond) {
      modifiedProfileIndex = idx;
    }
  }

  if (modifiedProfileIndex == INVALID_PROFILE) {
    logprintf("%s:%d Unable to find the currentProfileIndex in the modified profiles, currentProfileIndex:%d currBW:%ld period ID:%s\n",
      __FUNCTION__, __LINE__, currentProfileIndex, currentBandwidth, periodId.c_str());
  }
  return modifiedProfileIndex;
}

/**
 *  @brief Clear profiles
 */
void ABRManager::clearProfiles() {

  std::lock_guard<std::mutex> lock(mProfileLock);
  mProfiles.clear();
  if (mSortedBWProfileList.size()) {
    mSortedBWProfileList.erase(mSortedBWProfileList.begin(),mSortedBWProfileList.end());
    mSortedBWProfileList.clear();
  }	
}

/**
 *  @brief Set the simulator log file directory index.
 */
void ABRManager::setLogDirectory(char driveName) {
  gsLogDirectory[0] = driveName;
}

/**
 *  @brief Set the default iframe bitrate
 */
void ABRManager::setDefaultIframeBitrate(long defaultIframeBitrate) {
  mDefaultIframeBitrate = defaultIframeBitrate;
}
