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

#include "tsFragmentProcessor.hpp"
#include "tsDemuxer.hpp"

#include "AampGrowableBuffer.h"
#include "StreamAbstractionAAMP.h"


#include <unordered_set>


#define DESCRIPTOR_TAG_SUBTITLE 0x59
#define DESCRIPTOR_TAG_AC3 0x6A
#define DESCRIPTOR_TAG_EAC3 0x7A


namespace aamp_ts {

enum class StreamType : uint16_t
{
	eSTREAM_TYPE_MPEG2_VIDEO   	= 0x02, /**< MPEG2 Video */
	eSTREAM_TYPE_MPEG1_AUDIO	= 0x03, /**< MPEG1 Audio */
	eSTREAM_TYPE_MPEG2_AUDIO	= 0x04, /**< MPEG2 Audio */
	eSTREAM_TYPE_PES_PRIVATE	= 0x06, /**< PES packets containing private data */
	eSTREAM_TYPE_AAC_ADTS   	= 0x0F, /**< MPEG2 AAC Audio */
	eSTREAM_TYPE_AAC_LATM   	= 0x11, /**< MPEG4 LATM AAC Audio */
	eSTREAM_TYPE_DSM_CC     	= 0x15, /**< ISO/IEC13818-6 DSM CC deferred association tag with ID3 metadata */
	eSTREAM_TYPE_H264       	= 0x1B, /**< H.264 Video */
	eSTREAM_TYPE_HEVC_VIDEO 	= 0x24, /**< HEVC video */
	eSTREAM_TYPE_ATSC_VIDEO 	= 0x80, /**< ATSC Video */
	eSTREAM_TYPE_ATSC_AC3   	= 0x81, /**< ATSC AC3 Audio */
	eSTREAM_TYPE_HDMV_DTS   	= 0x82, /**< HDMV DTS Audio */
	eSTREAM_TYPE_LPCM_AUDIO 	= 0x83, /**< LPCM Audio */
	eSTREAM_TYPE_ATSC_AC3PLUS 	= 0x84,	/**< SDDS Audio */
	eSTREAM_TYPE_DTSHD_AUDIO	= 0x86, /**< DTS-HD Audio */
	eSTREAM_TYPE_ATSC_EAC3  	= 0x87, /**< ATSC E-AC3 Audio */
	eSTREAM_TYPE_DTS_AUDIO  	= 0x8A, /**< DTS Audio  */
	eSTREAM_TYPE_AC3_AUDIO  	= 0x91, /**< A52b/AC3 Audio */
	eSTREAM_TYPE_SDDS_AUDIO1	= 0x94  /**< SDDS Audio */
};

namespace {

	constexpr bool enable_dump_packet {false};

	static void dump_DbgPacket(unsigned char *packet, int packetSize)
	{
		if (enable_dump_packet)
		{
			int i;
			char buff[1024];

			int col = 0;
			int buffPos = 0;
			buff[buffPos] = 0x00;
			for (i = 0; i < packetSize; ++i)
			{
				buffPos += snprintf(&buff[buffPos], (sizeof(buff) - buffPos), "%02X\n", packet[i]);
				++col;
				if (col == 8)
				{
					strcat(buff, " ");
					buffPos += 1;
				}
				if (col == 16)
				{
					buffPos += snprintf(&buff[buffPos], (sizeof(buff) - buffPos), "\n");
					col = 0;
				}
			}
			printf("%s\n", buff);
		}
	}
}

TSFragmentProcessor::TSFragmentProcessor() :
	m_pmtCollector(NULL),
	m_videoComponentCount{0},
	m_audioComponentCount{0},
	m_dsmccComponentFound{false}
{
	bool optimizeMuxed = false;
	
	mAudioDemuxer = aamp_utils::make_unique<Demuxer>(nullptr, eMEDIATYPE_AUDIO, optimizeMuxed );
	AAMPLOG_INFO(" [%p] Audio demuxer: %p", this, mAudioDemuxer.get());

	mVideoDemuxer = aamp_utils::make_unique<Demuxer>(nullptr, eMEDIATYPE_VIDEO, optimizeMuxed);
	AAMPLOG_INFO(" [%p] Video demuxer: %p", this, mVideoDemuxer.get());

	mDsmccDemuxer = aamp_utils::make_unique<Demuxer>(nullptr, eMEDIATYPE_DSM_CC, optimizeMuxed);
	AAMPLOG_INFO(" [%p] DSMCC demuxer: %p", this, mDsmccDemuxer.get());

	ResetAudioComponents();
	ResetVideoComponents();
}

TSFragmentProcessor::~TSFragmentProcessor()
{
	AAMPLOG_DEBUG(" [%p] .dtor()", this);

	if (m_pmtCollector)
	{
		free(m_pmtCollector);
		m_pmtCollector = nullptr;
	}

	ResetAudioComponents();
	ResetVideoComponents();
}

void TSFragmentProcessor::Reset()
{
	m_havePAT = false;
	m_havePMT = false;
	m_demuxInitialized = false;
	m_ActiveAudioTrackIndex = 0;
}

bool TSFragmentProcessor::ProcessFragment(const AampGrowableBuffer & fragment,
	double position, double duration, bool discontinuity_pending,
	MediaProcessor::process_fcn_t processor)
{
	constexpr int m_ttsSize {0};
	const size_t frag_size = fragment.GetLen();
	const uint8_t * base_frag_ptr = reinterpret_cast<const uint8_t *>(fragment.GetPtr());
	uint8_t * curr_packet_ptr = const_cast<uint8_t *>(base_frag_ptr) + m_ttsSize;
	size_t curr_packet_len = frag_size;

	m_packetStartAfterFirstPTS = -1;

	if (discontinuity_pending || !m_demuxInitialized)
	{
		std::array<std::reference_wrapper<std::unique_ptr<Demuxer>>, 3> demuxers {mVideoDemuxer, mAudioDemuxer, mDsmccDemuxer};
		for (auto idx = 0; idx < demuxers.size(); idx++)
		{
			auto & demuxer = demuxers[idx].get();
			if (demuxer)
			{
				AAMPLOG_INFO("Initializing demuxer [%d | %p]", demuxer->GetType(), demuxer.get());

				demuxer->flush();
				demuxer->init(
							  position,
							  duration,
							  false,
							  true,
							  false // optimizeMuxed
							  );
			}
		}
	}

	if (ValidateFragment(fragment, curr_packet_ptr, curr_packet_len))
	{
		// Parse the fragment to extract macro data
		ParseFragment(base_frag_ptr, curr_packet_ptr, curr_packet_len, discontinuity_pending);
		
		// Demux the fragment to extract the actual audio/video/metadata information
		DemuxFragment(curr_packet_ptr, curr_packet_len, discontinuity_pending, processor);
	}
	else
	{
		AAMPLOG_ERR(" Fragment validation failed.");
		return false;
	}

	return true;
}

bool TSFragmentProcessor::ValidateFragment(const AampGrowableBuffer & fragment, uint8_t * & curr_packet_ptr, size_t & curr_len) const
{
	const auto base_frag_ptr = reinterpret_cast<const unsigned char *>(fragment.GetPtr());

	// It seems some ts have an invalid packet at the start, so try skipping it
	while (((curr_packet_ptr[0] != 0x47) || ((curr_packet_ptr[1] & 0x80) != 0x00) || ((curr_packet_ptr[3] & 0xC0) != 0x00)) && (curr_len > ts_packet_size))
	{
		curr_packet_ptr += ts_packet_size; // Just jump a packet
		curr_len -= ts_packet_size;
		// If we don't find any valid packet within 2 packets from the start or the fragment is less than 2 packets reset everything 
		// (and let the following check reject the whole packet)		
		if (((curr_packet_ptr - base_frag_ptr) > (2 * ts_packet_size)) || (curr_len < ts_packet_size))
		{
			AAMPLOG_ERR(" No valid ts packet found near the start of the segment");
			curr_packet_ptr = const_cast<unsigned char *>(base_frag_ptr);
			curr_len = fragment.GetLen();
			break;
		}
	}

	// Dump the offending packet in the logs (?) and terminate the processing
	if ((curr_packet_ptr[0] != 0x47) || ((curr_packet_ptr[1] & 0x80) != 0x00) || ((curr_packet_ptr[3] & 0xC0) != 0x00))
	{
		AAMPLOG_ERR(" Segment doesn't starts with valid TS packet, discarding. Dump first packet");
		size_t n = fragment.GetLen();
		if (n > ts_packet_size)
		{
			n = ts_packet_size;
		}
		for (int i = 0; i < n; i++)
		{
			printf("0x%02x ", curr_packet_ptr[i]);
		}
		printf("\n");
		return false;
	}

	return true;
}

void TSFragmentProcessor::ParseFragment(const uint8_t * base_frag_ptr, uint8_t * packet_ptr, size_t curr_len, bool discontinuity_pending)
{
	const int m_ttsSize {0};
	const uint8_t *packet_end = packet_ptr + curr_len;

	uint32_t packetCount = 0;
	int pid, payloadStart, adaptation, payloadOffset;

	m_actualStartPTS = -1LL;
	if (discontinuity_pending)
	{
		AAMPLOG_INFO(" Discontinuity pending, resetting m_havePAT & m_havePMT to force recalculation.");

		m_havePAT = false;
		m_havePMT = false;
	}

	while (packet_ptr < packet_end)
	{
		pid = (((packet_ptr[1] << 8) | packet_ptr[2]) & 0x1FFF);

		if (pid == 0)
		{
			adaptation = ((packet_ptr[3] & 0x30) >> 4);
			if (adaptation & 0x01)
			{
				payloadOffset = 4;
				if (adaptation & 0x02)
				{
					payloadOffset += (1 + packet_ptr[4]);
				}
				payloadStart = (packet_ptr[1] & 0x40);
				if (payloadStart)
				{
					int tableid = packet_ptr[payloadOffset + 1];
					if (tableid == 0x00)
					{
						int version = packet_ptr[payloadOffset + 6];
						int current = (version & 0x01);
						version = ((version >> 1) & 0x1F);
						// TRACE4("PAT current version %d existing version %d", version, m_versionPAT);

						AAMPLOG_INFO("PAT current version %d existing version %d", version, m_versionPMT);
						if (!m_havePAT || (current && (version != m_versionPAT)))
						{
							dump_DbgPacket(packet_ptr, ts_packet_size);
							int length = ((packet_ptr[payloadOffset + 2] & 0x0F) << 8) + (packet_ptr[payloadOffset + 3]);
							if (length >= aamp_ts::pat_spts_size)
							{
								int patTableIndex = payloadOffset + 9; 				// PAT table start
								int patTableEndIndex = payloadOffset + length -1; 	// end of PAT table

								m_havePAT = true;
								m_versionPAT = version;

								// Find first program number not 0 or until end of PAT
								do {
									m_program = ((packet_ptr[patTableIndex + 0] << 8) + packet_ptr[patTableIndex + 1]);
									m_pmtPid = (((packet_ptr[patTableIndex + 2] & 0x1F) << 8) + packet_ptr[patTableIndex + 3]);
									patTableIndex += aamp_ts::pat_table_entry_size;
								} while (m_program == 0 && patTableIndex < patTableEndIndex);

								if ((m_program != 0) && (m_pmtPid != 0))
								{
									if (length > aamp_ts::pat_spts_size)
									{
										AAMPLOG_WARN(" PAT is MPTS, using program %d.", m_program);
									}
									if (m_havePMT)
									{
										AAMPLOG_INFO(" PMT change detected in PAT");
										m_havePMT = false;
										goto done;
									}
									m_havePMT = false;
									AAMPLOG_INFO(" acquired PAT version %d program %X PMT pid %X", version, m_program, m_pmtPid);
								}
								else
								{
									AAMPLOG_WARN(" ignoring pid 0 TS packet with suspect program %x and pmtPid %x", m_program, m_pmtPid);
									dump_DbgPacket(packet_ptr, ts_packet_size);
									m_program = -1;
									m_pmtPid = -1;
								}
							}
							else
							{
								AAMPLOG_WARN(" ignoring pid 0 TS packet with length of %d - not SPTS?", length);
							}
						}
					}
					else
					{
						AAMPLOG_WARN(" ignoring pid 0 TS packet with tableid of %x", tableid);
					}
				}
				else
				{
					AAMPLOG_WARN(" ignoring pid 0 TS packet with adaptation of %x", adaptation);
				}
			}
			else
			{
				AAMPLOG_WARN(" ignoring pid 0 TS with no payload indicator");
			}
		}
		else if (pid == m_pmtPid)
		{
			AAMPLOG_DEBUG(" Got PMT : m_pmtPid %u", m_pmtPid);

			adaptation = ((packet_ptr[3] & 0x30) >> 4);
			if (adaptation & 0x01)
			{
				payloadOffset = 4;
				if (adaptation & 0x02)
				{
					payloadOffset += (1 + packet_ptr[4]);
				}
				payloadStart = (packet_ptr[1] & 0x40);
				if (payloadStart)
				{
					int tableid = packet_ptr[payloadOffset + 1];
					if (tableid == 0x02)
					{
						int program = ((packet_ptr[payloadOffset + 4] << 8) + packet_ptr[payloadOffset + 5]);
						if (program == m_program)
						{
							int version = packet_ptr[payloadOffset + 6];
							int current = (version & 0x01);
							version = ((version >> 1) & 0x1F);

							AAMPLOG_DEBUG(" PMT current version %d existing version %d", version, m_versionPMT);
							if (!m_havePMT || (current && (version != m_versionPMT)))
							{
								dump_DbgPacket(packet_ptr, ts_packet_size);
								if (m_havePMT && (version != m_versionPMT))
								{
									AAMPLOG_DEBUG(" pmt change detected: version %d -> %d", m_versionPMT, version);
									m_havePMT = false;
									goto done;
								}
								if (!m_havePMT)
								{
									const int sectionLength = (((packet_ptr[payloadOffset + 2] & 0x0F) << 8) + packet_ptr[payloadOffset + 3]);
									// Check if pmt payload fits in one TS packet:
									// section data starts after pointer_field (1), tableid (1), and section length (2)
									if (payloadOffset + 4 + sectionLength <= ts_packet_size - m_ttsSize)
									{
										AAMPLOG_DEBUG(" Processing PMT Section...");
										ProcessPMTSection(packet_ptr + payloadOffset + 4, sectionLength);
									}
									else if (sectionLength <= aamp_ts::pmt_section_max_size)
									{
										AAMPLOG_INFO(" Caching partial PMT");
										if (!m_pmtCollector)
										{
											m_pmtCollector = (unsigned char*)malloc(aamp_ts::pmt_section_max_size);
											if (!m_pmtCollector)
											{
												AAMPLOG_ERR(" Unable to allocate pmt collector buffer - ignoring large pmt (section length %d)", sectionLength);
												goto done;
											}
											AAMPLOG_INFO(" allocating pmt collector buffer %p", m_pmtCollector);
										}

										// section data starts after table id, section length and pointer field
										int sectionOffset = payloadOffset + 4;
										int sectionAvail = ts_packet_size - m_ttsSize - sectionOffset;
										unsigned char *sectionData = packet_ptr + sectionOffset;
										memcpy(m_pmtCollector, sectionData, sectionAvail );
										m_pmtCollectorSectionLength = sectionLength;
										m_pmtCollectorOffset = sectionAvail;
										m_pmtCollectorNextContinuity = ((packet_ptr[3] + 1) & 0xF);
										AAMPLOG_INFO(" starting to collect multi-packet pmt: section length %d", sectionLength);
									}
									else
									{
										AAMPLOG_WARN(" ignoring oversized pmt (section length %d)", sectionLength);
									}
								}
							}
						}
						else
						{
							AAMPLOG_WARN(" ignoring pmt TS packet with mismatched program of %x (expecting %x)", program, m_program);
							dump_DbgPacket(packet_ptr, ts_packet_size);
						}
					}
					else
					{
						AAMPLOG_DEBUG(" ignoring pmt TS packet with tableid of %x", tableid);
					}
				}
				else
				{
					// process subsequent parts of multi-packet pmt
					if (m_pmtCollectorOffset)
					{
						int continuity = (packet_ptr[3] & 0xF);
						if (((continuity + 1) & 0xF) == m_pmtCollectorNextContinuity)
						{
							AAMPLOG_WARN(" next packet of multi-packet pmt has wrong continuity count %d (expecting %d)",
								m_pmtCollectorNextContinuity, continuity);
							// Assume continuity counts for all packets of this pmt will remain the same.
							// Allow this since it has been observed in the field
							m_pmtCollectorNextContinuity = continuity;
						}
						if (continuity == m_pmtCollectorNextContinuity)
						{
							int avail = ts_packet_size - m_ttsSize - payloadOffset;
							int sectionAvail = m_pmtCollectorSectionLength - m_pmtCollectorOffset;
							int copylen = ((avail > sectionAvail) ? sectionAvail : avail);
							if(m_pmtCollector)
							{    //CID:87880 - forward null
								memcpy(&m_pmtCollector[m_pmtCollectorOffset], &packet_ptr[payloadOffset], copylen);
							}
							m_pmtCollectorOffset += copylen;
							if (m_pmtCollectorOffset == m_pmtCollectorSectionLength)
							{
								ProcessPMTSection(m_pmtCollector, m_pmtCollectorSectionLength);
								m_pmtCollectorOffset = 0;
							}
							else
							{
								m_pmtCollectorNextContinuity = ((continuity + 1) & 0xF);
							}
						}
						else
						{
							AAMPLOG_ERR(" aborting multi-packet pmt due to discontinuity error: expected %d got %d",
								m_pmtCollectorNextContinuity, continuity);
							m_pmtCollectorOffset = 0;
						}
					}
					else if (!m_havePMT)
					{
						AAMPLOG_WARN(" ignoring unexpected pmt TS packet without payload start indicator");
					}
				}
			}
			else
			{
				AAMPLOG_WARN(" ignoring unexpected pmt TS packet without payload indicator");
			}
		}

	done:
		packet_ptr += ts_packet_size;
		++packetCount;
	}

	AAMPLOG_INFO(" Parsed %u packets.", packetCount);
}

void TSFragmentProcessor::DemuxFragment(const uint8_t * base_packet_ptr, size_t curr_len, bool discontinuity_pending, 
	MediaProcessor::process_fcn_t processor)
{
	int audio_pid = -1;
	int video_pid = -1;
	int dsmcc_pid = -1;

	uint64_t firstPcr = 0;
	bool basePtsUpdatedFromCurrentSegment = false;

	if (mVideoDemuxer && m_videoComponentCount > 0)
	{
		video_pid = m_videoComponents[0].pid;
	}

	if (mAudioDemuxer && m_audioComponentCount > 0)
	{
		audio_pid = m_audioComponents[m_ActiveAudioTrackIndex].pid;
	}

	if (mDsmccDemuxer && m_dsmccComponentFound)
	{
		dsmcc_pid = m_dsmccComponent.pid;
	}

	uint64_t packet_cnt{0};
	std::unordered_set<Demuxer*> updated_demuxers{};
	uint8_t * curr_ptr = const_cast<uint8_t *>(base_packet_ptr);

	while (curr_len >= ts_packet_size)
	{
		Demuxer * curr_demuxer {nullptr};
		const int pid = (curr_ptr[1] & 0x1f) << 8 | curr_ptr[2];

		if (pid == video_pid)
		{
			curr_demuxer = mVideoDemuxer.get();
		}
		else if (pid == audio_pid)
		{
			curr_demuxer = mAudioDemuxer.get();
		}
		else if (pid == dsmcc_pid)
		{
			curr_demuxer = mDsmccDemuxer.get();
		}

		if ((discontinuity_pending || !m_demuxInitialized ) && !firstPcr && (pid == m_pcrPid))
		{
			int adaptation_fieldlen = 0;
			if ((curr_ptr[3] & 0x20) == 0x20)
			{
				adaptation_fieldlen = curr_ptr[4];
				if (0 != adaptation_fieldlen && (curr_ptr[5] & 0x10))
				{
					firstPcr = (unsigned long long) curr_ptr[6] << 25 | (unsigned long long) curr_ptr[7] << 17
						| (unsigned long long) curr_ptr[8] << 9 | curr_ptr[9] << 1 | (curr_ptr[10] & (0x80)) >> 7;

					AAMPLOG_DEBUG(" Packet: %" PRIu64 " - set base PTS from first PCR: %" PRIu64 " [%d]", packet_cnt, firstPcr, pid);

					if (mDsmccDemuxer)
					{
						AAMPLOG_DEBUG(" Set base PTS for DSMCC demuxer from firstPCR: %" PRIu64 " - pid: %d", firstPcr, pid);
						mDsmccDemuxer->setBasePTS(firstPcr, true);
					}
					m_demuxInitialized = true;
				}
			}
		}

		if (curr_demuxer)
		{
			bool ptsError = false;
			bool basePTSUpdated = false;
			bool isPacketIgnored = false;
			// ToDo: Verify!
			bool applyOffset {true};

			curr_demuxer->processPacket(curr_ptr, basePTSUpdated, ptsError, isPacketIgnored, applyOffset, processor);

			/* Audio is not playing in particular hls file.
			 * We always choose the first audio pid to play the audio data, even if we
			 * have multiple audio tracks in the PMT Table.
			 * But in one particular hls file, we dont have PES data in the first audio pid.
			 * So, we have now modified to choose the next available audio pid index,
			 * when there is no PES data available in the current audio pid.
			 */
			if (isPacketIgnored)
			{
				if( (m_audioComponentCount > 0) && (m_ActiveAudioTrackIndex < m_audioComponentCount-1) )
				{
					m_ActiveAudioTrackIndex++;
					audio_pid = m_audioComponents[m_ActiveAudioTrackIndex].pid;
					AAMPLOG_WARN(" Switched to next audio pid, since no PES data in current pid");
				}
			}

			// Process PTS updates
			if(!m_demuxInitialized)
			{
				AAMPLOG_WARN(" PCR not available before ES packet, updating firstPCR [%llu]", curr_demuxer->getBasePTS());

				m_demuxInitialized = true;
				firstPcr = curr_demuxer->getBasePTS();
			}

			// Generate an error only if the PTS is not being updated by the current packet
			if (ptsError && !basePtsUpdatedFromCurrentSegment)
			{
				AAMPLOG_WARN(" PTS error, discarding segment");
				break;
			}

			if (basePTSUpdated)
			{
				auto curr_base_pts = curr_demuxer->getBasePTS();
				AAMPLOG_INFO(" Base PTS updated from current segment [%p] - %llu [%zu]", curr_demuxer, curr_base_pts, sizeof(curr_base_pts));

				basePtsUpdatedFromCurrentSegment = true;
			}

			// Caching the demuxer pointer for later use
			updated_demuxers.emplace(curr_demuxer);
		}

		curr_ptr += ts_packet_size;
		curr_len -= ts_packet_size;
		packet_cnt++;
	}

	for (auto demuxer : updated_demuxers)
	{
		if (demuxer && demuxer->HasCachedData())
		{
			demuxer->ConsumeCachedData(processor);
		}
	}

	return;
}

void TSFragmentProcessor::ProcessPMTSection(uint8_t * section, size_t sectionLength)
{
	unsigned char *programInfo, *programInfoEnd;
	char work[32];

	int version = ((section[2] >> 1) & 0x1F);
	int pcrPid = (((section[5] & 0x1F) << 8) + section[6]);
	int infoLength = (((section[7] & 0x0F) << 8) + section[8]);

	memset(work, 0, sizeof(work));

	// Reset of old values
	ResetAudioComponents();
	ResetVideoComponents();
	m_dsmccComponentFound = false;

	// Program loop starts after program info descriptor and continues
	// to the CRC at the end of the section
	programInfo = &section[9 + infoLength];
	programInfoEnd = section + sectionLength - 4;
	while (programInfo < programInfoEnd)
	{
		const auto streamType = static_cast<StreamType>(programInfo[0]);
		const int pid = (((programInfo[1] & 0x1F) << 8) + programInfo[2]);
		const int len = (((programInfo[3] & 0x0F) << 8) + programInfo[4]);

		AAMPLOG_DEBUG(" Stream_type: %d - pid: %d | len: %d", static_cast<int>(streamType), pid, len);

		switch (streamType)
		{
		case StreamType::eSTREAM_TYPE_MPEG2_VIDEO:
		case StreamType::eSTREAM_TYPE_HEVC_VIDEO:
		case StreamType::eSTREAM_TYPE_ATSC_VIDEO:
		case StreamType::eSTREAM_TYPE_H264: // H.264 Video
			AAMPLOG_DEBUG(" video_component | pid: %d", pid);
			if (m_videoComponentCount < max_pmt_pid_count)
			{
				m_videoComponents[m_videoComponentCount].pid = pid;
				m_videoComponents[m_videoComponentCount].elemStreamType = static_cast<int>(streamType);
				++m_videoComponentCount;
			}
			else
			{
				AAMPLOG_WARN(" pmt contains more than %zu video PIDs", max_pmt_pid_count);
			}
			break;
		case StreamType::eSTREAM_TYPE_PES_PRIVATE:
		{
			/*Check descriptors for audio in private PES*/
			bool isAudio = false;
			if (len > 2)
			{
				int descIdx, maxIdx;
				int descrTag, descrLen;

				descIdx = 5;
				maxIdx = descIdx + len;

				while (descIdx < maxIdx)
				{
					descrTag = programInfo[descIdx];
					descrLen = programInfo[descIdx + 1];
					AAMPLOG_DEBUG(" descrTag 0x%x descrLen %d", descrTag, descrLen);
					if (descrTag == DESCRIPTOR_TAG_AC3)
					{
						AAMPLOG_DEBUG(" descrTag 0x%x descrLen %d. marking as audio component", descrTag, descrLen);
						isAudio = true;
						// audioFormat = FORMAT_AUDIO_ES_AC3;
						break;
					}
					if (descrTag == DESCRIPTOR_TAG_EAC3)
					{
						AAMPLOG_DEBUG(" descrTag 0x%x descrLen %d. marking as audio component", descrTag, descrLen);
						isAudio = true;
						// audioFormat = FORMAT_AUDIO_ES_EC3;
						break;
					}
					descIdx += (2 + descrLen);
				}
			}
			if (!isAudio)
			{
				break;
			}
		}
		//no break - intentional fall-through. audio detected in private PES
		case StreamType::eSTREAM_TYPE_MPEG1_AUDIO:
		case StreamType::eSTREAM_TYPE_MPEG2_AUDIO:
		case StreamType::eSTREAM_TYPE_AAC_ADTS:
		case StreamType::eSTREAM_TYPE_AAC_LATM:
		case StreamType::eSTREAM_TYPE_ATSC_AC3:
		case StreamType::eSTREAM_TYPE_HDMV_DTS:
		case StreamType::eSTREAM_TYPE_LPCM_AUDIO:
		case StreamType::eSTREAM_TYPE_ATSC_AC3PLUS:
		case StreamType::eSTREAM_TYPE_DTSHD_AUDIO:
		case StreamType::eSTREAM_TYPE_ATSC_EAC3:
		case StreamType::eSTREAM_TYPE_DTS_AUDIO:
		case StreamType::eSTREAM_TYPE_AC3_AUDIO:
		case StreamType::eSTREAM_TYPE_SDDS_AUDIO1:
			AAMPLOG_DEBUG(" audio_component | pid: %d", pid);

			if (m_audioComponentCount < max_pmt_pid_count)
			{
				m_audioComponents[m_audioComponentCount].pid = pid;
				m_audioComponents[m_audioComponentCount].elemStreamType = static_cast<int>(streamType);
				m_audioComponents[m_audioComponentCount].associatedLanguage = nullptr;
				if (len > 2)
				{
					int descIdx, maxIdx;
					int descrTag, descrLen;

					descIdx = 5;
					maxIdx = descIdx + len;

					while (descIdx < maxIdx)
					{
						descrTag = programInfo[descIdx];
						descrLen = programInfo[descIdx + 1];

						switch (descrTag)
						{
							// ISO_639_language_descriptor
						case 0x0A:
							memcpy(work, &programInfo[descIdx + 2], descrLen);
							work[descrLen] = '\0';
							m_audioComponents[m_audioComponentCount].associatedLanguage = strdup(work);
							break;
						}

						descIdx += (2 + descrLen);
					}
				}
				++m_audioComponentCount;
			}
			else
			{
				AAMPLOG_WARN(" pmt contains more than %zu audio PIDs", max_pmt_pid_count);
			}
			break;

		case StreamType::eSTREAM_TYPE_DSM_CC:
			AAMPLOG_DEBUG(" dsmcc_component | pid: %d", pid);

			if (!m_dsmccComponentFound)
			{
				m_dsmccComponent.pid = pid;
				m_dsmccComponent.elemStreamType = static_cast<int>(streamType);
				m_dsmccComponentFound = true;
			}
			break;

		default:
			AAMPLOG_INFO(" PMT contains unused stream type 0x%x", static_cast<int>(streamType));
			break;
		}
		programInfo += (5 + len);
	}

	if (m_videoComponentCount > 0)
	{
		AAMPLOG_INFO( "[%p] found %d video pids in program %d with pcr pid %d and video pid %d",
			this, m_videoComponentCount, m_program, pcrPid, m_videoComponents[0].pid);
	}
	if (m_audioComponentCount > 0)
	{
		AAMPLOG_INFO( "[%p] found %d audio pids in program %d with pcr pid: %d",
			this, m_audioComponentCount, m_program, pcrPid);
	}
	if (m_dsmccComponentFound)
	{
		AAMPLOG_INFO( "[%p] found dsmcc pid in program %d with pcr pid %d dsmcc pid %d",
			this, m_program, pcrPid, m_dsmccComponent.pid);
	}

	if (m_videoComponentCount == 0)
	{
		for (int audioIndex = 0; audioIndex < m_audioComponentCount; ++audioIndex)
		{
			if (pcrPid == m_audioComponents[audioIndex].pid)
			{
				AAMPLOG_INFO(" indexing audio");
				mIndexedAudio = true;

				break;
			}
		}
	}

	m_pcrPid = pcrPid;
	m_versionPMT = version;
	m_havePMT = true;

	return;
}

void TSFragmentProcessor::ResetAudioComponents()
{
	// Reset of old values
	for (auto & comp : m_audioComponents)
	{
		if (comp.associatedLanguage)
		{
			free(comp.associatedLanguage);
		}
		comp.associatedLanguage = nullptr;
		memset(&comp, 0, sizeof(RecordingComponent));
	}

	m_audioComponentCount = 0;	
}

void TSFragmentProcessor::ResetVideoComponents()
{
	for (auto & comp : m_videoComponents)
	{
		memset(&comp, 0, sizeof(RecordingComponent));
	}
	m_videoComponentCount = 0;
}

bool TSFragmentProcessor::ReadTimeStamp(const unsigned char *p, long long& TS)
{
	bool result = true;

	if ((p[0] & 0x01) != 1)
	{
		result = false;
	}
	if ((p[2] & 0x01) != 1)
	{
		result = false;
	}
	if ((p[4] & 0x01) != 1)
	{
		result = false;
	}

	switch ((p[0] & 0xF0) >> 4)
	{
	case 1:
	case 2:
	case 3:
		break;
	default:
		result = false;
		break;
	}

	TS = ((((long long)(p[0] & 0x0E)) << 30) >> 1) |
		(((long long)(p[1] & 0xFF)) << 22) |
		((((long long)(p[2] & 0xFE)) << 15) >> 1) |
		(((long long)(p[3] & 0xFF)) << 7) |
		(((long long)(p[4] & 0xFE)) >> 1);

	return result;
}



}

