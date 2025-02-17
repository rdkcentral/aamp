/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
* @file MetadataProcessor.h
* @brief Header file for Elementary Fragment Processor
*/

#ifndef __METADATAPROCESSOR_HPP__
#define __METADATAPROCESSOR_HPP__

#include "mediaprocessor.h"

#include "tsDemuxer.hpp"
#include "tsFragmentProcessor.hpp"

#include "priv_aamp.h"
#include <pthread.h>

/// Function to call to update the local PTS record
using ptsoffset_update_t = std::function<void (double, bool)>;

class IsoBmffProcessor;
class TSProcessor;
class CachedFragment;

class Demuxer;
namespace aamp
{

/**
 * Interface for the metadata processor
 * @brief Class for Metadata processing
 */
class MetadataProcessorIntf
{
public:

	/**
	 * @brief Construct a new MetadataProcessor Interface object
	 * 
	 * @param[in] id3_hdl Callback function to generate the event for ID3 metadata
	 * @param[in] ptsoffset_callback Callback function to update the PST offset in the main AAMP instance
	 */
	MetadataProcessorIntf(id3_callback_t id3_hdl, ptsoffset_update_t ptsoffset_callback)
		: mID3Handler{id3_hdl},
		mPtsOffsetUpdate{ptsoffset_callback},
		mBasePTS(0)
	{ }

	/**
	 * MetadataProcessor destructor
	 */
	virtual ~MetadataProcessorIntf() {}

	/// Deleted Copy constructor
	MetadataProcessorIntf(const MetadataProcessorIntf &) = delete;
	/// Deleted Assignment constructor
	MetadataProcessorIntf& operator=(const MetadataProcessorIntf &) = delete;

	/**
	 * @brief Processes the fragment for extracting metadata
	 * 
	 * @param cachedFragment The cached fragment
	 * @param type The fragment's type
	 * @param discontinuity True if it is occurring a discontinuity
	 * @param proc_position The current processing position
	 * @param ptsError Flag to report is there has been an error extracting PTS
	 * @param uri The current fragment's URI - for debug only.
	 */
	virtual void ProcessFragmentMetadata(const CachedFragment * cachedFragment,
		AampMediaType type,
		bool discontinuity_pending, 
		const double proc_position,
		bool & ptsError, 
		const std::string & uri) = 0;

protected:

	id3_callback_t mID3Handler;				/**< Function to use to emit the ID3 event */

	ptsoffset_update_t mPtsOffsetUpdate;	/**< Function to use to update the PTS offset */

	uint64_t mCurrentMaxPTS{0};				/**< Max value of PTS since the last discontinuity */
	double mCurrentMaxPTS_s {0.};			/**< Max value of PTS (converted in seconds) since the last discontinuity */
	
	uint64_t mBasePTS;						/**< Base PTS value*/

};

/**
 * @brief MetadataProcessor implementation
 * 
 * @tparam ProcessorTypeT Video processor type
 */
template <class ProcessorTypeT>
class MetadataProcessorImpl
{
public: 

	/**
	 * @brief Construct a new Metadata Processor Impl object
	 * 
	 * @param processor The video processor to associate to the implementation
	 */
	MetadataProcessorImpl(ProcessorTypeT processor) : mVideoProcessor{std::move(processor)} {}

protected:

	/// Video processor object
	ProcessorTypeT mVideoProcessor;
};

/**
 * @class IsoBMFFMetadataProcessor
 * @brief Class for Metadata processing of IsoBMFF assets
 */
class IsoBMFFMetadataProcessor : public MetadataProcessorIntf, MetadataProcessorImpl<std::weak_ptr<IsoBmffProcessor>>
{
public:

	/**
	 * IsoBMFFMetadataProcessor constructor
	 *
	 * @param[in] id3_hdl Callback function to generate the event for ID3 metadata
	 * @param[in] ptsoffset_callback Callback function to update the PST offset in the main AAMP instance
	 * @param[in] video_processor Video processor object
	 * 
	 * @note The mVideoProcessor is a std::weak_ptr because we use the processor created in the stream abstraction class 
	 * if it's instantiated (and do nothing if not) without keeping a "copy" of the shared pointer
	 */
	IsoBMFFMetadataProcessor(id3_callback_t id3_hdl,
		ptsoffset_update_t ptsoffset_callback,
		std::weak_ptr<IsoBmffProcessor> video_processor
		);

	/**
	 * IsoBMFFMetadataProcessor destructor
	 */
	~IsoBMFFMetadataProcessor(){};

	/// Deleted Copy constructor
	IsoBMFFMetadataProcessor(const IsoBMFFMetadataProcessor &) = delete;

	/// Deleted Assignment constructor
	IsoBMFFMetadataProcessor& operator=(const IsoBMFFMetadataProcessor &) = delete;

	virtual void ProcessFragmentMetadata(const CachedFragment * cachedFragment,
		AampMediaType type,
		bool discontinuity_pending, 
		const double proc_position,
		bool & ptsError, 
		const std::string & uri) override;

private:

	/// @brief Tries to get from the video processor the PTS at tune time
	/// @return True if the PTS at tune time is valid 
	bool SetTuneTimePTS();

	/**
	 * @brief Processes the IsoBMFF stream to extract ID3 metadata
	 * 
	 * @param type AampMediaType
	 * @param data_ptr Pointer to the segment's data
	 * @param data_len Length of the segment
	 */
	void ProcessID3Metadata(AampMediaType type, const char * data_ptr, size_t data_len);

    /// Flag for tracking whether the current PTS is valid (Initialized) or not
    bool processPTSComplete;
	
};

/**
 * @class TSMetadataProcessor
 * @brief Class for Metadata processing of IsoBMFF assets
 */
class TSMetadataProcessor : public MetadataProcessorIntf, MetadataProcessorImpl<std::shared_ptr<TSProcessor>>
{
public:
	/**
	 * TSMetadataProcessor constructor
	 *
	 * @param[in] id3_hdl Callback function to generate the event for ID3 metadata
	 * @param[in] ptsoffset_callback Callback function to update the PST offset in the main AAMP instance
	 * @param[in] video_processor Video processor object
	 * 
	 * @note The mVideoProcessor is a std::shared_ptr because to HLS/TS we want to use our own instance of the TSProcessor
	 * and in this way we can create it outside this class (i.e. without having to pass all the construction parameters), before 
	 * moving it onto the local stored object.
	 */
	TSMetadataProcessor(id3_callback_t id3_hdl,
		ptsoffset_update_t ptsoffset_callback,
		std::shared_ptr<TSProcessor> video_processor
		);

	/**
	 * TSMetadataProcessor destructor
	 */
	~TSMetadataProcessor(){};

	/// Deleted Copy constructor
	TSMetadataProcessor(const TSMetadataProcessor &) = delete;
	/// Deleted Assignment constructor
	TSMetadataProcessor& operator=(const TSMetadataProcessor &) = delete;

	virtual void ProcessFragmentMetadata(const CachedFragment * cachedFragment,
		AampMediaType type,
		bool discontinuity_pending, 
		const double proc_position,
		bool & ptsError, 
		const std::string & uri) override;

private:

	// std::unique_ptr<Demuxer> mDsmccDemuxer {nullptr};

	std::unique_ptr<aamp_ts::TSFragmentProcessor> mProcessor {nullptr};

};


}

#endif /* __METADATAPROCESSOR_HPP__ */

