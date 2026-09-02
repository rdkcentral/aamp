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

#ifndef __TSDEMUXER_HPP__
#define __TSDEMUXER_HPP__


#include "AampMediaType.h"
#include "uint33_t.h"

#include <cstdint>
#include <vector>

#include "AampSegmentInfo.hpp"
#include "mediaprocessor.h"

#include "inttypes.h"

#include <mutex>


namespace aamp_ts
{

	constexpr size_t ts_packet_size = 188u;
	constexpr size_t max_ts_packet_size = 208u;

	constexpr size_t pes_header_size = 6u;
	constexpr size_t pes_min_data = pes_header_size + 3u;

	constexpr size_t pmt_section_max_size = 1021u;
	constexpr size_t patpmt_max_size = 2048u;
	constexpr size_t pat_spts_size = 13u;
	constexpr size_t pat_table_entry_size = 4u;

}

class PrivateInstanceAAMP;

/**
 * @class Demuxer
 * @brief Software demuxer of MPEGTS
 */
class Demuxer
{
private:
	double ptsOffset = 0.0;
	PrivateInstanceAAMP *aamp = nullptr;
	int pes_state = 0;
	int pes_header_ext_len = 0;
	int pes_header_ext_read = 0;
	std::vector<uint8_t> pes_header{};

	/* All public methods should be locked using this mutex as
	 * member data is highly coupled (especially in processdata()).
	 * Concurrent access to member data is highly likely to corrupt or return corrupt data.
	 * Testing shows that 4 threads can access one instance of this class.
	 * setBasePTS(), getBasePTS() & HasCachedData() methods imply
	 * that there are pre-existing interface races that this change does not address*/
	std::mutex mMutex{};
	std::vector<uint8_t> es{};
	double position = 0.0;
	double duration = 0.0;
	/* The first access unit of an epoch is buffered here until the next
	 * access unit arrives, so its true duration can be measured from the
	 * DTS delta before it is emitted. */
	std::vector<uint8_t> pending_es{};
	SegmentInfo_t pending_info{};
	double total_sample_duration = 0.0;
	uint33_t base_pts{};
	bool rollover_pts = false;
	uint33_t current_pts{};
	uint33_t current_dts{};
	uint33_t first_pts{};
	bool update_first_pts = false;
	AampMediaType type = eMEDIATYPE_DEFAULT;
	bool trickmode = false;
	bool finalized_base_pts = false;
	bool allowPtsRewind = false;
	bool reached_steady_state = false;
	/* When true, skip the next rollover detection (used after discontinuity/ptsOffset changes)
	 * to avoid false-positive rollover detection caused by the large PTS jump across encoder epochs.
	 */
	bool suppress_rollover_detection = false;


	/**
	 * Checks whether the steady state has been reached
	 * @returns True if the steady state has been reached
	*/
	bool CheckForSteadyState();

	/**
	 * @brief Updates internal PTS, DTS and duration and fills a @a SegmentInfo_t with the updated values
	 * @return SegmentInfo_t containing current's segment PTS, DTS and duration
	 */
	SegmentInfo_t UpdateSegmentInfo() const;

	/**
	 * @brief Emits a single access unit via the processor or SendStreamCopy.
	 * @param[in] info Timing information for the sample.
	 * @param[in,out] payload Elementary stream bytes (moved when a processor
	 *        is supplied); cleared on return.
	 * @param[in] processor Optional processor.
	 */
	void emitSample(const SegmentInfo_t &info, std::vector<uint8_t> &payload, const MediaProcessor::process_fcn_t &processor);

	/**
	 * @brief Emits the buffered sample, when other samples from segment have been processed.
	 * @param[in] processor Optional processor.
	 */
	void emitLastSample(const MediaProcessor::process_fcn_t &processor);

	/**
	 * @brief reset demux state
	 */
	void resetInternal();

	/**
	 * @brief Sends elementary stream with proper PTS
	 * @param[in] processor Function to process the demuxed segment
	 */
	void sendInternal(MediaProcessor::process_fcn_t processor);

public:
	void setPtsOffset( double offs )
	{ // used to optimize hls/ts discontinuity handling
		std::lock_guard<std::mutex> lock{mMutex};
		ptsOffset = offs;
		// A new encoder epoch begins at each discontinuity. Clear any
		// stale rollover flag from the previous period so that the first
		// frames of the incoming segment (which may start near PTS 0)
		// are not incorrectly bumped by one full 33-bit cycle.
		rollover_pts = false;
		// Suppress rollover detection for the next PTS update to avoid
		// falsely detecting a 33-bit wrap when there is a large PTS jump
		// caused by the discontinuity / restamp boundary.
		suppress_rollover_detection = true;
		// Flush any sample still held for look-ahead from the previous epoch
		// so it is not measured against the new (discontinuous) timeline.
		emitLastSample(nullptr);
	}

	/**
	 * @brief Demuxer Constructor
	 * @param[in] aamp pointer to PrivateInstanceAAMP object associated with demux
	 * @param[in] type Media type to be demuxed
	 */
	Demuxer(class PrivateInstanceAAMP *aamp, AampMediaType type, bool optimizeMuxed )
	 : aamp(aamp), type(type)
	{
		//mutex in init
		init(0, 0, false, true, optimizeMuxed );
	}

	/**
	 * @brief Copy Constructor: deleted
	 */
	Demuxer(const Demuxer&) = delete;

	/**
	 * @brief Move Constructor: deleted
	 */
	Demuxer(Demuxer &&) noexcept = delete;

	/**
	 * @brief Copy assignment operator overloading: deleted
	 */
	Demuxer& operator=(const Demuxer&) = delete;

	/**
	 * @brief Move assignment operator overloading: deleted
	 */
	Demuxer& operator=(Demuxer &&) noexcept = delete;

	/**
	 * @brief Demuxer Destructor
	 *
	 * Acquires mMutex to serialize destruction with any concurrent
	 * accesses from public methods that also lock mMutex.
	 */
	~Demuxer()
	{
		std::lock_guard<std::mutex> lock(mMutex);
	}

	/**
	 * @brief Initialize demux
	 * @param[in] position start position
	 * @param[in] duration duration
	 * @param[in] trickmode true if trickmode
	 * @param[in] resetBasePTS true to reset base pts used for restamping
	 */
	void init(double position, double duration, bool trickmode, bool resetBasePTS, bool optimizeMuxed );

	/**
	 * @brief flush es buffer and reset demux state
	 */
	void flush();

	/**
	 * @brief reset demux state
	 */
	void reset();

	/**
	 * @brief Set base PTS used for re-stamping
	 * @param[in] basePTS new base PTS
	 * @param[in] final true if base PTS is finalized
	 */
	void setBasePTS(unsigned long long basePTS, bool isFinal);

	/**
	 * @brief Get base PTS used for re-stamping
	 * @retval base PTS used for re-stamping
	 */
	unsigned long long getBasePTS();

	/**
	 * @brief Process a TS packet
	 * @param[in] packetStart start of buffer containing packet
	 * @param[out] basePtsUpdated true if base PTS is updated
	 * @param[in] ptsError true if encountered PTS error.
	 */
	void processPacket(const unsigned char * packetStart, bool &basePtsUpdated, bool &ptsError, bool &isPacketIgnored, bool applyOffset, MediaProcessor::process_fcn_t processor);

	/**
	 * @brief
	 *
	 * @param processor
	 */
	void send(MediaProcessor::process_fcn_t processor)
	{
		std::lock_guard<std::mutex> lock{mMutex};
		sendInternal(processor);
	}

	/** @brief Provides the @a AampMediaType of the demuxer
	 * @return The AampMediaType of the demuxer
	 */
	AampMediaType GetType()
	{
		std::lock_guard<std::mutex> lock{mMutex};
		return type;
	}

	/**
	 * @brief Consumes the cached data of the @a es buffer, if present
	 * @note Note that the @a es buffer is "cleared" inside the @a send function
	 * @return True if data was present
	 * @return False if there was no data
	 */
	bool ConsumeCachedData(MediaProcessor::process_fcn_t processor)
	{
		std::lock_guard<std::mutex> lock{mMutex};

		bool sent = false;
		if (!es.empty())
		{
			sendInternal(processor);
			sent = true;
		}
		if (!pending_es.empty())
		{
			emitLastSample(processor);
			sent = true;
		}
		return sent;
	}

	/**
	 * @brief Checks if there is any cached data in the @a es buffer
	 * @return True if there is any cached data
	 * @return False if there is no cached data
	*/
	bool HasCachedData()
	{

		std::lock_guard<std::mutex> lock{mMutex};
		return !es.empty() || !pending_es.empty();
	}

	/**
	 * @brief Provides the current size of the @a es buffer
	 * @return The size of data contained in the @a es buffer
	*/
	size_t GetCachedDataSize()
	{

		std::lock_guard<std::mutex> lock{mMutex};
		return es.size() + pending_es.size();
	}

};

#endif	/* __TSDEMUXER_HPP__ */
