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
* @file tsFragmentProcessor.hpp
* @brief Header file for TS Fragment Processor
*/

#ifndef __TSSEGMENTPROCESSOR_HPP__
#define __TSSEGMENTPROCESSOR_HPP__


#include "AampLogManager.h"
#include "tsprocessor.h"
#include "tsDemuxer.hpp"



#include <array>

#include <cstdint>
#include <cstddef>


class CachedFragment;
class AampGrowableBuffer;

namespace aamp_ts {

constexpr size_t max_pmt_pid_count = 8u;

/**
 * @brief Class to process the fragment and process the demuxed ESs
 */
class TSFragmentProcessor
{
public:

	/**
	 * @brief Construct a new TSFragmentProcessor object
	 */
	TSFragmentProcessor();

	/// @brief Destroy the TSFragmentProcessor object
	virtual ~TSFragmentProcessor();

	/**
	 * @brief Processes the fragment
	 * 
	 * @param fragment The fragment to process
	 * @param position Current fragment position
	 * @param duration Current fragment duration
	 * @param discontinuity_pending True if there is a pending discontinuity 
	 * @param processor Function to process the demuxed ESs
	 * @return true If the processing has been completed
	 * @return false If the processing has not been completed
	 */
	bool ProcessFragment(const AampGrowableBuffer & fragment,
		double position, double duration, bool discontinuity_pending,
		MediaProcessor::process_fcn_t processor);

	/// @brief 
	void Reset();

	/**
	 * @brief Converts the binary data into the timestamp (PTS)
	 * 
	 * @param[in] p The binary data containing the PTS
	 * @param[out] TS The resulting PTS
	 * @return true If the conversion is successful
	 * @return false Id the conversion failed
	 */
	static bool ReadTimeStamp(const unsigned char *p, long long& TS);

private:
	/// @brief Resets the audio component and the associated variables
	void ResetAudioComponents();

	/// @brief Resets the video component and the associated variables
	void ResetVideoComponents();

	/**
	 * @brief Parses the fragment
	 * 
	 * @param base_frag_ptr Pointer to the start of the fragment data to parse
	 * @param curr_packet_ptr Pointer to the current data to be parsed (due to the fragment's validation it can be different from the start of the fragment data)
	 * @param curr_len Size of the data to parse 
	 * @param discontinuity_pending True if there is a pending discontinuity
	 */
	void ParseFragment(const uint8_t * base_frag_ptr, uint8_t * curr_packet_ptr, size_t curr_len, bool discontinuity_pending);

	/**
	 * @brief Validates the current fragment
	 * 
	 * @param fragment The fragment object to validate
	 * @param curr_packet_ptr The pointer to the start of the data to be processed
	 * @param curr_len Length of the data to be processed
	 * @return true If the fragment is valid
	 * @return false If the fragment is NOT valid
	 */
	bool ValidateFragment(const AampGrowableBuffer & fragment, uint8_t * & curr_packet_ptr, size_t & curr_len) const;

	/**
	 * @brief Demuxes the fragment
	 * 
	 * @param packet_ptr Pointer to the packet's data
	 * @param curr_len Size of the data to be demuxed
	 * @param discontinuity_pending True if there is a discontinuity pending
	 * @param processor Processing function to pass to the demuxer to process the demuxed ES
	 */
	void DemuxFragment(const uint8_t * packet_ptr, size_t curr_len, bool discontinuity_pending,
		MediaProcessor::process_fcn_t processor);

	/**
	 * @brief Processes the PMT section to extract the PIDs of the different components
	 * 
	 * @param section Pointer to the section data
	 * @param sectionLength Size of the section data to parse
	 */
	void ProcessPMTSection(uint8_t * section, size_t sectionLength);


	bool m_havePAT {false};					//!< PAT detected
	uint16_t m_versionPAT {0};		 		//!< Pat Version number
	uint16_t m_program {0};					//!< Program version ID

	bool m_havePMT {false};					//!< PMT detected
	uint16_t m_pmtPid {0};					//!< PID of PMT
	uint16_t m_versionPMT {0}; 				//!< Version number for PMT which is being examined


	int m_pcrPid;							//!< PID of PCR component
	bool mIndexedAudio{false};				//!< Audio components are indexed
	uint16_t m_ActiveAudioTrackIndex{0};	//!< Currently active audio index (PID) to use for demuxing

	uint33_t m_currentPTS {0}; 				//!< Store the current PTS value of a recording
	uint33_t m_actualStartPTS{0};			
	uint64_t m_packetStartAfterFirstPTS{0};


	// ?? vector<uint8_t> ?? 
	unsigned char *m_pmtCollector; 		//!< A buffer pointer to hold PMT data at the time of examining TS buffer
	int m_pmtCollectorNextContinuity;	//!< Keeps next continuity counter for PMT packet at the time of examine the TS Buffer
	int m_pmtCollectorSectionLength; 	//!< Update section length while examining PMT table
	int m_pmtCollectorOffset; 			//!< If it is set, process subsequent parts of multi-packet PMT

	uint16_t m_videoComponentCount; 	//!< Number of video components found
	std::array<RecordingComponent, max_pmt_pid_count> m_videoComponents;	//!< Collection of video components
	uint16_t m_audioComponentCount;		//!< Number of audio components found
	std::array<RecordingComponent, max_pmt_pid_count> m_audioComponents;	//!< Collection of audio components

	bool m_dsmccComponentFound; 		//!< True if DSMCC found
	RecordingComponent m_dsmccComponent; //!< Digital storage media command and control (DSM-CC) Component

	bool m_demuxInitialized {false};	//!< True if the demuxers have been Initialized
	std::unique_ptr<Demuxer> mVideoDemuxer {nullptr};	//!< Video demuxer object
	std::unique_ptr<Demuxer> mAudioDemuxer {nullptr};	//!< Audio demuxer object
	std::unique_ptr<Demuxer> mDsmccDemuxer {nullptr};	//!< DSMCC demuxer object


};
	
}

#endif  /* __TSSEGMENTPROCESSOR_HPP__ */
