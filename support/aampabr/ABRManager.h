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
 * @file ABRManager.h
 * @brief Handles operations on ABR functionalities
 ***************************************************/

#ifndef ABR_MANAGER_H
#define ABR_MANAGER_H

#include <vector>
#include <map>
#include <string>
#include <cstdio>
#include <mutex>

typedef long BitsPerSecond;
#define BITSPERSECOND_FORMAT "ld"

/**
 * @class ABRManager
 * @brief ABR Manager for HLS/DASH
 * 
 * It returns the profile index with the desired bitrate
 * base on the network condition.
 */
class ABRManager {
public:
  /**
   * @brief The profile info used
   * for ramping up/down bitrate.
   */
  struct ProfileInfo {
    /**
     * @brief Is iframe track
     */
    bool isIframeTrack;

    /**
     * @brief Bandwidth / second (Bitrate)
     */
    long bandwidthBitsPerSecond;

    /**
     * @brief Width of resolution (optional)
     */
    int width;

    /**
     * @brief Height of resolution (optional)
     */
    int height;

    /**
     * @brief period-Id of profiles (optional)
     */
    std::string periodId;

    /**
     * @brief profileIndex or PeriodIndex (optional)
     */
    int userData;
  };

  /**
   * @brief Persist Network Bandwidth 
   */
  static long mPersistBandwidth;

  /**
   * @brief Persist Network Bandwidth Updated Time
   */

  static long long mPersistBandwidthUpdatedTime;
public:
  /**
   * @fn ABRManager
   */
  ABRManager();

  /**
   * @fn getInitialProfileIndex
   * 
   * @param chooseMediumProfile Boolean flag, true means
   * to choose the medium profile, otherwise to choose the profile whose
   * bitrate >= the default bitrate.
   * @param periodId empty string by default, Period-Id of the profiles
   * added to ABR map
   * 
   * @return The initial profile index 
   */
  int getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId= std::string());

  /**
   * @fn updateProfile
   * @return void
   */
  void updateProfile();

  /**
   * @fn getBestMatchedProfileIndexByBandWidth
   * @param bandWidth  The given bandwidth
   * @return the best matched profile index
   */
  int getBestMatchedProfileIndexByBandWidth(int bandwidth);

  /**
   * @fn getRampedDownProfileIndex
   *
   * @param currentProfileIndex The current profile index
   * @param periodId empty string by default, Period-Id of profiles
   *
   * @return the profile index of a lower bitrate (one step)
   */
  int getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId= std::string());

  /**
   * @fn getRampedUpProfileIndex
   *
   * @param currentProfileIndex The current profile index
   * @param periodId empty string by default, Period-Id of profiles
   *
   * @return the profile index of a upper bitrate (one step)
   */
  int getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId= std::string());

  /**
   * @fn isProfileIndexBitrateLowest
   *
   * @param currentProfileIndex The current profile index
   * @param periodId empty string by default, Period-Id of profiles
   *
   * @return True means it reaches to the lowest, otherwise, it doesn't.
   */
  bool isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId= std::string());

  /**
   * @fn getProfileIndexByBitrateRampUpOrDown
   * 
   * @param currentProfileIndex The current profile index
   * @param currentBandwidth The current band width
   * @param networkBandwidth The current available bandwidth (network bandwidth)
   * @param nwConsistencyCnt Network consistency count, used for bitrate ramping up/down
   * @param periodId empty string by default, Period-Id of profiles
   * @return int Profile index
   */
  int getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, long currentBandwidth, long networkBandwidth, int nwConsistencyCnt = DEFAULT_ABR_NW_CONSISTENCY_COUNT, const std::string& periodId= std::string());

  /**
   * @fn getBandwidthOfProfile
   *
   * @param profileIndex The profile index
   * @return long bandwidth of the profile
   */
  long getBandwidthOfProfile(int profileIndex);

  /**
   * @brief Get profile of bandwidth
   * 
   * @param bandwidth The bandwidth
   * @return int index of the bandwidth
   */
  int getProfileOfBandwidth(long bandwidth);

  /**
   * @fn getMaxBandwidthProfile
   * 
   * @param periodId empty string by default, Period-Id of profiles
   *
   * @return int index of the max bandwidth
   */
  int getMaxBandwidthProfile(const std::string& periodId = std::string());
public:
  // Getters/Setters
  /**
   * @fn getProfileCount
   * 
   * @return The number of profiles 
   */
  int getProfileCount();

  /**
   * @fn setDefaultInitBitrate
   * 
   * @param defaultInitBitrate Default init bitrate
   */
  void setDefaultInitBitrate(long defaultInitBitrate);

  /**
   * @fn getLowestIframeProfile
   * 
   * @return the lowest iframe profile index. 
   */
  int getLowestIframeProfile() const;

  /**
   * @fn getDesiredIframeProfile
   * 
   * @return the desired iframe profile index. 
   */
  int getDesiredIframeProfile() const;

  /**
   * @fn addProfile
   * @param profile The profile info
   */
  void addProfile(ProfileInfo profile);

  /**
   * @fn clearProfiles
   * @return void
   */
  void clearProfiles();

  /**
   * @fn removeProfiles
   * @param[in] vector of profile bitrates to remove from ABR data
   * @param[in] currentProfileIndex
   * @param[in] period Id empty string by default, Period-Id of profiles
   * @return modified ProfileIndex
   */
  int removeProfiles(std::vector<long> profileBPS, int currentProfileIndex, const std::string& periodId = std::string());

  /**
   * @fn setLogDirectory
   */
  void setLogDirectory(char driveName);

  /**
   * @fn setDefaultIframeBitrate
   * 
   * @param defaultIframeBitrate Default iframe bitrate
   */
  void setDefaultIframeBitrate(long defaultIframeBitrate);
   /**
    * @fn getUserDataOfProfile
    *
    * @param profileIndex The profile index
    * @return int userdata / period index
    */
   int getUserDataOfProfile(int profileIndex);
   /**
    * @brief Set the Persist Network Bandwidth
    *
    * @param network bitrate
    */
   static void setPersistBandwidth(long bitrate){mPersistBandwidth = bitrate;}
   /**
    * @brief Get Persisted Network Bandwidth
    *
    * @return  bandwidth
    */
   static long getPersistBandwidth() { return mPersistBandwidth;}
   
   /**
    * @brief Get the available profiles
    */
   std::vector<ProfileInfo> getProfileInfo() { return mProfiles;}

protected:
	/**
	 * @brief Logger
	 */
	static void logprintf( const char *fmt, ... ) __attribute__ ((format (printf, 1, 2)));

private:
  /**
   * @fn getProfileCountUnlocked
   *
   * @return The number of profiles
   */
  int getProfileCountUnlocked() const;

  /**
   * @brief Add new profile info to sorted BW list
   * @param[in] profileInfo profile info
   * @param[in] idx profile index in list
   */
  void addSortedBWProfileListUnlocked(const ABRManager::ProfileInfo &profileInfo, int idx);

  /**
   * @brief A list of available profiles.
   */
  std::vector<ProfileInfo> mProfiles;

  /**
   * @brief A sorted list of profiles with periodId.
   * Populate the container with sorted order of BW (Bandwidth) vs its index under each periodId
   */
  std::map<std::string, std::map<long,int>> mSortedBWProfileList;

  /**
   * @brief Define type: iterator of SortedBWProfileListIter 
   */
  typedef std::map<long, int>::iterator SortedBWProfileListIter;

  /**
   * @brief Define type: reverse iterator of SortedBWProfileListIter 
   */
  typedef std::map<long, int>::reverse_iterator SortedBWProfileListRevIter;

  /**
   * @brief Lowest iframe Profile index
   */ 
  int mLowestIframeProfile;

  /**
   * @brief Desired iframe Profile index
   */
  int mDesiredIframeProfile;

  /**
   * @brief Default initialization bitrate
   */
  long mDefaultInitBitrate;


  /**
   * @brief The number of ABR profiles that ramping up
   */
  int mAbrProfileChangeUpCount;
  
  /**
   * @brief The number of ABR profiles that ramping down
   */
  int mAbrProfileChangeDownCount;

  /**
   * @brief Default iframe bitrate
   */
  long mDefaultIframeBitrate;

public:
  /**
   * @brief Invalid profile index
   */
  static const int INVALID_PROFILE = -1;

private:
  /**
   * @brief Default init bitrate value.
   */
  static const int DEFAULT_BITRATE = 1000000;

  /**
   * @brief The width of 4K video
   */
  static const int WIDTH_4K = 1920;

  /**
   * @brief The height of 4K video
   */
  static const int HEIGHT_4K = 1080;

  /**
   * @brief The default value of the network consistency count.
   * If the profile change exceeds this value, the ABR will be performed.
   * Used when bitrate ramping up/down
   */
  static const int DEFAULT_ABR_NW_CONSISTENCY_COUNT = 2;

  /**
   * @brief Mutex for make internal structures thread safe
   */
  std::mutex mProfileLock;
};
extern void ABRLogger(const char* levelstr,const char* file, int line,const char* fmt, ...)  __attribute__ ((format (printf, 4, 5)));;
#endif
