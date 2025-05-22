/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include "tsDemuxer.hpp"

#include "priv_aamp.h"
#include "AampLogManager.h"

// TS Demuxing defines

#define PES_STATE_WAITING_FOR_HEADER  0
#define PES_STATE_GETTING_HEADER  1
#define PES_STATE_GETTING_HEADER_EXTENSION  2
#define PES_STATE_GETTING_ES  3

#define PAYLOAD_UNIT_START(packetStart) ( packetStart[1] & 0x40)
#define CONTAINS_PAYLOAD(packetStart) ( packetStart[3] & 0x10)
#define IS_PES_PACKET_START(a) ( (a[0] == 0 )&& (a[1] == 0 ) &&(a[2] == 1 ))
#define PES_OPTIONAL_HEADER_PRESENT(pesStart) ( ( pesStart[6] & 0xC0) == 0x80 )
#define PES_OPTIONAL_HEADER_LENGTH(pesStart) (pesStart[aamp_ts::pes_header_size+2])
#define PES_PAYLOAD_LENGTH(pesStart) (pesStart[4]<<8|pesStart[5])
#define ADAPTATION_FIELD_PRESENT(buf) ((buf[3] & 0x20) == 0x20)

// #define PES_HEADER_LENGTH 6
// #define PES_MIN_DATA (PES_HEADER_LENGTH+3)

#define MAX_FIRST_PTS_OFFSET (uint33_t{45000}) /*500 ms*/

/** Maximum PTS value */
// #define MAX_PTS (uint33_t::max_value().value)
constexpr uint64_t max_pts_value = uint33_t::max_value().value;

/**
 * @brief std::exchange for pre-c++14 compiler
 * @param obj	-	object whose value to replace
 * @param new_value	-	the value to assign to obj
 * @return The old value of obj
 */
template<class T, class U = T>
T exchange(T& obj, U&& new_value)
{
	T old_value = std::move(obj);
	obj = std::forward<U>(new_value);
	return old_value;
}


namespace {

	static unsigned long long mTimeAdjust;
	static bool mbInduceRollover;

	/**
	 * @brief extract 33 bit timestamp from ts packet header
	 * @param ptr pointer to first of five bytes encoding a 33 bit PTS timestamp
	 * @return 33 bit unsigned integer (which fits in long long)
	 */
	uint33_t Extract33BitTimestamp( const unsigned char *ptr )
	{
		unsigned long long v; // this is to hold a 64bit integer, lowest 36 bits contain a timestamp with markers
		v = (unsigned long long) (ptr[0] & 0x0F) << 32
			| (unsigned long long) ptr[1] << 24 | (unsigned long long) ptr[2] << 16
			| (unsigned long long) ptr[3] << 8 | (unsigned long long) ptr[4];
		unsigned long long timeStamp = 0;
		timeStamp |= (v >> 3) & (0x0007ULL << 30); // top 3 bits, shifted left by 3, other bits zero
		timeStamp |= (v >> 2) & (0x7fff << 15); // middle 15 bits
		timeStamp |= (v >> 1) & (0x7fff << 0); // bottom 15 bits

		if( mbInduceRollover )
		{
			mTimeAdjust = (max_pts_value-90000*10) - timeStamp;
			mbInduceRollover = false;
		}
		timeStamp += mTimeAdjust;
		return {timeStamp};
	}

}

void tsdemuxer_InduceRollover( bool enable )
{ // for use by aampcli.exe - allows induced PTS rollover to be triggered or (re)disabled
	mTimeAdjust = 0;
	mbInduceRollover = enable;
}

// using namespace aamp_ts;

bool Demuxer::CheckForSteadyState()
{
	if (!reached_steady_state)
	{
		if( aamp && ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) )
		{ // skip below sanity checks if restamping from 0
			reached_steady_state = true;
			return true;
		}
		if ((base_pts > current_pts)
			|| (current_dts && base_pts > current_dts))
		{
			AAMPLOG_WARN("Discard ES Type %d position %f base_pts %" PRIu64 " current_pts %" PRIu64 " diff %f seconds length %d",
				type, position, base_pts.value, current_pts.value, (double)(base_pts - current_pts) / 90000, (int)es.GetLen() );
			es.Clear();
			return false;
		}

		if (base_pts + uint33_t::half_max() > current_pts + uint33_t::half_max())
		{
			AAMPLOG_WARN("Discard ES Type %d position %f base_pts %" PRIu64 " current_pts %" PRIu64 " base_pts+half_max %" PRIu64 " current_pts+half_max %" PRIu64 ,
				type, position, base_pts.value, current_pts.value, (base_pts+uint33_t::half_max()).value, (current_pts+uint33_t::half_max()).value);
			es.Clear();
			return false;
		}
		reached_steady_state = true;
		return true;
	}
	return true;
}

SegmentInfo_t Demuxer::UpdateSegmentInfo() const
{
	SegmentInfo_t ret {position, 0, duration};
	if (!trickmode)
	{
		ret.pts_s += static_cast<double>(current_pts.value - base_pts.value) / 90000.;
	}
	if (!trickmode)
	{
		ret.dts_s = position + static_cast<double>(current_dts.value - base_pts.value) / 90000.;
	}
	if( aamp && ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp))
	{
		ret.pts_s += ptsOffset; // non-zero when pts restamping in use
		ret.dts_s += ptsOffset; // non-zero when pts restamping in use
	}
	return ret;
}

void Demuxer::send()
{
	if (CheckForSteadyState())
	{
		const auto info = UpdateSegmentInfo();

		if (aamp)
		{
			aamp->SendStreamCopy(type, es.GetPtr(), es.GetLen(), info.pts_s, info.dts_s, duration);
		}
		es.Clear();
	}
}

void Demuxer::resetInternal()
{
	es.Free();
	pes_header.Free();
}

void Demuxer::sendInternal(MediaProcessor::process_fcn_t processor)
{
	if (processor)
	{
		if (CheckForSteadyState())
		{
			// Copy the segment data into a vector and pass it to the processing function
			uint8_t * data_ptr = reinterpret_cast<uint8_t *>(es.GetPtr());
			const auto len = es.GetLen();
			std::vector<uint8_t> buf(len);
			const auto info {UpdateSegmentInfo()};
			buf.assign(data_ptr, data_ptr + len);
			processor(type, std::move(info), std::move(buf));
			es.Clear();
		}
	}
	else
	{
		send();
	}
}

void Demuxer::init(double position, double duration, bool trickmode, bool resetBasePTS, bool optimizeMuxed )
{
	std::lock_guard<std::mutex> lock{mMutex};
	this->position = position;
	this->duration = duration;
	this->trickmode = trickmode;
	if (resetBasePTS)
	{
		base_pts = -1;
	}
	current_dts = 0;
	current_pts = 0;
	first_pts = 0;
	update_first_pts = false;
	finalized_base_pts = false;
	pes_state = PES_STATE_WAITING_FOR_HEADER;
	AAMPLOG_DEBUG("init : position %f, duration %f resetBasePTS %d", position, duration, resetBasePTS);
	
	if( optimizeMuxed )
	{ // when hls/ts in use, restamp starting from zero to avoid jitter at playback start
		base_pts = 0;
		finalized_base_pts = true;
	}
}

void Demuxer::flush()
{
	std::lock_guard<std::mutex> lock{mMutex};
	auto len = es.GetLen();
	if (len > 0)
	{
		AAMPLOG_INFO("demux : sending remaining bytes. es.len %d", (int)es.GetLen());
		send();
	}
	resetInternal();
}

void Demuxer::reset()
{
	std::lock_guard<std::mutex> lock{mMutex};
	resetInternal();
}

void Demuxer::setBasePTS(unsigned long long basePTS, bool isFinal)
{
	std::lock_guard<std::mutex> lock{mMutex};
	if (!trickmode)
	{
		AAMPLOG_INFO("Type[%d], basePTS %llu final %d", (int)type, basePTS, (int)isFinal);
	}
	if( !finalized_base_pts )
	{
		base_pts = basePTS;
		finalized_base_pts = isFinal;
	}
}

unsigned long long Demuxer::getBasePTS()
{
	std::lock_guard<std::mutex> lock{mMutex};
	return base_pts.value;
}

void Demuxer::processPacket(const unsigned char * packetStart, bool &basePtsUpdated, bool &ptsError, bool &isPacketIgnored, bool applyOffset, MediaProcessor::process_fcn_t processor)
{
	std::lock_guard<std::mutex> lock{mMutex};
	int adaptation_fieldlen = 0;
	basePtsUpdated = false;
	if (CONTAINS_PAYLOAD(packetStart))
	{
		if (ADAPTATION_FIELD_PRESENT(packetStart))
		{
			adaptation_fieldlen = packetStart[4];
		}
		int pesOffset = 4 + adaptation_fieldlen;
		if (ADAPTATION_FIELD_PRESENT(packetStart))
		{
			pesOffset++;
		}
		/*Store the pts/dts*/
		if (PAYLOAD_UNIT_START(packetStart))
		{
			if (es.GetLen() > 0)
			{
				if (processor)
				{
					sendInternal(processor);
				}
				else
				{
					send();
				}
			}

			const unsigned char* pesStart = packetStart + pesOffset;
			if (IS_PES_PACKET_START(pesStart))
			{
				if (PES_OPTIONAL_HEADER_PRESENT(pesStart))
				{
					if ((pesStart[7] & 0x80) && ((pesStart[9] & 0x20) == 0x20))
					{
						const uint33_t timeStamp = Extract33BitTimestamp(&pesStart[9]);
						auto prev_pts = exchange(current_pts, timeStamp);
						if(prev_pts > current_pts && prev_pts - current_pts > uint33_t::half_max())
						{//pts may come out of order so prev>current is not sufficient to detect the rollover
							AAMPLOG_WARN("PTS Rollover type:%d %" PRIu64 " -> %" PRIu64 , type, prev_pts.value, current_pts.value);
						}
						current_pts = timeStamp;
						AAMPLOG_DEBUG("PTS updated %" PRIu64 , current_pts.value);
						if(!finalized_base_pts)
						{
							finalized_base_pts = true;
							if(!trickmode)
							{
								if (-1 == base_pts)
								{
									/*Few  HLS streams are not muxed TS content, instead video is in TS and audio is in AAC format.
									So need to avoid  pts modification with offset value for video to avoid av sync issues.*/
									if(applyOffset)
									{
										base_pts = current_pts - MAX_FIRST_PTS_OFFSET;
										AAMPLOG_WARN("Type[%d] base_pts not initialized, updated to %" PRIu64 , type, base_pts.value);
									}
									else
									{
										base_pts = current_pts;
									}
								}
								else
								{
									if(current_pts < base_pts)
									{
										auto orig_base_pts = base_pts;
										if (current_pts > MAX_FIRST_PTS_OFFSET)
										{
											if(applyOffset)
											{
												base_pts = current_pts - MAX_FIRST_PTS_OFFSET;
											}
											else
											{
												base_pts = current_pts;
											}
										}
										else
										{
											base_pts = current_pts;
										}
										AAMPLOG_WARN("Type[%d] current_pts[%" PRIu64 "] < base_pts[%" PRIu64 "], base_pts[%" PRIu64 "]->[%" PRIu64 "]",
											type, current_pts.value, base_pts.value, orig_base_pts.value, base_pts.value);
									}
									else /*current_pts >= base_pts*/
									{
										auto delta = current_pts - base_pts;
										if (MAX_FIRST_PTS_OFFSET < delta)
										{
											auto orig_base_pts = base_pts;
											if(applyOffset)
											{
												base_pts = current_pts - MAX_FIRST_PTS_OFFSET;
											}
											else
											{
												base_pts = current_pts;
											}
											AAMPLOG_INFO("Type[%d] delta[%" PRIu64 "] > MAX_FIRST_PTS_OFFSET, current_pts[%" PRIu64 "] base_pts[%" PRIu64 "]->[%" PRIu64 "]",
												type, delta.value, current_pts.value, orig_base_pts.value, base_pts.value);
										}
										else
										{
											AAMPLOG_INFO("Type[%d] PTS in range.delta[%" PRIu64 "] <= MAX_FIRST_PTS_OFFSET base_pts[%" PRIu64 "]",
												type, delta.value, base_pts.value);
										}
									}
								}
							}
							if (-1 == base_pts)
							{
								base_pts = timeStamp;
								AAMPLOG_WARN("base_pts not available, updated to pts %" PRIu64 , timeStamp.value);
							}
							else if (base_pts > timeStamp)
							{
								AAMPLOG_WARN("base_pts update from %" PRIu64 " to %" PRIu64 , base_pts.value, timeStamp.value);
								base_pts = timeStamp;
							}
							basePtsUpdated = true;
						}
					}
					else
					{
						AAMPLOG_WARN("PTS NOT present pesStart[7] & 0x80 = 0x%x pesStart[9]&0xF0 = 0x%x",
							pesStart[7] & 0x80, (pesStart[9] & 0x20));
					}

					if (((pesStart[7] & 0xC0) == 0xC0) && ((pesStart[14] & 0x1) == 0x01))
					{
						current_dts = Extract33BitTimestamp(&pesStart[14]);
					}
					else
					{ // if dts not explicit in pes header, spec says dts=pts
						current_dts = current_pts;
					}
				}
				else
				{
					AAMPLOG_WARN("Optional pes header NOT present pesStart[6] & 0xC0 = 0x%x", pesStart[6] & 0xC0);
				}
			}
			else
			{
				AAMPLOG_WARN("Packet start prefix check failed 0x%x 0x%x 0x%x adaptation_fieldlen %d", pesStart[0],
					pesStart[1], pesStart[2], adaptation_fieldlen);

				/* video stops playing when ad fragments are injected
					* without expected discontinuity tag in the hls manifest files.
					* This results in PTS error and video looping.
					* In particular hls file, video payload alone is available and
					* unexpectedly we received audio payload without proper PES data.
					* But this audio TS packet got processed, and PES packet start code
					* is not available in that packet.
					* This is the first audio TS packet got in the middle of playback and
					* current_pts is not updated, and the pts was default initialized value.
					* So we returned the PTS error from this api, as current_pts is less than
					* base_pts value.
					* Now we have avoided the pts check if the current_pts is not updated for
					* first audio, video or dsmcc packet due to TS packet doesn't have proper PES data.
					*/
				if( current_pts == 0 )
				{
					AAMPLOG_WARN("Avoiding PTS check when new audio or video TS packet is received without proper PES data");
					isPacketIgnored = true;
					return;
				}
			}
			AAMPLOG_DEBUG(" PES_PAYLOAD_LENGTH %d", PES_PAYLOAD_LENGTH(pesStart));
		}
		if ((first_pts == 0) && !update_first_pts)
		{
			first_pts = current_pts;
			update_first_pts = true;
			//Notify first video PTS to AAMP for VTT initialization
			if (aamp && !trickmode && type == eMEDIATYPE_VIDEO)
			{
				aamp->NotifyFirstVideoPTS(first_pts.value);
				//Notifying BasePTS value for media progress event
				aamp->NotifyVideoBasePTS(base_pts.value);
			}
		}
		/*PARSE PES*/
		{
			unsigned char* data = const_cast<unsigned char*>(packetStart) + pesOffset;
			int size = aamp_ts::ts_packet_size - pesOffset;
			int bytes_to_read;
			if (PAYLOAD_UNIT_START(packetStart))
			{
				pes_state = PES_STATE_GETTING_HEADER;
				pes_header.Clear();
				AAMPLOG_DEBUG("Payload Unit Start");
			}

			while (size > 0)
			{
				switch (pes_state)
				{
				case PES_STATE_WAITING_FOR_HEADER:
					AAMPLOG_WARN("PES_STATE_WAITING_FOR_HEADER , discard data. type =%d size = %d", (int)type, size);
					size = 0;
					break;
				case PES_STATE_GETTING_HEADER:
					bytes_to_read = (int)(aamp_ts::pes_min_data - pes_header.GetLen());
					if( bytes_to_read<=0 )
					{
						AAMPLOG_WARN( "bad pes_header length" );
						break;
					}
					if (size < bytes_to_read)
					{
						bytes_to_read = size;
					}
					AAMPLOG_DEBUG("PES_STATE_GETTING_HEADER. size = %d, bytes_to_read =%d", size, bytes_to_read);
					pes_header.AppendBytes( data, bytes_to_read);
					data += bytes_to_read;
					size -= bytes_to_read;
					if (pes_header.GetLen() == aamp_ts::pes_min_data)
					{
						if (!IS_PES_PACKET_START(pes_header.GetPtr()))
						{
							AAMPLOG_WARN("Packet start prefix check failed 0x%x 0x%x 0x%x", pes_header.GetPtr()[0],
								pes_header.GetPtr()[1], pes_header.GetPtr()[2]);
							pes_state = PES_STATE_WAITING_FOR_HEADER;
							break;
						}
						if (PES_OPTIONAL_HEADER_PRESENT(pes_header.GetPtr()))
						{
							pes_state = PES_STATE_GETTING_HEADER_EXTENSION;
							pes_header_ext_len = PES_OPTIONAL_HEADER_LENGTH(pes_header.GetPtr());
							pes_header_ext_read = 0;
							AAMPLOG_DEBUG(
								"Optional header preset len = %d. Switching to PES_STATE_GETTING_HEADER_EXTENSION",
								pes_header_ext_len);
						}
						else
						{
							AAMPLOG_WARN(
								"Optional header not preset pesStart[6] 0x%x bytes_to_read %d- switching to PES_STATE_WAITING_FOR_HEADER",
								pes_header.GetPtr()[6], bytes_to_read);
							pes_state = PES_STATE_WAITING_FOR_HEADER;
						}
					}
					break;
				case PES_STATE_GETTING_HEADER_EXTENSION:
					bytes_to_read = pes_header_ext_len - pes_header_ext_read;
					if (bytes_to_read > size)
					{
						bytes_to_read = size;
					}
					data += bytes_to_read;
					size -= bytes_to_read;
					pes_header_ext_read += bytes_to_read;
					if (pes_header_ext_read == pes_header_ext_len)
					{
						pes_state = PES_STATE_GETTING_ES;
						AAMPLOG_DEBUG("Optional header read. switch to PES_STATE_GETTING_ES");
					}
					break;
				case PES_STATE_GETTING_ES:
					/*Handle padding?*/
					AAMPLOG_TRACE("PES_STATE_GETTING_ES bytes_to_read = %d", size);
					es.AppendBytes(data, size);
					size = 0;
					break;
				default:
					pes_state = PES_STATE_WAITING_FOR_HEADER;
					AAMPLOG_ERR("Invalid pes_state. type =%d size = %d", (int)type, size);
					break;
				}
			}
		}
	}
	else
	{
		AAMPLOG_INFO("No payload in packet packetStart[3] 0x%x", packetStart[3]);
	}
	ptsError = false;
}

