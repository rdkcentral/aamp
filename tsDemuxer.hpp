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


#include "AampGrowableBuffer.h"
#include "AampMediaType.h"
#include "uint33_t.h"

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
	double ptsOffset;
	PrivateInstanceAAMP *aamp;
	int pes_state;
	int pes_header_ext_len;
	int pes_header_ext_read;
	AampGrowableBuffer pes_header;
	
	/* All public methods should be locked using this mutex as
	 * member data is highly coupled (especially in processdata()).
	 * Concurrent access to member data is highly likely to corrupt or return corrupt data.
	 * Testing shows that 4 threads can access one instance of this class.
	 * setBasePTS(), getBasePTS() & HasCachedData() methods imply
	 * that there are pre-existing interface races that this change does not address*/
	std::mutex mMutex;
	AampGrowableBuffer es;
	double position;
	double duration;
	uint33_t base_pts;
	uint33_t current_pts;
	uint33_t current_dts;
	uint33_t first_pts;
	bool update_first_pts;
	AampMediaType type;
	bool trickmode;
	bool finalized_base_pts;
	bool allowPtsRewind;
	bool reached_steady_state;

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
	 * @brief Sends elementary stream with proper PTS
	 */
	void send();

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
		ptsOffset = offs;
	}
	
	/**
	 * @brief Demuxer Constructor
	 * @param[in] aamp pointer to PrivateInstanceAAMP object associated with demux
	 * @param[in] type Media type to be demuxed
	 */
	Demuxer(class PrivateInstanceAAMP *aamp, AampMediaType type, bool optimizeMuxed )
	 : aamp(aamp), pes_state(0),
		pes_header_ext_len(0), pes_header_ext_read(0), pes_header("pes_header"), mMutex(),
		es("es"), position(0), duration(0), base_pts{0}, current_pts{0},
		current_dts{0}, type(type), trickmode(false), finalized_base_pts(false),
		allowPtsRewind(false), first_pts{0}, update_first_pts(false), reached_steady_state(false), ptsOffset(0.0)
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
	 */
	~Demuxer()
	{
		std::lock_guard<std::mutex> lock{mMutex};
		es.Free();
		pes_header.Free();
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

		if (es.GetLen())
		{
			sendInternal(processor);
			return true;
		}
		return false;
	}

	/**
	 * @brief Checks if there is any cached data in the @a es buffer
	 * @return True if there is any cached data
	 * @return False if there is no cached data
	*/
	bool HasCachedData()
	{

		std::lock_guard<std::mutex> lock{mMutex};
		return !!es.GetLen();
	}

	/**
	 * @brief Provides the current size of the @a es buffer
	 * @return The size of data contained in the @a es buffer
	*/
	size_t GetCachedDataSize()
	{

		std::lock_guard<std::mutex> lock{mMutex};
		return es.GetLen();
	}

};

#endif	/* __TSDEMUXER_HPP__ */
