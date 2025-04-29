/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
* @file tsprocessor.cpp
* @brief Source file for player context
*/

#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include "priv_aamp.h"
#include "StreamAbstractionAAMP.h"

#include "tsprocessor.h"
#include "tsDemuxer.hpp"

#include "AampUtils.h"

#include "AampSegmentInfo.hpp"

#include <iomanip>
#include <unordered_set>

#define PACKET_SIZE (188)
#define MAX_PACKET_SIZE (208)
#define FIXED_FRAME_RATE (10)
#define FRAME_WIDTH_MAX (1920)
#define FRAME_HEIGHT_MAX (1080)
#define WAIT_FOR_DATA_MILLISEC 100
#define WAIT_FOR_DATA_MAX_RETRIES 1
#define MAX_PMT_SECTION_SIZE (1021)
#define PATPMT_MAX_SIZE (2*1024)
#define PAT_SPTS_SIZE (13)
#define PAT_TABLE_ENTRY_SIZE (4)

/** Maximum PTS value */
#define MAX_PTS (uint33_t::max_value().value)

/** Maximum descriptor present for a elementary stream */
#define MAX_DESCRIPTOR (4)

#define I_FRAME (0x1)
#define P_FRAME (0x2)
#define B_FRAME (0x4)
#define ALL_FRAMES (0x7)
#define SEQ_START (0x10)
#define IDX_BUFF_SIZE (128*1024)

/*Throttle Default Parameters - From DVRManager*/
#define DEFAULT_THROTTLE_MAX_DELAY_MS 500
#define DEFAULT_THROTTLE_MAX_DIFF_SEGMENTS_MS 400
#define DEFAULT_THROTTLE_DELAY_IGNORED_MS 200
#define DEFAULT_THROTTLE_DELAY_FOR_DISCONTINUITY_MS 2000

#define DESCRIPTOR_TAG_SUBTITLE 0x59
#define DESCRIPTOR_TAG_AC3 0x6A
#define DESCRIPTOR_TAG_EAC3 0x7A

/**
 * @enum StreamType
 * @brief Types of streams
 */

enum StreamType
{
	eSTREAM_TYPE_MPEG2_VIDEO = 0x02,   /**< MPEG2 Video */
	eSTREAM_TYPE_MPEG1_AUDIO = 0x03,   /**< MPEG1 Audio */
	eSTREAM_TYPE_MPEG2_AUDIO = 0x04,   /**< MPEG2 Audio */
	eSTREAM_TYPE_PES_PRIVATE = 0x06,   /**< PES packets containing private data */
	eSTREAM_TYPE_AAC_ADTS    = 0x0F,   /**< MPEG2 AAC Audio */
	eSTREAM_TYPE_AAC_LATM    = 0x11,   /**< MPEG4 LATM AAC Audio */
	eSTREAM_TYPE_DSM_CC      = 0x15,   /**< ISO/IEC13818-6 DSM CC deferred association tag with ID3 metadata */
	eSTREAM_TYPE_H264        = 0x1B,   /**< H.264 Video */
	eSTREAM_TYPE_HEVC_VIDEO  = 0x24,   /**< HEVC video */
	eSTREAM_TYPE_ATSC_VIDEO  = 0x80,   /**< ATSC Video */
	eSTREAM_TYPE_ATSC_AC3    = 0x81,   /**< ATSC AC3 Audio */
	eSTREAM_TYPE_HDMV_DTS    = 0x82,   /**< HDMV DTS Audio */
	eSTREAM_TYPE_LPCM_AUDIO  = 0x83,   /**< LPCM Audio */
	eSTREAM_TYPE_ATSC_AC3PLUS  = 0x84, /**< SDDS Audio */
	eSTREAM_TYPE_DTSHD_AUDIO = 0x86,   /**< DTS-HD Audio */
	eSTREAM_TYPE_ATSC_EAC3   = 0x87,   /**< ATSC E-AC3 Audio */
	eSTREAM_TYPE_DTS_AUDIO   = 0x8A,   /**< DTS Audio  */
	eSTREAM_TYPE_AC3_AUDIO   = 0x91,   /**< A52b/AC3 Audio */
	eSTREAM_TYPE_SDDS_AUDIO1 = 0x94    /**< SDDS Audio */
};

/**
 * @brief Get the format for a stream type
 * @param[in] streamType stream type
 * @return StreamOutputFormat format for input stream type
 */
static StreamOutputFormat getStreamFormatForCodecType(int streamType)
{
	StreamOutputFormat format = FORMAT_UNKNOWN; // Maybe GStreamer will be able to detect the format

	switch (streamType)
	{
		case eSTREAM_TYPE_MPEG2_VIDEO:
			format = FORMAT_VIDEO_ES_MPEG2;
			break;
		case eSTREAM_TYPE_MPEG1_AUDIO:
			format = FORMAT_AUDIO_ES_MP3;
			break;
		case eSTREAM_TYPE_MPEG2_AUDIO:
		case eSTREAM_TYPE_AAC_ADTS:
		case eSTREAM_TYPE_AAC_LATM:
			format = FORMAT_AUDIO_ES_AAC;
			break;
		case eSTREAM_TYPE_H264:
			format = FORMAT_VIDEO_ES_H264;
			break;
		case eSTREAM_TYPE_HEVC_VIDEO:
			format = FORMAT_VIDEO_ES_HEVC;
			break;
		case eSTREAM_TYPE_ATSC_AC3:
			format = FORMAT_AUDIO_ES_AC3;
			break;
		case eSTREAM_TYPE_ATSC_AC3PLUS:
		case eSTREAM_TYPE_ATSC_EAC3:
			format = FORMAT_AUDIO_ES_EC3;
			break;
		case eSTREAM_TYPE_PES_PRIVATE:
			//could be any ac4, ac3, subtitles etc.
			break;
		// not sure on the format, so keep as unknown
		case eSTREAM_TYPE_DSM_CC:
		case eSTREAM_TYPE_ATSC_VIDEO:
		case eSTREAM_TYPE_HDMV_DTS:
		case eSTREAM_TYPE_LPCM_AUDIO:
		case eSTREAM_TYPE_DTSHD_AUDIO:
		case eSTREAM_TYPE_DTS_AUDIO:
		case eSTREAM_TYPE_AC3_AUDIO:
		case eSTREAM_TYPE_SDDS_AUDIO1:
		default:
			break;
	}
	return format;
}

/**
 * @brief TSProcessor Constructor
 */
TSProcessor::TSProcessor(class PrivateInstanceAAMP *aamp,StreamOperation streamOperation, id3_callback_t id3_hdl, int track, TSProcessor* peerTSProcessor, TSProcessor* auxTSProcessor)
	: m_needDiscontinuity(true),
	m_PatPmtLen(0), m_PatPmt(0), m_PatPmtTrickLen(0), m_PatPmtTrick(0), m_PatPmtPcrLen(0), m_PatPmtPcr(0),
	m_nullPFrame(0), m_nullPFrameLength(0), m_nullPFrameNextCount(0), m_nullPFrameOffset(0),
	m_emulationPreventionCapacity(0), m_emulationPreventionOffset(0), m_emulationPrevention(0), aamp(aamp),
	m_currStreamOffset(0), m_currPTS(-1), m_currTimeStamp(-1LL), m_currFrameNumber(-1),
	m_currFrameLength(0), m_currFrameOffset(-1LL), m_trickExcludeAudio(true), m_patCounter(0),
	m_pmtCounter(0), m_playMode(PlayMode_normal), m_playModeNext(PlayMode_normal), m_playRate(1.0f), m_absPlayRate(1.0f),
	m_playRateNext(1.0f), m_apparentFrameRate(FIXED_FRAME_RATE), m_packetSize(PACKET_SIZE), m_ttsSize(0),
	m_pcrPid(-1), m_videoPid(-1), m_haveBaseTime(false), m_haveEmittedIFrame(false), m_haveUpdatedFirstPTS(true),
	m_pcrPerPTSCount(0), m_baseTime(0), m_segmentBaseTime(0), m_basePCR(-1LL), m_prevRateAdjustedPCR(-1LL),
	m_currRateAdjustedPCR(0), m_currRateAdjustedPTS(-1LL), m_nullPFrameWidth(-1), m_nullPFrameHeight(-1),
	m_frameWidth(FRAME_WIDTH_MAX), m_frameHeight(FRAME_HEIGHT_MAX), m_scanForFrameSize(false), m_scanRemainderSize(0),
	m_scanRemainderLimit(SCAN_REMAINDER_SIZE_MPEG2), m_isH264(false), m_isMCChannel(false), m_isInterlaced(false), m_isInterlacedKnown(false),
	m_throttle(true), m_haveThrottleBase(false), m_lastThrottleContentTime(-1LL), m_lastThrottleRealTime(-1LL),
	m_baseThrottleContentTime(-1LL), m_baseThrottleRealTime(-1LL), m_throttlePTS{uint33_t::max_value()}, m_insertPCR(false),
	m_scanSkipPacketsEnabled(false), m_currSPSId(0), m_picOrderCount(0), m_updatePicOrderCount(false),
	m_havePAT(false), m_versionPAT(0), m_program(0), m_pmtPid(0), m_havePMT(false), m_versionPMT(-1), m_indexAudio(false),
	m_haveAspect(false), m_haveFirstPTS(false), m_currentPTS(uint33_t::max_value()), m_pmtCollectorNextContinuity(0),
	m_pmtCollectorSectionLength(0), m_pmtCollectorOffset(0), m_pmtCollector(NULL),
	m_scrambledWarningIssued(false), m_checkContinuity(false), videoComponentCount(0), audioComponentCount(0),
	m_actualStartPTS{uint33_t::max_value()}, m_throttleMaxDelayMs(DEFAULT_THROTTLE_MAX_DELAY_MS),
	m_throttleMaxDiffSegments(DEFAULT_THROTTLE_MAX_DIFF_SEGMENTS_MS),
	m_throttleDelayIgnoredMs(DEFAULT_THROTTLE_DELAY_IGNORED_MS), m_throttleDelayForDiscontinuityMs(DEFAULT_THROTTLE_DELAY_FOR_DISCONTINUITY_MS),
	m_throttleCond(), m_basePTSCond(), m_mutex(), m_enabled(true), m_processing(false), m_framesProcessedInSegment(0),
	m_lastPTSOfSegment(-1), m_streamOperation(streamOperation), m_vidDemuxer(NULL), m_audDemuxer(NULL), m_dsmccDemuxer(NULL),
	m_demux(false), m_peerTSProcessor(peerTSProcessor), m_packetStartAfterFirstPTS(-1), m_queuedSegment(NULL),
	m_queuedSegmentPos(0), m_queuedSegmentDuration(0), m_queuedSegmentLen(0), m_queuedSegmentDiscontinuous(false), m_startPosition(-1.0),
	m_track(track), m_last_frame_time(0), m_demuxInitialized(false), m_basePTSFromPeer(-1), m_dsmccComponentFound(false), m_dsmccComponent()
	, m_AudioTrackIndexToPlay(0)
	, m_auxTSProcessor(auxTSProcessor)
	, m_auxiliaryAudio(false)
	,m_audioGroupId()
	,m_applyOffset(true)
{
	AAMPLOG_INFO(" constructor: %p", this);
	bool optimizeMuxed = false;

	if( aamp && ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp))
	{
		optimizeMuxed = (m_streamOperation == eStreamOp_DEMUX_ALL);
	}

	memset(m_SPS, 0, 32 * sizeof(H264SPS));
	memset(m_PPS, 0, 256 * sizeof(H264PPS));
	m_versionPMT = 0;

	if ((m_streamOperation == eStreamOp_DEMUX_ALL) || (m_streamOperation == eStreamOp_DEMUX_VIDEO) || (m_streamOperation == eStreamOp_DEMUX_VIDEO_AND_AUX))
	{
		m_vidDemuxer = new Demuxer(aamp, eMEDIATYPE_VIDEO, optimizeMuxed );
		//demux DSM CC stream only together with video stream
		m_dsmccDemuxer = new Demuxer(aamp, eMEDIATYPE_DSM_CC, optimizeMuxed );
		m_demux = true;
	}

	if ((m_streamOperation == eStreamOp_DEMUX_ALL) || (m_streamOperation == eStreamOp_DEMUX_AUDIO))
	{
		m_audDemuxer = new Demuxer(aamp, eMEDIATYPE_AUDIO, optimizeMuxed);
		m_demux = true;
	}
	else if ((m_streamOperation == eStreamOp_DEMUX_AUX) || m_streamOperation == eStreamOp_DEMUX_VIDEO_AND_AUX)
	{
		m_auxiliaryAudio = true;
		m_audDemuxer = new Demuxer(aamp, eMEDIATYPE_AUX_AUDIO, optimizeMuxed);
		// Map auxiliary specific streamOperation back to generic streamOperation used by TSProcessor
		if (m_streamOperation == eStreamOp_DEMUX_AUX)
		{
			m_streamOperation = eStreamOp_DEMUX_AUDIO; // this is an audio only streamOperation
		}
		else
		{
			m_streamOperation = eStreamOp_DEMUX_ALL; // this is a muxed streamOperation
		}
		m_demux = true;
	}

	int compBufLen = MAX_PIDS*sizeof(RecordingComponent);
	memset(videoComponents, 0, compBufLen);
	memset(audioComponents, 0, compBufLen);
}

/**
 * @brief TSProcessor Destructor
 */
TSProcessor::~TSProcessor()
{
	AAMPLOG_INFO("destructor: %p", this);
	if (m_PatPmt)
	{
		free(m_PatPmt);
		m_PatPmt = 0;
	}
	if (m_PatPmtTrick)
	{
		free(m_PatPmtTrick);
		m_PatPmtTrick = 0;
	}
	if (m_PatPmtPcr)
	{
		free(m_PatPmtPcr);
		m_PatPmtPcr = 0;
	}

	if (m_nullPFrame)
	{
		free(m_nullPFrame);
		m_nullPFrame = 0;
	}
	if (m_pmtCollector)
	{
		free(m_pmtCollector);
		m_pmtCollector = 0;
	}
	if (m_emulationPrevention)
	{
		free(m_emulationPrevention);
		m_emulationPrevention = 0;
	}

	for (int i = 0; i < audioComponentCount; ++i)
	{
		if (audioComponents[i].associatedLanguage)
		{
			free(audioComponents[i].associatedLanguage);
		}
	}

	SAFE_DELETE(m_vidDemuxer);
	SAFE_DELETE(m_audDemuxer);
	SAFE_DELETE(m_dsmccDemuxer);

	if (m_queuedSegment)
	{
		free(m_queuedSegment);
		m_queuedSegment = NULL;
	}
}


#define BUFFER_POOL_SIZE (10)
#define PLAY_BUFFER_AUDIO_MAX_PACKETS (100)
#define PLAY_BUFFER_MAX_PACKETS (512)
#define PLAY_BUFFER_SIZE (PLAY_BUFFER_MAX_PACKETS*MAX_PACKET_SIZE)
#define POOL_BUFFER_ALIGNMENT (16)
#define PLAY_BUFFER_CTX_OFFSET (0)
#define PLAY_BUFFER_SIGNATURE (('P')|(('L')<<8)|(('A')<<16)|(('Y')<<24))


/**
 * @brief Insert PAT and PMT sections
 * @retval length of output buffer
 */
int TSProcessor::insertPatPmt(unsigned char *buffer, bool trick, int bufferSize)
{
	int len;

	if (trick && m_trickExcludeAudio)
	{
		len = m_PatPmtTrickLen;
		memcpy(buffer, m_PatPmtTrick, len);
	}
	else if (trick && m_isMCChannel)
	{
		len = m_PatPmtPcrLen;
		memcpy(buffer, m_PatPmtPcr, len);
	}
	else
	{
		len = m_PatPmtLen;
		memcpy(buffer, m_PatPmt, len);
	}

	int index = 3 + m_ttsSize;
	buffer[index] = ((buffer[index] & 0xF0) | (m_patCounter++ & 0x0F));

	index += m_packetSize;
	while (index < len)
	{
		buffer[index] = ((buffer[index] & 0xF0) | (m_pmtCounter++ & 0x0F));
		index += m_packetSize;
	}

	return len;
}

/**
 * @brief insert PCR to the packet in case of PTS restamping
 */
void TSProcessor::insertPCR(unsigned char *packet, int pid)
{
	int i;
	long long currPCR;

	assert(m_playMode == PlayMode_retimestamp_Ionly);
	i = 0;
	if (m_ttsSize)
	{
		memset(packet, 0, m_ttsSize);
		i += m_ttsSize;
	}
	packet[i + 0] = 0x47;
	packet[i + 1] = (0x60 | (unsigned char)((pid >> 8) & 0x1F));
	packet[i + 2] = (unsigned char)(0xFF & pid);
	// 2 bits Scrambling = no; 2 bits adaptation field = has adaptation, no payload; 4 bits continuity counter
	// Don't increment continuity counter since there is no payload
	packet[i + 3] = (0x20 | (m_continuityCounters[pid] & 0x0F));
	packet[i + 4] = 0xB7; // 1 byte of adaptation data, but require length of 183 when no payload is indicated
	packet[i + 5] = 0x10; // PCR
	currPCR = ((m_currRateAdjustedPTS - 10000) & 0x1FFFFFFFFLL);
	AAMPLOG_TRACE("TSProcessor::insertPCR: m_currRateAdjustedPTS= %llx currPCR= %llx", m_currRateAdjustedPTS, currPCR);
	writePCR(&packet[i + 6], currPCR, true);
	for (int i = 6 + 6 + m_ttsSize; i < m_packetSize; i++)
	{
		packet[i] = 0xFF;
	}
}

/**
 * @brief process PMT section and update media components.
 * @note Call with section pointing to first byte after section_length
 */
void TSProcessor::processPMTSection(unsigned char* section, int sectionLength)
{
	unsigned char *programInfo, *programInfoEnd;
	unsigned int dataDescTags[MAX_PIDS];
	int streamType = 0, pid = 0, len = 0;
	char work[32];
	StreamOutputFormat videoFormat = FORMAT_INVALID;
	StreamOutputFormat audioFormat = FORMAT_INVALID;
	bool cueiDescriptorFound = false;

	int version = ((section[2] >> 1) & 0x1F);
	int pcrPid = (((section[5] & 0x1F) << 8) + section[6]);
	int infoLength = (((section[7] & 0x0F) << 8) + section[8]);

	for (int i = 0; i < audioComponentCount; ++i)
	{
		if (audioComponents[i].associatedLanguage)
		{
			free(audioComponents[i].associatedLanguage);
		}
	}

	memset(videoComponents, 0, sizeof(videoComponents));
	memset(audioComponents, 0, sizeof(audioComponents));

	memset(dataDescTags, 0, sizeof(dataDescTags));

	memset(work, 0, sizeof(work));

	videoComponentCount = audioComponentCount = 0;
	m_dsmccComponentFound = false;

	//program info descriptors
	unsigned char *esInfo = section + 9;
	unsigned char *esInfoEnd = esInfo + infoLength;
	while (esInfo < esInfoEnd)
	{
        /*If the PMT program_info loop carries a registration descriptor (tag = 0x05)
         and the registration descriptor carries a format_identifier attribute with value of "0x43554549" (as per ANSI/SCTE 35 2019a), then the corresponding ES with stream_type as "0x86" is an ES carrying SCTE-35 payload */
		int descriptorTag = esInfo[0];
		int descriptorLength = esInfo[1];
		AAMPLOG_INFO("program info descriptors : descriptorTag 0x%x  descriptorLength %d", descriptorTag ,descriptorLength);
		if (descriptorTag == 0x05 && descriptorLength >= 4 &&
			esInfo[2] == 'C' && esInfo[3] == 'U' && esInfo[4] == 'E' && esInfo[5] == 'I')
		{
			AAMPLOG_INFO("Found SCTE-35 descriptor (CUEI)");
			cueiDescriptorFound = true;
		}
		// move to the next descriptor
		esInfo += 2 + descriptorLength; // 2 - descriptor tag and descriptor length bytes
	}
	// Program loop starts after program info descriptor and continues
	// to the CRC at the end of the section
	programInfo = &section[9 + infoLength];
	programInfoEnd = section + sectionLength - 4;

	while (programInfo < programInfoEnd)
	{
		streamType = programInfo[0];
		pid = (((programInfo[1] & 0x1F) << 8) + programInfo[2]);
		len = (((programInfo[3] & 0x0F) << 8) + programInfo[4]);

		switch (streamType)
		{
		case eSTREAM_TYPE_MPEG2_VIDEO:
		case eSTREAM_TYPE_HEVC_VIDEO:
		case eSTREAM_TYPE_ATSC_VIDEO:
			if (videoComponentCount < MAX_PIDS)
			{
				videoComponents[videoComponentCount].pid = pid;
				videoComponents[videoComponentCount].elemStreamType = streamType;
				++videoComponentCount;
			}
			else
			{
				AAMPLOG_WARN("Warning: RecordContext: pmt contains more than %d video PIDs", MAX_PIDS);
			}
			break;
		case eSTREAM_TYPE_H264: // H.264 Video
			if (videoComponentCount < MAX_PIDS)
			{
				videoComponents[videoComponentCount].pid = pid;
				videoComponents[videoComponentCount].elemStreamType = streamType;
				++videoComponentCount;
				m_isH264 = true;
				m_scanRemainderLimit = SCAN_REMAINDER_SIZE_H264;
			}
			else
			{
				AAMPLOG_WARN("Warning: RecordContext: pmt contains more than %d video PIDs", MAX_PIDS);
			}
			break;
		case eSTREAM_TYPE_PES_PRIVATE:
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
					AAMPLOG_INFO("descrTag 0x%x descrLen %d", descrTag, descrLen);
					if (descrTag == DESCRIPTOR_TAG_AC3)
					{
						AAMPLOG_INFO("descrTag 0x%x descrLen %d. marking as audio component", descrTag, descrLen);
						isAudio = true;
						audioFormat = FORMAT_AUDIO_ES_AC3;
						break;
					}
					if (descrTag == DESCRIPTOR_TAG_EAC3)
					{
						AAMPLOG_INFO("descrTag 0x%x descrLen %d. marking as audio component", descrTag, descrLen);
						isAudio = true;
						audioFormat = FORMAT_AUDIO_ES_EC3;
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
		case eSTREAM_TYPE_MPEG1_AUDIO:
		case eSTREAM_TYPE_MPEG2_AUDIO:
		case eSTREAM_TYPE_AAC_ADTS:
		case eSTREAM_TYPE_AAC_LATM:
		case eSTREAM_TYPE_ATSC_AC3:
		case eSTREAM_TYPE_HDMV_DTS:
		case eSTREAM_TYPE_LPCM_AUDIO:
		case eSTREAM_TYPE_ATSC_AC3PLUS:
		case eSTREAM_TYPE_DTSHD_AUDIO:
		if (cueiDescriptorFound && streamType == eSTREAM_TYPE_DTSHD_AUDIO)
		{
			// Skip processing for DTSHD_AUDIO(0x86) if cueiDescriptorFound is true , since it is SCTE-35 data
			AAMPLOG_INFO(" Stream Type : SCTE-35");
			break;
		}
		case eSTREAM_TYPE_ATSC_EAC3:
		case eSTREAM_TYPE_DTS_AUDIO:
		case eSTREAM_TYPE_AC3_AUDIO:
		case eSTREAM_TYPE_SDDS_AUDIO1:
			if (audioComponentCount < MAX_PIDS)
			{
				audioComponents[audioComponentCount].pid = pid;
				audioComponents[audioComponentCount].elemStreamType = streamType;
				audioComponents[audioComponentCount].associatedLanguage = 0;
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
							audioComponents[audioComponentCount].associatedLanguage = strdup(work);
							break;
						}

						descIdx += (2 + descrLen);
					}
				}
				++audioComponentCount;
			}
			else
			{
				AAMPLOG_WARN("Warning: RecordContext: pmt contains more than %d audio PIDs", MAX_PIDS);
			}
			break;

		case eSTREAM_TYPE_DSM_CC:
			if(!m_dsmccComponentFound)
			{
				m_dsmccComponent.pid = pid;
				m_dsmccComponent.elemStreamType = streamType;
				m_dsmccComponentFound = true;
			}
			break;

		default:
			AAMPLOG_DEBUG("RecordContext: pmt contains unused stream type 0x%x", streamType);
			break;
		}
		programInfo += (5 + len);
	}
	aamp->mAudioComponentCount = audioComponentCount;
	aamp->mVideoComponentCount = videoComponentCount;
	if (videoComponentCount > 0)
	{
		m_videoPid = videoComponents[0].pid;
		AAMPLOG_INFO( "[%p] found %d video pids in program %d with pcr pid %d video pid %d",
			this, videoComponentCount, m_program, pcrPid, m_videoPid);
	}
	if (audioComponentCount > 0)
	{
		std::vector<AudioTrackInfo> audioTracks;
		std::map<std::string, int> languageCount;
		for(int i=0; i< audioComponentCount; i++)
		{
			std::string index = "mux-" + std::to_string(i);
			std::string language;
			if(audioComponents[i].associatedLanguage)
			{
				language = Getiso639map_NormalizeLanguageCode(audioComponents[i].associatedLanguage,aamp->GetLangCodePreference());
			}
			std::string group_id = m_audioGroupId;
			std::string name = language; // use 3 character language code as default track name
			// Note that we COULD map the full human-readable language name (i.e. eng -> English) here,
			// but better to let the UI layer localize, if required.
			// Video engine doesn't know if "German" or "Deutsch" is better as track name.
			int count = ++languageCount[language];
			if (count > 1) { // append suffix to make unique if needed
				name += std::to_string(count);
			}
			std::string characteristics = "muxed-audio";
			StreamOutputFormat streamtype = getStreamFormatForCodecType(audioComponents[i].elemStreamType);
			std::string codec = GetAudioFormatStringForCodec(streamtype);
			audioTracks.push_back(AudioTrackInfo(index, language, group_id, name, codec, characteristics, 0));
			AAMPLOG_INFO( "[%p] found audio#%d in program %d with pcr pid %d audio pid %d lan:%s codec:%s group:%s",
				this, i, m_program, pcrPid, audioComponents[i].pid, language.c_str(), codec.c_str(), group_id.c_str());
		}
		if(ISCONFIGSET(eAAMPConfig_EnablePublishingMuxedAudio))
		{
			if(audioTracks.size() > 0)
			{
				if(aamp->mpStreamAbstractionAAMP)
				{
					aamp->mpStreamAbstractionAAMP->SetAudioTrackInfoFromMuxedStream(audioTracks);
				}
			}
			if(m_audDemuxer)
			{
				// Audio demuxer found, select audio by preference
				int trackIndex = SelectAudioIndexToPlay();
				if(trackIndex != -1 && trackIndex < audioComponentCount && aamp && aamp->mpStreamAbstractionAAMP)
				{
					AAMPLOG_INFO("Selected best track audio#%d with per preference", trackIndex);
					m_AudioTrackIndexToPlay = trackIndex;
					std::string index = "mux-" + std::to_string(trackIndex);
					aamp->mpStreamAbstractionAAMP->SetCurrentAudioTrackIndex(index);
				}
			}
		}
	}

	if (videoComponentCount > 0)
	{
		videoFormat = getStreamFormatForCodecType(videoComponents[0].elemStreamType);
	}
	if (audioComponentCount > 0 && (eSTREAM_TYPE_PES_PRIVATE != audioComponents[0].elemStreamType))
	{
		audioFormat = getStreamFormatForCodecType(audioComponents[0].elemStreamType);
	}
	// Notify the format to StreamSink
	if (!m_auxiliaryAudio)
	{
		aamp->SetStreamFormat(videoFormat, audioFormat, FORMAT_INVALID);
	}
	else
	{
		aamp->SetStreamFormat(videoFormat, FORMAT_INVALID, audioFormat);
	}

	if (m_dsmccComponentFound)
	{
		AAMPLOG_INFO( "[%p] found dsmcc pid in program %d with pcr pid %d dsmcc pid %d",
			this, m_program, pcrPid, m_dsmccComponent.pid);
	}

	if (videoComponentCount == 0)
	{
		for (int audioIndex = 0; audioIndex < audioComponentCount; ++audioIndex)
		{
			if (pcrPid == audioComponents[audioIndex].pid)
			{
				AAMPLOG_INFO("RecordContext: indexing audio");
				m_indexAudio = true;

				break;
			}
		}
	}

	m_pcrPid = pcrPid;
	m_versionPMT = version;
	m_havePMT = true;
}

/**
 * @brief Generate and update PAT and PMT sections
 */
void TSProcessor::updatePATPMT()
{

	if (m_PatPmt)
	{
		free(m_PatPmt);
		m_PatPmt = 0;
	}
	if (m_PatPmtTrick)
	{
		free(m_PatPmtTrick);
		m_PatPmtTrick = 0;
	}

	if (m_PatPmtPcr)
	{
		free(m_PatPmtPcr);
		m_PatPmtPcr = 0;
	}

	generatePATandPMT(false, &m_PatPmt, &m_PatPmtLen);
	generatePATandPMT(true, &m_PatPmtTrick, &m_PatPmtTrickLen);
	generatePATandPMT(false, &m_PatPmtPcr, &m_PatPmtPcrLen, true);
}

/**
 * @brief Send discontinuity packet. Not relevant for demux operations
 */
void TSProcessor::sendDiscontinuity(double position)
{

	long long currPTS, currPCR, insertPCR = -1LL;
	bool haveInsertPCR = false;

	// Set inital PCR value based on first PTS
	currPTS = m_currentPTS.value;
	currPCR = ((currPTS - 10000) & 0x1FFFFFFFFLL);
	if (!m_haveBaseTime)
	{
		if (m_playModeNext == PlayMode_retimestamp_Ionly)
		{
			haveInsertPCR = true;
			insertPCR = currPCR;

			m_haveUpdatedFirstPTS = false;
			m_pcrPerPTSCount = 0;
			m_prevRateAdjustedPCR = -1LL;
			m_currRateAdjustedPCR = currPCR;
			m_currRateAdjustedPTS = currPTS;
			m_currPTS = -1LL;
		}
#if 0
		else
		{
			haveInsertPCR = findNextPCR(insertPCR);
			if (haveInsertPCR)
			{
				insertPCR -= 1;
			}
		}
		if (haveInsertPCR && (m_playRateNext != 1.0))
		{
			m_haveBaseTime = true;
			m_baseTime = insertPCR;
			m_segmentBaseTime = m_baseTime;
			INFO("have baseTime %llx from pid %x on signal of PCR discontinuity", m_baseTime, m_pcrPid);
		}
#endif
	}

	// Signal a discontinuity on the PCR pid and the video pid
	// if there is a video pid and it is not the same as the PCR
	int discCount = 1;
	if ((m_videoPid != -1) && (m_videoPid != m_pcrPid))
	{
		discCount += 1;
	}
	for (int discIndex = 0; discIndex < discCount; ++discIndex)
	{
		int discPid = (discIndex == 0) ? m_pcrPid : m_videoPid;
		unsigned char discontinuityPacket[MAX_PACKET_SIZE];
		int i = 0;
		if (m_ttsSize)
		{
			memset(discontinuityPacket, 0, m_ttsSize);
			i += m_ttsSize;
		}
		discontinuityPacket[i + 0] = 0x47;
		discontinuityPacket[i + 1] = 0x60;
		discontinuityPacket[i + 1] |= (unsigned char)((discPid >> 8) & 0x1F);
		discontinuityPacket[i + 2] = (unsigned char)(0xFF & discPid);
		discontinuityPacket[i + 3] = 0x20; // 2 bits Scrambling = no; 2 bits adaptation field = has adaptation, no cont; 4 bits continuity counter
		discontinuityPacket[i + 4] = 0xB7; // 1 byte of adaptation data, but require length of 183 when no payload is indicated
		discontinuityPacket[i + 5] = 0x80; // discontinuity
		for (int i = 6 + m_ttsSize; i < m_packetSize; i++)
		{
			discontinuityPacket[i] = 0xFF;
		}
		m_continuityCounters[discPid] = 0x00;

		AAMPLOG_TRACE("emit pcr discontinuity");
		if (!m_demux)
		{
			aamp->SendStreamCopy((AampMediaType)m_track, discontinuityPacket, m_packetSize, position, position, 0);
		}
		if (haveInsertPCR)
		{
			i = 0;
			if (m_ttsSize)
			{
				memset(discontinuityPacket, 0, m_ttsSize);
				i += m_ttsSize;
			}
			discontinuityPacket[i + 0] = 0x47;
			discontinuityPacket[i + 1] = 0x60;
			discontinuityPacket[i + 1] |= (unsigned char)((discPid >> 8) & 0x1F);
			discontinuityPacket[i + 2] = (unsigned char)(0xFF & discPid);
			discontinuityPacket[i + 3] = 0x21; // 2 bits Scrambling = no; 2 bits adaptation field = has adaptation, no cont; 4 bits continuity counter
			discontinuityPacket[i + 4] = 0xB7; // 1 byte of adaptation data, but require length of 183 when no payload is indicated
			discontinuityPacket[i + 5] = 0x10; // PCR
			writePCR(&discontinuityPacket[i + 6], insertPCR, true);
			for (int i = 6 + 6 + m_ttsSize; i < m_packetSize; i++)
			{
				discontinuityPacket[i] = 0xFF;
			}
			m_continuityCounters[discPid] = 0x01;

			AAMPLOG_TRACE("supply new pcr value");

			if (!m_demux)
			{
				aamp->SendStreamCopy((AampMediaType)m_track, discontinuityPacket, m_packetSize, position, position, 0);
			}
		}
	}
	m_needDiscontinuity = false;
}

/**
 * @brief Get current time stamp in milliseconds
 * @retval time stamp in milliseconds
 */
long long TSProcessor::getCurrentTime()
{
	struct timeval tv;
	long long currentTime;

	gettimeofday(&tv, 0);

	currentTime = (((unsigned long long)tv.tv_sec) * 1000 + ((unsigned long long)tv.tv_usec) / 1000);

	return currentTime;
}

/**
 * @brief sleep used internal by throttle logic
 */
bool TSProcessor::msleep(long long throttleDiff)
{
	bool aborted = false;
	std::unique_lock<std::mutex> lock(m_mutex);
	if( m_enabled)
	{
		(void)m_throttleCond.wait_for(lock, std::chrono::milliseconds(throttleDiff));
	}
	else
	{
		aborted = true;
	}
	return aborted;
}

/**
 * @brief Blocks based on PTS. Can be used for pacing injection.
 * @retval true if aborted
 */
bool TSProcessor::throttle()
{
	bool aborted = false;
	// If throttling is enabled, regulate output data rate
	if (m_throttle)
	{
		// Track content time via PTS values compared to real time and don't
		// let data get more than 200 ms ahead of real time.
		long long contentTime = ((m_playRate != 1.0) ? m_throttlePTS.value : m_actualStartPTS.value);
		if (contentTime != -1LL)
		{
			long long now, contentTimeDiff, realTimeDiff;
			AAMPLOG_TRACE("contentTime %lld (%lld ms) m_playRate %f", contentTime, contentTime / 90, m_playRate);

			// Convert from 90KHz milliseconds
			contentTime = (contentTime / 90LL);

			now = getCurrentTime();
			if (m_haveThrottleBase)
			{

				if (m_lastThrottleContentTime != -1LL)
				{
					contentTimeDiff = contentTime - m_lastThrottleContentTime;
					realTimeDiff = now - m_lastThrottleRealTime;
					if (((contentTimeDiff > 0) && (contentTimeDiff < m_throttleMaxDiffSegments)) && ((realTimeDiff > 0) && (realTimeDiff < m_throttleMaxDiffSegments)))
					{
						contentTimeDiff = contentTime - m_baseThrottleContentTime;
						realTimeDiff = now - m_baseThrottleRealTime;
						if ((realTimeDiff > 0) && (realTimeDiff < contentTimeDiff))
						{
							long long throttleDiff = (contentTimeDiff - realTimeDiff - m_throttleDelayIgnoredMs);
							if (throttleDiff > 0)
							{
								if (throttleDiff > m_throttleMaxDelayMs)
								{
									// Don't delay more than 500 ms in any given request
									AAMPLOG_TRACE("TSProcessor::fillBuffer: throttle: cap %lld to %d ms", throttleDiff, m_throttleMaxDelayMs);
									throttleDiff = m_throttleMaxDelayMs;
								}
								else
								{
									AAMPLOG_TRACE("TSProcessor::fillBuffer: throttle: sleep %lld ms", throttleDiff);
								}
								aborted = msleep(throttleDiff);
							}
							else
							{
								AAMPLOG_INFO("throttleDiff negative? %lld", throttleDiff);
							}
						}
						else
						{
							AAMPLOG_TRACE("realTimeDiff %lld", realTimeDiff);
						}
					}
					else if ((contentTimeDiff < -m_throttleDelayForDiscontinuityMs) || (contentTimeDiff > m_throttleDelayForDiscontinuityMs))
					{
						// There has been some timing irregularity such as a PTS discontinuity.
						// Establish a new throttling time base.
						m_haveThrottleBase = false;
						AAMPLOG_INFO(" contentTimeDiff( %lld) greater than threshold (%d) - probable pts discontinuity", contentTimeDiff, m_throttleDelayForDiscontinuityMs);
					}
					else
					{
						AAMPLOG_INFO(" Out of throttle window - contentTimeDiff %lld realTimeDiff  %lld", contentTimeDiff, realTimeDiff);
					}
				}
				else
				{
					AAMPLOG_TRACE(" m_lastThrottleContentTime %lld", m_lastThrottleContentTime);
				}
			}
			else
			{
				AAMPLOG_INFO("Do not have ThrottleBase");
			}

			if (!m_haveThrottleBase)
			{
				m_haveThrottleBase = true;
				m_baseThrottleRealTime = now;
				m_baseThrottleContentTime = contentTime;
			}
			m_lastThrottleRealTime = now;
			m_lastThrottleContentTime = contentTime;
		}
		else if (m_demux && (1.0 != m_playRate))
		{
			if (m_last_frame_time)
			{
				long long now = getCurrentTime();
				long long nextFrameTime = m_last_frame_time + 1000 / m_apparentFrameRate;
				if (nextFrameTime > now)
				{
					long long throttleDiff = nextFrameTime - now;
					AAMPLOG_INFO("Wait %llu ms ", throttleDiff);
					aborted = msleep(throttleDiff);
				}
			}
			m_last_frame_time = getCurrentTime();
		}
		else
		{
			AAMPLOG_INFO("contentTime not updated yet");
		}
	}
	else
	{
		AAMPLOG_TRACE("Throttle not enabled");
	}
	AAMPLOG_TRACE("Exit");
	return aborted;
}

/**
 * @brief Process buffers and update internal states related to media components
 * @retval false if operation is aborted.
 */
bool TSProcessor::processBuffer(unsigned char *buffer, int size, bool &insPatPmt, bool discontinuity_pending)
{
	bool result = true;
	unsigned char *packet, *bufferEnd;
	int pid, payloadStart, adaptation, payloadOffset;
	int continuity, scramblingControl;
	int packetCount = 0;
	insPatPmt = false;
	bool removePatPmt = false;
	m_packetStartAfterFirstPTS = -1;

	if (m_playRate != 1.0)
	{
		insPatPmt = true;
		removePatPmt = true;
		AAMPLOG_INFO("Replace PAT/PMT");
	}

	bool doThrottle = m_throttle;

	/*m_actualStartPTS stores the pts of  segment which will be used by throttle*/
	m_actualStartPTS = -1LL;

	packet = buffer + m_ttsSize;

	// For the moment, insist on buffers being TS packet aligned
	if (!((packet[0] == 0x47) && ((size%m_packetSize) == 0)))
	{
		AAMPLOG_ERR("Error: data buffer not TS packet aligned");
		AAMPLOG_WARN("packet=%p size=%d m_packetSize=%d", packet, size, m_packetSize);
		assert(false);
	}

	if ( discontinuity_pending || ((m_streamOperation == eStreamOp_DEMUX_ALL) && (ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp))) )
	{
		// HACK - PAT/PMT can change across HLS discontinuity with PTS Restamp enabled ; without this, our test asset ends up losing audio during 2nd period
		AAMPLOG_INFO(" Discontinuity pending, resetting m_havePAT & m_havePMT");

		std::lock_guard<std::mutex> guard(m_mutex);
		m_havePAT = false;
		m_havePMT = false;
	}

	bufferEnd = packet + size - m_ttsSize;
	while (packet < bufferEnd)
	{
		pid = (((packet[1] << 8) | packet[2]) & 0x1FFF);
		AAMPLOG_TRACE("pid = %d, m_ttsSize %d", pid, m_ttsSize);

		if (m_checkContinuity)
		{
			if ((pid != 0x1FFF) && (packet[3] & 0x10))
			{
				continuity = (packet[3] & 0x0F);
				int expected = m_continuityCounters[pid];
				expected = ((expected + 1) & 0xF); //CID:94829 - No effect
				if (expected != continuity)
				{
					AAMPLOG_WARN("input SPTS discontinuity on pid %X (%d instead of %d) offset %llx",
						pid, continuity, expected, (long long)(packetCount*m_packetSize));
				}

				m_continuityCounters[pid] = continuity;
			}
		}

		if (pid == 0)
		{
			adaptation = ((packet[3] & 0x30) >> 4);
			if (adaptation & 0x01)
			{
				payloadOffset = 4;

				if (adaptation & 0x02)
				{
					payloadOffset += (1 + packet[4]);
				}

				payloadStart = (packet[1] & 0x40);
				if (payloadStart)
				{
					int tableid = packet[payloadOffset + 1];
					if (tableid == 0x00)
					{
						int version = packet[payloadOffset + 6];
						int current = (version & 0x01);
						version = ((version >> 1) & 0x1F);

						AAMPLOG_TRACE("PAT current version %d existing version %d", version, m_versionPAT);
						if (!m_havePAT || (current && (version != m_versionPAT)))
						{
							int length = ((packet[payloadOffset + 2] & 0x0F) << 8) + (packet[payloadOffset + 3]);

							if (length >= PAT_SPTS_SIZE)
							{
								int patTableIndex = payloadOffset + 9; 				// PAT table start
								int patTableEndIndex = payloadOffset + length -1; 	// end of PAT table
								uint32_t computed_crc = aamp_ComputeCRC32( &packet[payloadOffset+1], length-1 );
								if( // compare to expected crc as packed after payload
								   ((computed_crc>>0x18)&0xff) != packet[payloadOffset+length+0] ||
								   ((computed_crc>>0x10)&0xff) != packet[payloadOffset+length+1] ||
								   ((computed_crc>>0x08)&0xff) != packet[payloadOffset+length+2] ||
								   ((computed_crc>>0x00)&0xff) != packet[payloadOffset+length+3] )
								{
									AAMPLOG_WARN( "ignoring corrupt PAT with invalid CRC" );
								}
								else
								{
									{
										std::lock_guard<std::mutex> guard(m_mutex);
										m_havePAT = true;
										m_versionPAT = version;
									}
									do {
										m_program = ((packet[patTableIndex + 0] << 8) + packet[patTableIndex + 1]);
										m_pmtPid = (((packet[patTableIndex + 2] & 0x1F) << 8) + packet[patTableIndex + 3]);

										patTableIndex += PAT_TABLE_ENTRY_SIZE;
										// Find first program number not 0 or until end of PAT
									} while (m_program == 0 && patTableIndex < patTableEndIndex);

									if ((m_program != 0) && (m_pmtPid != 0))
									{
										if (length > PAT_SPTS_SIZE)
										{
											AAMPLOG_WARN("RecordContext: PAT is MPTS, using program %d.", m_program);
										}
										if (m_havePMT)
										{
											AAMPLOG_INFO("RecordContext: pmt change detected in pat");
											m_havePMT = false;
											goto done;
										}
										m_havePMT = false;
										AAMPLOG_DEBUG("RecordContext: acquired PAT version %d program %X pmt pid %X", version, m_program, m_pmtPid);
									}
									else
									{
										AAMPLOG_WARN("Warning: RecordContext: ignoring pid 0 TS packet with suspect program %x and pmtPid %x", m_program, m_pmtPid);
										m_program = -1;
										m_pmtPid = -1;
									}
								}
							}
							else
							{
								AAMPLOG_WARN("Warning: RecordContext: ignoring pid 0 TS packet with length of %d - not SPTS?", length);
							}
						}
					}
					else
					{
						AAMPLOG_WARN("Warning: RecordContext: ignoring pid 0 TS packet with tableid of %x", tableid);
					}
				}
				else
				{
					AAMPLOG_WARN("Warning: RecordContext: ignoring pid 0 TS packet with adaptation of %x", adaptation);
				}
			}
			else
			{
				AAMPLOG_WARN("Warning: RecordContext: ignoring pid 0 TS with no payload indicator");
			}
			/*For trickmodes, remove PAT/PMT*/
			if (removePatPmt)
			{
				// Change to null packet
				packet[1] = ((packet[1] & 0xE0) | 0x1F);
				packet[2] = 0xFF;
			}
		}
		else if (pid == m_pmtPid)
		{
			AAMPLOG_TRACE("Got PMT : m_pmtPid %d", m_pmtPid);
			adaptation = ((packet[3] & 0x30) >> 4);
			if (adaptation & 0x01)
			{
				payloadOffset = 4;

				if (adaptation & 0x02)
				{
					payloadOffset += (1 + packet[4]);
				}

				payloadStart = (packet[1] & 0x40);
				if (payloadStart)
				{
					int tableid = packet[payloadOffset + 1];
					if (tableid == 0x02)
					{
						int program = ((packet[payloadOffset + 4] << 8) + packet[payloadOffset + 5]);
						if (program == m_program)
						{
							int version = packet[payloadOffset + 6];
							int current = (version & 0x01);
							version = ((version >> 1) & 0x1F);

							AAMPLOG_TRACE("PMT current version %d existing version %d", version, m_versionPMT);
							if (!m_havePMT || (current && (version != m_versionPMT)))
							{
								if (m_havePMT && (version != m_versionPMT))
								{
									AAMPLOG_INFO("RecordContext: pmt change detected: version %d -> %d", m_versionPMT, version);
									m_havePMT = false;
									goto done;
								}

								if (!m_havePMT)
								{
									int sectionLength = (((packet[payloadOffset + 2] & 0x0F) << 8) + packet[payloadOffset + 3]);
									// Check if pmt payload fits in one TS packet:
									// section data starts after pointer_field (1), tableid (1), and section length (2)
									if (payloadOffset + 4 + sectionLength <= m_packetSize - m_ttsSize)
									{
										processPMTSection(packet + payloadOffset + 4, sectionLength);
									}
									else if (sectionLength <= MAX_PMT_SECTION_SIZE)
									{
										if (!m_pmtCollector)
										{
											m_pmtCollector = (unsigned char*)malloc(MAX_PMT_SECTION_SIZE);
											if (!m_pmtCollector)
											{
												AAMPLOG_ERR("Error: unable to allocate pmt collector buffer - ignoring large pmt (section length %d)", sectionLength);
												goto done;
											}
											AAMPLOG_INFO("RecordContext: allocating pmt collector buffer %p", m_pmtCollector);
										}
										// section data starts after table id, section length and pointer field
										int sectionOffset = payloadOffset + 4;
										int sectionAvail = m_packetSize - m_ttsSize - sectionOffset;
										unsigned char *sectionData = packet + sectionOffset;
										memcpy(m_pmtCollector, sectionData, sectionAvail);
										m_pmtCollectorSectionLength = sectionLength;
										m_pmtCollectorOffset = sectionAvail;
										m_pmtCollectorNextContinuity = ((packet[3] + 1) & 0xF);
										AAMPLOG_INFO("RecordContext: starting to collect multi-packet pmt: section length %d", sectionLength);
									}
									else
									{
										AAMPLOG_WARN("Warning: RecordContext: ignoring oversized pmt (section length %d)", sectionLength);
									}
								}
							}
						}
						else
						{
							AAMPLOG_WARN("Warning: RecordContext: ignoring pmt TS packet with mismatched program of %x (expecting %x)", program, m_program);
						}
					}
					else
					{
						AAMPLOG_TRACE("Warning: RecordContext: ignoring pmt TS packet with tableid of %x", tableid);
					}
				}
				else
				{
					// process subsequent parts of multi-packet pmt
					if (m_pmtCollectorOffset)
					{
						int continuity = (packet[3] & 0xF);
						if (((continuity + 1) & 0xF) == m_pmtCollectorNextContinuity)
						{
							AAMPLOG_WARN("Warning: RecordContext: next packet of multi-packet pmt has wrong continuity count %d (expecting %d)",
								m_pmtCollectorNextContinuity, continuity);
							// Assume continuity counts for all packets of this pmt will remain the same.
							// Allow this since it has been observed in the field
							m_pmtCollectorNextContinuity = continuity;
						}
						if (continuity == m_pmtCollectorNextContinuity)
						{
							int avail = m_packetSize - m_ttsSize - payloadOffset;
							int sectionAvail = m_pmtCollectorSectionLength - m_pmtCollectorOffset;
							int copylen = ((avail > sectionAvail) ? sectionAvail : avail);
							if(m_pmtCollector)
							{    //CID:87880 - forward null
								memcpy(&m_pmtCollector[m_pmtCollectorOffset], &packet[payloadOffset], copylen);
							}
							m_pmtCollectorOffset += copylen;
							if (m_pmtCollectorOffset == m_pmtCollectorSectionLength)
							{
								processPMTSection(m_pmtCollector, m_pmtCollectorSectionLength);
								m_pmtCollectorOffset = 0;
							}
							else
							{
								m_pmtCollectorNextContinuity = ((continuity + 1) & 0xF);
							}
						}
						else
						{
							AAMPLOG_ERR("Error: RecordContext: aborting multi-packet pmt due to discontinuity error: expected %d got %d",
								m_pmtCollectorNextContinuity, continuity);
							m_pmtCollectorOffset = 0;
						}
					}
					else if (!m_havePMT)
					{
						AAMPLOG_WARN("Warning: RecordContext: ignoring unexpected pmt TS packet without payload start indicator");
					}
				}
			}
			else
			{
				AAMPLOG_WARN("Warning: RecordContext: ignoring unexpected pmt TS packet without payload indicator");
			}
			/*For trickmodes, remove PAT/PMT*/
			if (removePatPmt)
			{
				// Change to null packet
				packet[1] = ((packet[1] & 0xE0) | 0x1F);
				packet[2] = 0xFF;
			}
		}
		else if ((pid == m_videoPid) || (pid == m_pcrPid))
		{

			if ((m_actualStartPTS == -1LL) && doThrottle)
			{
				payloadOffset = 4;
				adaptation = ((packet[3] & 0x30) >> 4);
				payloadStart = (packet[1] & 0x40);

				scramblingControl = ((packet[3] & 0xC0) >> 6);
				if (scramblingControl)
				{
					if (!m_scrambledWarningIssued)
					{
						AAMPLOG_TRACE("RecordingContext: video pid %x transport_scrambling_control bits are non-zero (%02x)- is data still scrambled?", pid, packet[3]);
						m_scrambledWarningIssued = true;
						AAMPLOG_TRACE("[found scrambled data, NOT writing idx,mpg files, returning(true)... ");
					}
				}
				m_scrambledWarningIssued = false;

				if (adaptation & 0x02)
				{
					payloadOffset += (1 + packet[4]);
				}

				if (adaptation & 0x01)
				{
					if (payloadStart)
					{
						if ((packet[payloadOffset] == 0x00) && (packet[payloadOffset + 1] == 0x00) && (packet[payloadOffset + 2] == 0x01))
						{
							int pesHeaderDataLen = packet[payloadOffset + 8];
							if (packet[payloadOffset + 7] & 0x80)
							{
								// PTS
								uint33_t PTS;
								bool validPTS;
								{
									long long result;
									validPTS = readTimeStamp(&packet[payloadOffset + 9], result);
									PTS = result;
								}

								if (validPTS)
								{

									m_packetStartAfterFirstPTS = (int)((packet - buffer) + PACKET_SIZE);
									uint33_t diffPTS {0};

									constexpr auto tenSecAsPTS = uint33_t{10ULL * 90000ULL};
									constexpr auto twelveSecAsPTS = uint33_t{12ULL * 90000ULL};

									if (m_actualStartPTS != uint33_t::max_value())
									{

										diffPTS = PTS - m_currentPTS;
									}

									// Consider a difference of 10 or more seconds a discontinuity
									if (diffPTS > tenSecAsPTS)
									{
										if ((diffPTS < twelveSecAsPTS))// && (m_recording->getGOPSize(0) < 4) )
										{
											// This is a music channel with a slow rate of video frames
											// so ignore the PTS jump
										}
										else
										{
											AAMPLOG_INFO("RecordContext: pts discontinuity: %" PRIu64 " to %" PRIu64, m_currentPTS.value, PTS.value);
											m_currentPTS = PTS;
										}
									}
									else
									{
										m_currentPTS = PTS;
										if (m_actualStartPTS == uint33_t::max_value())
										{
											m_actualStartPTS = PTS;
											AAMPLOG_TRACE("Updated m_actualStartPTS to %" PRIu64, m_actualStartPTS.value);
										}
									}

									m_haveFirstPTS = true;
								}
							}
							payloadOffset = payloadOffset + 9 + pesHeaderDataLen;
						}
					}
				}

				(void)payloadOffset; // Avoid a warning as the last value set may not be used.
			}
			if (doThrottle)
			{
				if (throttle())
				{
					AAMPLOG_INFO("throttle aborted");
					m_haveThrottleBase = false;
					m_lastThrottleContentTime = -1;
					result = false;
					break;
				}
				doThrottle = false;
			}
			if (1.0 != m_playRate && !m_demux)
			{
				/*reTimestamp updates packet, so pass a dummy variable*/
				unsigned char* tmpPacket = packet;
				reTimestamp(tmpPacket, m_packetSize);
			}
		}
		/*For trickmodes, keep only video pid*/
		else if (m_playRate != 1.0)
		{
			// Change to null packet
			packet[1] = ((packet[1] & 0xE0) | 0x1F);
			packet[2] = 0xFF;
		}

	done:
		packet += m_packetSize;
		++packetCount;
	}

	return result;
}

/**
 * @brief Update internal state variables to set up throttle
 */
void TSProcessor::setupThrottle(int segmentDurationMsSigned)
{
	int segmentDurationMs = abs(segmentDurationMsSigned);
	m_throttleMaxDelayMs = segmentDurationMs + DEFAULT_THROTTLE_DELAY_IGNORED_MS;
	m_throttleMaxDiffSegments = m_throttleMaxDelayMs;
	m_throttleDelayIgnoredMs = DEFAULT_THROTTLE_DELAY_IGNORED_MS;
	m_throttleDelayForDiscontinuityMs = segmentDurationMs * 10;
	AAMPLOG_TRACE("segmentDurationMs %d", segmentDurationMs);
}

/**
 * @brief Demux TS and send elementary streams
 * @retval true on success, false on PTS error
 */
bool TSProcessor::demuxAndSend(const void *ptr, size_t len, double position, double duration, bool discontinuous, MediaProcessor::process_fcn_t processor, TrackToDemux trackToDemux)
{
	int videoPid = -1, audioPid = -1, dsmccPid = -1;
	unsigned long long firstPcr = 0;
	bool isTrickMode = !( 1.0 == m_playRate);
	bool notifyPeerBasePTS = false;
	bool ret = true;
	bool basePtsUpdatedFromCurrentSegment = false;
	bool optimizeMuxed = false;

	if( aamp && ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp))
	{
		optimizeMuxed = (m_streamOperation == eStreamOp_DEMUX_ALL);
	}

	if (m_vidDemuxer && ((trackToDemux == ePC_Track_Both) || (trackToDemux == ePC_Track_Video)))
	{
		if (videoComponentCount > 0)
		{
			videoPid = videoComponents[0].pid;
		}
		if(m_dsmccComponentFound)
		{
			dsmccPid = m_dsmccComponent.pid;
		}
		if (discontinuous || !m_demuxInitialized )
		{
			if (discontinuous && (1.0 == m_playRate))
			{
				AAMPLOG_INFO("TSProcessor:%p discontinuous buffer- flushing video demux", this);
			}
			m_vidDemuxer->flush();
			m_vidDemuxer->init(position, duration, isTrickMode, true, optimizeMuxed );
			m_dsmccDemuxer->flush();
			m_dsmccDemuxer->init(position, duration, isTrickMode, true, optimizeMuxed);
		}
	}
	if (m_audDemuxer && ((trackToDemux == ePC_Track_Both) || (trackToDemux == ePC_Track_Audio)))
	{
		if (audioComponentCount > 0)
		{
			audioPid = audioComponents[m_AudioTrackIndexToPlay].pid;
		}
		if (discontinuous || !m_demuxInitialized )
		{
			if(discontinuous)
			{
				AAMPLOG_INFO("TSProcessor:%p discontinuous buffer- flushing audio demux", this);
			}
			m_audDemuxer->flush();
			m_audDemuxer->init(position, duration, isTrickMode, (eStreamOp_DEMUX_AUDIO != m_streamOperation), optimizeMuxed );
		}
	}

	/*In the case of audio demux only operation, base_pts is already set before this function is called*/
	if (eStreamOp_DEMUX_AUDIO == m_streamOperation )
	{
		m_demuxInitialized = true;
	}
	AAMPLOG_INFO("demuxAndSend : len  %d videoPid %d audioPid %d m_pcrPid %d videoComponentCount %d m_demuxInitialized = %d", (int)len, videoPid, audioPid, m_pcrPid, videoComponentCount, m_demuxInitialized);

	std::unordered_set<Demuxer*> updated_demuxers{};
	const unsigned char * packetStart = (const unsigned char *)ptr;
	while (len >= PACKET_SIZE)
	{
		Demuxer* demuxer = NULL;
		int pid = (packetStart[1] & 0x1f) << 8 | packetStart[2];
		bool dsmccDemuxerUsed = false;

		if (m_vidDemuxer && (pid == videoPid))
		{
			demuxer = m_vidDemuxer;
		}
		else if (m_audDemuxer && (pid == audioPid))
		{
			demuxer = m_audDemuxer;
		}
		else if(m_dsmccDemuxer && (pid == dsmccPid))
		{
			demuxer = m_dsmccDemuxer;
			dsmccDemuxerUsed = true;
		}

		if ((discontinuous || !m_demuxInitialized ) && !firstPcr && (pid == m_pcrPid))
		{
			int adaptation_fieldlen = 0;
			if ((packetStart[3] & 0x20) == 0x20)
			{
				adaptation_fieldlen = packetStart[4];
				if (0 != adaptation_fieldlen && (packetStart[5] & 0x10))
				{
					firstPcr = (unsigned long long) packetStart[6] << 25 | (unsigned long long) packetStart[7] << 17
						| (unsigned long long) packetStart[8] << 9 | packetStart[9] << 1 | (packetStart[10] & (0x80)) >> 7;
  //CID:98793 - Uncaught expression results
					if (m_playRate == 1.0)
					{
						AAMPLOG_INFO("firstPcr %llu", firstPcr);
					}
					if (m_vidDemuxer)
					{
						m_vidDemuxer->setBasePTS(firstPcr, false);
					}
					if (m_audDemuxer)
					{
						m_audDemuxer->setBasePTS(firstPcr, false);
					}
					if (m_dsmccDemuxer)
					{
						m_dsmccDemuxer->setBasePTS(firstPcr, true);
					}
					notifyPeerBasePTS = true;
					m_demuxInitialized = true;
				}
			}
		}

		if (demuxer)
		{
			bool ptsError = false;  //CID:87386 , 86687 - Initialization
			bool  basePTSUpdated = false;
			bool isPacketIgnored = false;
			demuxer->processPacket(packetStart, basePTSUpdated, ptsError, isPacketIgnored, m_applyOffset, processor);

			/* Audio is not playing in particular hls file.
			 * We always choose the first audio pid to play the audio data, even if we
			 * have multiple audio tracks in the PMT Table.
			 * But in one particular hls file, we dont have PES data in the first audio pid.
			 * So, we have now modified to choose the next available audio pid index,
			 * when there is no PES data available in the current audio pid.
			 */
			if( ( demuxer == m_audDemuxer ) && isPacketIgnored )
			{
				if( (audioComponentCount > 0) && (m_AudioTrackIndexToPlay < audioComponentCount-1) )
				{
					m_AudioTrackIndexToPlay++;
					AAMPLOG_WARN("Switched to next audio pid, since no PES data in current pid");
				}
			}

			// Process PTS updates and errors only for audio and video demuxers
			if(!m_demuxInitialized && !dsmccDemuxerUsed)
			{
				AAMPLOG_WARN("PCR not available before ES packet, updating firstPCR");
				m_demuxInitialized = true;
				notifyPeerBasePTS = true;
				firstPcr = demuxer->getBasePTS();
			}

			if( basePTSUpdated && notifyPeerBasePTS && !dsmccDemuxerUsed)
			{
				if (m_audDemuxer && (m_audDemuxer != demuxer) && (eStreamOp_DEMUX_AUDIO != m_streamOperation))
				{
					AAMPLOG_INFO("Using first video pts as base pts");
					m_audDemuxer->setBasePTS(demuxer->getBasePTS(), true);
				}
				else if (m_vidDemuxer && (m_vidDemuxer != demuxer) && (eStreamOp_DEMUX_VIDEO != m_streamOperation))
				{
					AAMPLOG_WARN("Using first audio pts as base pts");
					m_vidDemuxer->setBasePTS(demuxer->getBasePTS(), true);
				}

				if (m_dsmccDemuxer)
				{
					m_dsmccDemuxer->setBasePTS(demuxer->getBasePTS(), true);
				}

				if(m_peerTSProcessor)
				{
					m_peerTSProcessor->setBasePTS( position, demuxer->getBasePTS());
				}
				if(m_auxTSProcessor)
				{
					m_auxTSProcessor->setBasePTS(position, demuxer->getBasePTS());
				}
				notifyPeerBasePTS = false;
			}
			if (ptsError && !dsmccDemuxerUsed && !basePtsUpdatedFromCurrentSegment)
			{
				AAMPLOG_WARN("PTS error, discarding segment");
				ret = false;
				break;
			}
			if (basePTSUpdated)
			{
				basePtsUpdatedFromCurrentSegment = true;
			}

			// Caching the demuxer pointer for later use
			updated_demuxers.emplace(demuxer);
		}
		else
		{
			if((pid == 0)||(pid == m_pmtPid))
			{
				AAMPLOG_TRACE("demuxAndSend : discarded PAT or PMT packet with pid %d", pid);
			}
			else
			{
				AAMPLOG_INFO("demuxAndSend : discarded packet with pid %d", pid);
			}
		}

		packetStart += PACKET_SIZE;
		len -= PACKET_SIZE;
	}

	for (auto demuxer : updated_demuxers)
	{
		if (demuxer && demuxer->HasCachedData())
		{
			demuxer->ConsumeCachedData(processor);
		}
	}

	return ret;
}

/**
 * @brief Reset TS processor state
 */
void TSProcessor::reset()
{
	AAMPLOG_INFO("PC reset");
	std::lock_guard<std::mutex> guard(m_mutex);
	if (m_vidDemuxer)
	{
		AAMPLOG_WARN("TSProcessor[%p] - reset video demux %p", this, m_vidDemuxer);
		m_vidDemuxer->reset();
	}

	if (m_audDemuxer)
	{
		AAMPLOG_WARN("TSProcessor[%p] - reset audio demux %p", this, m_audDemuxer);
		m_audDemuxer->reset();
	}
	m_enabled = true;
	m_demuxInitialized = false;
	m_basePTSFromPeer = -1;
	m_havePAT = false;
	m_havePMT = false;
	m_AudioTrackIndexToPlay = 0;
}

/**
 * @brief Flush all buffered data to sink
 * @note Relevant only when s/w demux is used
 */
void TSProcessor::flush()
{
	AAMPLOG_INFO("PC flush");
	std::lock_guard<std::mutex> guard(m_mutex);
	if (m_vidDemuxer)
	{
		AAMPLOG_WARN("TSProcessor[%p] flush video demux %p", this, m_vidDemuxer);
		m_vidDemuxer->flush();
	}

	if (m_audDemuxer)
	{
		AAMPLOG_WARN("TSProcessor[%p] flush audio demux %p", this, m_audDemuxer);
		m_audDemuxer->flush();
	}

	if (m_dsmccDemuxer)
	{
		AAMPLOG_WARN("TSProcessor[%p] flush dsmcc demux %p", this, m_dsmccDemuxer);
		m_dsmccDemuxer->flush();
	}
}

/**
 * @brief Send queued segment
 */
void TSProcessor::sendQueuedSegment(long long basepts, double updatedStartPosition)
{
	AAMPLOG_WARN("PC %p basepts %lld", this, basepts);
	std::lock_guard<std::mutex> guard(m_mutex);
	if (m_queuedSegment)
	{
		if (-1 != updatedStartPosition)
		{
			AAMPLOG_DEBUG("Update position from %f to %f", m_queuedSegmentPos, updatedStartPosition);
			m_queuedSegmentPos = updatedStartPosition;
		}
		else if (eStreamOp_DEMUX_AUDIO == m_streamOperation)
		{
			if(basepts)
			{
				m_audDemuxer->setBasePTS(basepts, true);
			}

			MediaProcessor::process_fcn_t processor = [this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
				aamp->SendStreamCopy(type, buf.data(), buf.size(), info.pts_s, info.dts_s, info.duration);
			};

			if(!demuxAndSend(m_queuedSegment, m_queuedSegmentLen, m_queuedSegmentPos, m_queuedSegmentDuration, m_queuedSegmentDiscontinuous, processor))
			{
				AAMPLOG_WARN("demuxAndSend");  //CID:90622- checked return
			}
		}
		else
		{
			AAMPLOG_ERR("sendQueuedSegment invoked in Invalid stream operation");
		}
		free(m_queuedSegment);
		m_queuedSegment = NULL;
	}
	else
	{
		AAMPLOG_WARN("No pending buffer");
	}
}

/**
 * @brief set base PTS for demux operations
 */
void TSProcessor::setBasePTS(double position, long long pts)
{
	//std::lock_guard<std::mutex> lock(m_mutex);
	std::unique_lock<std::mutex> lock(m_mutex);
	m_basePTSFromPeer = pts;
	m_startPosition = position;
	AAMPLOG_INFO("pts = %lld", pts);
	if (m_audDemuxer)
	{
		bool optimizeMuxed = false;
		m_audDemuxer->flush();
		m_audDemuxer->init(position, 0, false, true, optimizeMuxed);
		m_audDemuxer->setBasePTS(pts, true);
		m_demuxInitialized = true;
	}
	m_basePTSCond.notify_one();
}

#include "test/gstTestHarness/tsdemux.hpp"

/**
 * @brief given TS media segment (not yet injected), extract and report first PTS
 */
double TSProcessor::getFirstPts( AampGrowableBuffer* pBuffer )
{
	double firstPts = 0.0;
	auto tsDemux = new TsDemux( eMEDIATYPE_VIDEO, pBuffer->GetPtr(), pBuffer->GetLen(), true );
	if( tsDemux )
	{
		firstPts = tsDemux->getPts(0);
		delete tsDemux;
	}
	return firstPts;
}

/**
 * @brief optionally specify new pts offset to apply for subsequently injected TS media segments
 */
void TSProcessor::setPtsOffset( double ptsOffset )
{
	if( m_vidDemuxer )
	{
		m_vidDemuxer->setPtsOffset( ptsOffset );
	}
	if( m_audDemuxer )
	{
		m_audDemuxer->setPtsOffset( ptsOffset );
	}
}

/**
 * @brief Does configured operation on the segment and injects data to sink
 *        Process and send media fragment
 */
bool TSProcessor::sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
								bool isInit, process_fcn_t processor, bool &ptsError)
{
	bool insPatPmt = false;  //CID:84507 - Initialization
	unsigned char * packetStart;
	char *segment = pBuffer->GetPtr();
	int len = (int)(pBuffer->GetLen());
	bool ret = false;
	ptsError = false;
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		if (!m_enabled)
		{
			AAMPLOG_INFO("Not Enabled, Returning");
			return false;
		}
		m_processing = true;
		if ((m_playModeNext != m_playMode) || (m_playRateNext != m_playRate))
		{
			AAMPLOG_TRACE("change play mode");
			m_playMode = m_playModeNext;

			m_playRate = m_playRateNext;
			m_absPlayRate = fabs(m_playRate);
			AAMPLOG_INFO("playback changed to rate %f mode %d", m_playRate, m_playMode);
			m_haveEmittedIFrame = false;
			m_currFrameOffset = -1LL;
			m_nullPFrameNextCount = 0;
			m_needDiscontinuity = true;
		}
	}
	m_framesProcessedInSegment = 0;
	m_lastPTSOfSegment = -1;
	packetStart = (unsigned char *)segment;

	// It seems some ts have an invalid packet at the start, so try skipping it
	while (((packetStart[0] != 0x47) || ((packetStart[1] & 0x80) != 0x00) || ((packetStart[3] & 0xC0) != 0x00)) && (len > 188))
	{
		packetStart += 188; // Just jump a packet
		len -= 188;
		if ((((char *)packetStart - segment) > 376) ||
			(len < 188))
		{
			AAMPLOG_ERR("No valid ts packet found near the start of the segment");
			packetStart = (unsigned char *)segment;
			len = (int)(pBuffer->GetLen());
			break;
		}
	}

	if ((packetStart[0] != 0x47) || ((packetStart[1] & 0x80) != 0x00) || ((packetStart[3] & 0xC0) != 0x00))
	{
		AAMPLOG_ERR("Segment doesn't starts with valid TS packet, discarding.");
		std::lock_guard<std::mutex> guard(m_mutex);
		m_processing = false;
		m_throttleCond.notify_one();
		return false;
	}
	if (len % m_packetSize)
	{
		int discardAtEnd = len % m_packetSize;
		AAMPLOG_INFO("Discarding %d bytes at end", discardAtEnd);
		len = len - discardAtEnd;
	}
	ret = processBuffer((unsigned char*)packetStart, len, insPatPmt, discontinuous);
	if (ret)
	{
		if (-1.0 == m_startPosition)
		{
			AAMPLOG_INFO("Reset m_startPosition to %f", position);
			m_startPosition = position;
		}
		double updatedPosition = (position - m_startPosition) / m_playRate;
		AAMPLOG_DEBUG("updatedPosition = %f Position = %f m_startPosition = %f m_playRate = %f", updatedPosition, position, m_startPosition, m_playRate);
		position = updatedPosition;

		if (m_needDiscontinuity&& !m_demux)
		{
			sendDiscontinuity(position);
		}
		if (insPatPmt && !m_demux)
		{
			unsigned char *sec = (unsigned char *)malloc(PATPMT_MAX_SIZE);
			if (NULL != sec)
			{
				updatePATPMT();
				int secSize = insertPatPmt(sec, (m_playMode != PlayMode_normal), PATPMT_MAX_SIZE);
				aamp->SendStreamCopy((AampMediaType)m_track, sec, secSize, position, position, 0);
				free(sec);
				AAMPLOG_TRACE("Send PAT/PMT");
			}
		}
		if (m_demux)
		{
			if (eStreamOp_DEMUX_AUDIO == m_streamOperation)
			{
				if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					if (-1 == m_basePTSFromPeer)
					{
						if (m_enabled)
						{
							AAMPLOG_WARN("TSProcessor[%p] wait for base PTS. m_audDemuxer %p", this, m_audDemuxer);
							m_basePTSCond.wait(lock);
						}

						if (!m_enabled)
						{
							AAMPLOG_INFO("Not Enabled, Returning");
							m_processing = false;
							m_throttleCond.notify_one();
							return false;
						}
						AAMPLOG_WARN("TSProcessor[%p] got base PTS. m_audDemuxer %p", this, m_audDemuxer);
					}
				}
				ret = demuxAndSend(packetStart, len, m_startPosition, duration, discontinuous, std::move(processor));
			}
			else if(!ISCONFIGSET(eAAMPConfig_DemuxAudioBeforeVideo))
			{
				ret = demuxAndSend(packetStart, len, position, duration, discontinuous, std::move(processor));
			}
			else
			{
				AAMPLOG_WARN("Sending Audio First");
				ret = demuxAndSend(packetStart, len, position, duration, discontinuous, processor, ePC_Track_Audio);
				ret |= demuxAndSend(packetStart, len, position, duration, discontinuous, std::move(processor), ePC_Track_Video);
			}
			ptsError = !ret;
		}
		else
		{
			aamp->SendStreamCopy((AampMediaType)m_track, packetStart, len, position, position, duration);
		}
	}
	if (-1 != duration)
	{
		int durationMs = (int)(duration * 1000);
		setupThrottle(durationMs);
	}
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		m_processing = false;
		m_throttleCond.notify_one();
	}
	return ret;
}

#define HEADER_SIZE 4
#define INDEX(i) (base+i < m_packetSize-m_ttsSize-HEADER_SIZE) ? i : i+m_ttsSize+HEADER_SIZE

// Call with buffer pointing to beginning of start code (iex 0x00, 0x00, 0x01, ...)

/**
 * @brief Process ES start code
 */
bool TSProcessor::processStartCode(unsigned char *buffer, bool& keepScanning, int length, int base)
{
	bool result = true;

	if (m_isH264)
	{
		int unitType = (buffer[INDEX(3)] & 0x1F);
		switch (unitType)
		{
		case 1:  // Non-IDR slice
		case 5:  // IDR slice
			if (m_isInterlacedKnown && (m_playMode == PlayMode_retimestamp_Ionly))
			{
				// Check if first_mb_in_slice is 0.  It will be 0 for the start of a frame, but could be non-zero
				// for frames with multiple slices.  This is encoded as a variable length Exp-Golomb code.  For the value
				// of zero this will be a single '1' bit.
				if (buffer[INDEX(4)] & 0x80)
				{
					H264SPS *pSPS;
					int mask = 0x40;
					unsigned char *p = &buffer[INDEX(4)];
					(void)getUExpGolomb(p, mask); // slice_type
					int pic_parameter_set_id = getUExpGolomb(p, mask);
					m_currSPSId = m_PPS[pic_parameter_set_id].spsId;
					pSPS = &m_SPS[m_currSPSId];

					if (pSPS->picOrderCountType == 0)
					{
						if (pSPS->separateColorPlaneFlag)
						{
							// color_plane_id
							getBits(p, mask, 2);
						}

						// frame_num
						getBits(p, mask, pSPS->log2MaxFrameNumMinus4 + 4);

						if (!pSPS->frameMBSOnlyFlag)
						{
							int field_pic_flag = getBits(p, mask, 1);
							if (field_pic_flag)
							{
								//bottom_field_flag
								getBits(p, mask, 1);
							}
						}

						if (unitType == 5) // IdrPicFlag == 1
						{
							//idr_pic_id
							getUExpGolomb(p, mask);
						}

						// Update pic_order_cnt_lsb.  This field gives the frame display order.
						// If the original values are left they may be out of order so we replace
						// them with sequentially incrementing values.
						putBits(p, mask, pSPS->log2MaxPicOrderCntLsbMinus4 + 4, m_picOrderCount);
						m_picOrderCount = m_picOrderCount + 1;
						if (m_picOrderCount == pSPS->maxPicOrderCount)
						{
							m_picOrderCount = 0;
						}
						m_updatePicOrderCount = false;
						m_scanForFrameSize = false;
					}
				}
			}
			break;
		case 2:  // Slice data partiton A
		case 3:  // Slice data partiton B
		case 4:  // Slice data partiton C
			break;
		case 6:  // SEI
			break;
		case 7:  // Sequence parameter set
		{
			bool scanForAspect = false;
			bool splitSPS = false;

			if (!m_emulationPrevention || (m_emulationPreventionCapacity < (m_emulationPreventionOffset + length)))
			{
				unsigned char *newBuff = 0;
				int newSize = m_emulationPreventionCapacity * 2 + length;
				newBuff = (unsigned char *)calloc(newSize,sizeof(unsigned char));
				if (!newBuff)
				{
					AAMPLOG_ERR("Error: unable to allocate emulation prevention buffer");
					break;
				}
				if (m_emulationPrevention)
				{
					if (m_emulationPreventionOffset > 0)
					{
						memcpy(newBuff, m_emulationPrevention, m_emulationPreventionOffset);
					}
					free(m_emulationPrevention);
				}
				m_emulationPreventionCapacity = newSize;
				m_emulationPrevention = newBuff;
			}
			if (m_emulationPreventionOffset > 0)
			{
				splitSPS = true;
			}
			for (int i = 4; i < length - 3; ++i)
			{
				if ((buffer[INDEX(i)] == 0x00) && (buffer[INDEX(i + 1)] == 0x00))
				{
					if (buffer[INDEX(i + 2)] == 0x01)
					{
						scanForAspect = true;
						break;
					}
					else if (buffer[INDEX(i + 2)] == 0x03)
					{
						m_emulationPrevention[m_emulationPreventionOffset++] = 0x00;
						m_emulationPrevention[m_emulationPreventionOffset++] = 0x00;
						i += 3;
					}
				}
				m_emulationPrevention[m_emulationPreventionOffset++] = buffer[INDEX(i)];
			}
			if (scanForAspect)
			{
				bool updateSPS = processSeqParameterSet(m_emulationPrevention, m_emulationPreventionOffset);
				if (updateSPS && !splitSPS)
				{
					int ppb = -1, pb = -1, b;
					int i = 0, j = 4;
					while (i < m_emulationPreventionOffset)
					{
						b = m_emulationPrevention[i];
						if ((ppb == 0) && (pb == 0) && (b == 1))
						{
							b = 3;
						}
						else
						{
							++i;
						}
						buffer[INDEX(j)] = b;
						j++;
						ppb = pb;
						pb = b;
					}
				}
				m_emulationPreventionOffset = 0;

				if ((m_playMode != PlayMode_retimestamp_Ionly) || !m_updatePicOrderCount)
				{
					// For IOnly we need to keep scanning in order to update
					// pic_order_cnt_lsb values
					m_scanForFrameSize = false;
					keepScanning = false;
				}
			}
		}
		break;
		case 8:  // Picture parameter set
			if (m_isInterlacedKnown && (m_playMode == PlayMode_retimestamp_Ionly))
			{
				bool processPPS = false;

				if (!m_emulationPrevention || (m_emulationPreventionCapacity < (m_emulationPreventionOffset + length)))
				{
					unsigned char *newBuff = 0;
					int newSize = m_emulationPreventionCapacity * 2 + length;
					newBuff = (unsigned char *)malloc(newSize*sizeof(unsigned char));
					if (!newBuff)
					{
						AAMPLOG_ERR("Error: unable to allocate emulation prevention buffer");
						break;
					}
					if (m_emulationPrevention)
					{
						if (m_emulationPreventionOffset > 0)
						{
							memcpy(newBuff, m_emulationPrevention, m_emulationPreventionOffset);
						}
						free(m_emulationPrevention);
					}
					m_emulationPreventionCapacity = newSize;
					m_emulationPrevention = newBuff;
				}
				for (int i = 4; i < length - 3; ++i)
				{
					if ((buffer[INDEX(i)] == 0x00) && (buffer[INDEX(i + 1)] == 0x00))
					{
						if (buffer[INDEX(i + 2)] == 0x01)
						{
							processPPS = true;
							break;
						}
						else if (buffer[INDEX(i + 2)] == 0x03)
						{
							m_emulationPrevention[m_emulationPreventionOffset++] = 0x00;
							m_emulationPrevention[m_emulationPreventionOffset++] = 0x00;
							i += 3;
						}
					}
					m_emulationPrevention[m_emulationPreventionOffset++] = buffer[INDEX(i)];
				}
				if (processPPS)
				{
					processPictureParameterSet(m_emulationPrevention, m_emulationPreventionOffset);

					m_emulationPreventionOffset = 0;
				}
			}
			break;
		case 9:  // NAL access unit delimiter
			break;
		case 10: // End of sequence
			break;
		case 11: // End of stream
			break;
		case 12: // Filler data
			break;
		case 13: // Sequence parameter set extension
			break;
		case 14: // Prefix NAL unit
			break;
		case 15: // Subset sequence parameter set
			break;
			// Reserved
		case 16:
		case 17:
		case 18:
			break;
			// unspecified
		case 0:
		case 21:
		case 22:
		case 23:
		default:
			break;
		}
	}
	else
	{
		switch (buffer[INDEX(3)])
		{
			// Sequence Header
		case 0xB3:
		{
			m_frameWidth = (((int)buffer[INDEX(4)]) << 4) | (((int)buffer[INDEX(5)]) >> 4);
			m_frameHeight = ((((int)buffer[INDEX(5)]) & 0x0F) << 8) | ((int)buffer[INDEX(6)]);
			if ((m_nullPFrameWidth != m_frameWidth) || (m_nullPFrameHeight != m_frameHeight))
			{
				AAMPLOG_INFO("TSProcessor: sequence frame size %dx%d", m_frameWidth, m_frameHeight);
			}
			keepScanning = false;
		}
		break;
		default:
			if ((buffer[INDEX(3)] >= 0x01) && (buffer[INDEX(3)] <= 0xAF))
			{
				// We have hit a slice.  Stop looking for the frame size.  This must
				// be an I-frame inside a sequence.
				keepScanning = false;
			}
			break;
		}
	}

	return result;
}

/**
 * @brief Updates state variables depending on interlaced
 */
void TSProcessor::checkIfInterlaced(unsigned char *packet, int length)
{
	unsigned char* packetEnd = packet + length;
	for (int i = 0; i < length; i += m_packetSize)
	{
		packet += m_ttsSize;

		int pid = (((packet[1] & 0x1F) << 8) | (packet[2] & 0xFF));
		int payloadStart = (packet[1] & 0x40);
		int adaptation = ((packet[3] & 0x30) >> 4);
		int payload = 4;

		{
			if (pid == m_pcrPid)
			{
				if (adaptation & 0x02)
				{
					payload += (1 + packet[4]);
				}
			}
			if (adaptation & 0x01)
			{
				// Update PTS/DTS values
				if (payloadStart)
				{
					if ((packet[payload] == 0x00) && (packet[payload + 1] == 0x00) && (packet[payload + 2] == 0x01))
					{
						int streamid = packet[payload + 3];
						int pesHeaderDataLen = packet[payload + 8];

						if ((streamid >= 0xE0) && (streamid <= 0xEF))
						{
							// Video
							m_scanForFrameSize = true;
						}
						payload = payload + 9 + pesHeaderDataLen;
					}
				}

				if (m_scanForFrameSize && (m_videoPid != -1) && (pid == m_videoPid))
				{
					int j, jmax;

					if (m_scanRemainderSize)
					{
						unsigned char *remainder = m_scanRemainder;
						int copyLen;
						int srcCapacity = (int)(packetEnd - (packet + payload));
						if (srcCapacity > 0)
						{
							copyLen = m_scanRemainderLimit + (m_scanRemainderLimit - m_scanRemainderSize);
							if (copyLen > packetEnd - (packet + payload))
							{
								AAMPLOG_INFO("scan copyLen adjusted from %d to %d", copyLen, (int)(packetEnd - (packet + payload)));
								copyLen = (int)(packetEnd - (packet + payload));
							}
							memcpy(remainder + m_scanRemainderSize, packet + payload, copyLen);
							m_scanRemainderSize += copyLen;
							if (m_scanRemainderSize >= m_scanRemainderLimit * 2)
							{
								for (j = 0; j < m_scanRemainderLimit; ++j)
								{
									if ((remainder[j] == 0x00) && (remainder[j + 1] == 0x00) && (remainder[j + 2] == 0x01))
									{
										processStartCode(&remainder[j], m_scanForFrameSize, 2 * m_scanRemainderLimit - j, j);
									}
								}

								m_scanRemainderSize = 0;
							}
						}
						else
						{
							m_scanRemainderSize = 0;
							AAMPLOG_INFO("TSProcessor::checkIfInterlaced: scan skipped");
							//TEMP
							if (m_scanSkipPacketsEnabled){
								FILE *pFile = fopen("/opt/trick-scan.dat\n", "wb");
								if (pFile)
								{
									fwrite(packet, 1, m_packetSize - m_ttsSize, pFile);
									fclose(pFile);
									AAMPLOG_INFO("scan skipped: packet writing to /opt/trick-scan.dat");
								}
							}
							//TEMP
						}
					}

					if (m_scanForFrameSize)
					{
						// We need to stop scanning at the point
						// where we can no longer access all needed start code bytes.
						jmax = m_packetSize - m_scanRemainderLimit - m_ttsSize;

						for (j = payload; j < jmax; ++j)
						{
							if ((packet[j] == 0x00) && (packet[j + 1] == 0x00) && (packet[j + 2] == 0x01))
							{
								processStartCode(&packet[j], m_scanForFrameSize, jmax - j, j);

								if (!m_scanForFrameSize || m_isInterlacedKnown)
								{
									break;
								}
							}
						}

						if (m_scanForFrameSize)
						{
							unsigned char* packetScanPosition = packet + m_packetSize - m_scanRemainderLimit - m_ttsSize;
							m_scanRemainderSize = m_scanRemainderLimit < packetEnd - packetScanPosition ? m_scanRemainderLimit : (int)(packetEnd - packetScanPosition);
							memcpy(m_scanRemainder, packetScanPosition, m_scanRemainderSize);
						}
					}
				}
			}
		}

		if (m_isInterlacedKnown)
		{
			break;
		}

		packet += (m_packetSize - m_ttsSize);
	}

	m_scanRemainderSize = 0;
}

/**
 * @brief Does PTS re-stamping
 */
void TSProcessor::reTimestamp(unsigned char *&packet, int length)
{
	long long PCR = 0;
	unsigned char *pidFilter;
	unsigned char* packetEnd = packet + length;

	if (m_isH264 && !m_isInterlacedKnown)
	{
		checkIfInterlaced(packet, length);
		AAMPLOG_TRACE("m_isH264 = %s m_isInterlacedKnown = %s m_isInterlaced %s", m_isH264 ? "true" : "false",
			m_isInterlacedKnown ? "true" : "false", m_isInterlaced ? "true" : "false");
	}

	// For MPEG2 use twice the desired frame rate for IOnly re-timestamping since
	// we insert a null P-frame after every I-frame.
	float rm = ((m_isH264 && !m_isInterlaced) ? 1.0 : 2.0);

	if (!m_haveBaseTime) m_basePCR = -1LL;

	AAMPLOG_TRACE("reTimestamp: packet %p length %d", packet, length);
	for (int i = 0; i < length; i += m_packetSize)
	{
		packet += m_ttsSize;

		int pid = (((packet[1] & 0x1F) << 8) | (packet[2] & 0xFF));
		int payloadStart = (packet[1] & 0x40);
		int adaptation = ((packet[3] & 0x30) >> 4);
		int payload = 4;
		int updatePCR = 0;

		// Apply pid filter
		pidFilter = (m_trickExcludeAudio ? m_pidFilterTrick : m_pidFilter);
		if (!pidFilter[pid])
		{
			// Change to null packet
			packet[1] = ((packet[1] & 0xE0) | 0x1F);
			packet[2] = 0xFF;
		}

		// Update continuity counter
		unsigned char byte3 = packet[3];
		if (byte3 & 0x10)
		{
			packet[3] = ((byte3 & 0xF0) | (m_continuityCounters[pid]++ & 0x0F));
		}

	  {
		  // Update PCR values
		  if (pid == m_pcrPid)
		  {
			  if (payloadStart) m_basePCR = -1LL;

			  if (adaptation & 0x02)
			  {
				  int adaptationFlags = packet[5];
				  if (adaptationFlags & 0x10)
				  {
					  long long timeOffset, timeOffsetBase, rateAdjustedPCR;

					  PCR = readPCR(&packet[6]);
					  if (!m_haveBaseTime)
					  {
						  m_haveBaseTime = true;
						  m_baseTime = PCR;
						  m_segmentBaseTime = m_baseTime;
						  AAMPLOG_INFO("have baseTime %llx from pid %x PCR", m_baseTime, pid);
					  }
					  if (m_basePCR < 0) m_basePCR = PCR;
					  if (m_playMode == PlayMode_retimestamp_Ionly)
					  {
						  rateAdjustedPCR = ((m_currRateAdjustedPTS - 10000) & 0x1FFFFFFFFLL);
					  }
					  else
					  {
						  timeOffset = PCR - m_baseTime;
						  if (m_playRate < 0.0)
						  {
							  timeOffset = -timeOffset;
							  if (m_basePCR >= 0)
							  {
								  timeOffsetBase = m_baseTime - m_basePCR;
								  if (timeOffset < timeOffsetBase)
								  {
									  // When playing in reverse, a gven frame may contain multiple PCR values
									  // and we must keep them all increasing in value
									  timeOffset = timeOffsetBase + (timeOffsetBase - timeOffset);
								  }
							  }
						  }
						  rateAdjustedPCR = (((long long)(timeOffset / m_absPlayRate + 0.5) + m_segmentBaseTime) & 0x1FFFFFFFFLL);
					  }
					  m_currRateAdjustedPCR = rateAdjustedPCR;
					  ++m_pcrPerPTSCount;
					  updatePCR = 1;
				  }
				  payload += (1 + packet[4]);
			  }
		  }

		  if (adaptation & 0x01)
		  {
			  // Update PTS/DTS values
			  if (payloadStart)
			  {
				  if (m_haveBaseTime || (m_playMode == PlayMode_retimestamp_Ionly))
				  {
					  if ((packet[payload] == 0x00) && (packet[payload + 1] == 0x00) && (packet[payload + 2] == 0x01))
					  {
						  int streamid = packet[payload + 3];
						  int pesHeaderDataLen = packet[payload + 8];
						  int tsbase = payload + 9;

						  if ((streamid >= 0xE0) && (streamid <= 0xEF))
						  {
							  // Video
							  if (m_playMode == PlayMode_retimestamp_Ionly)
							  {
								  m_scanForFrameSize = true;
								  if (m_isH264)
								  {
									  m_updatePicOrderCount = true;
								  }
							  }
						  }
						  else if (((streamid >= 0xC0) && (streamid <= 0xDF)) || (streamid == 0xBD))
						  {
							  // Audio
						  }

						  long long timeOffset;
						  long long PTS = 0, DTS;
						  long long rateAdjustedPTS = 0, rateAdjustedDTS;
						  bool validPTS = false;
						  if (packet[payload + 7] & 0x80)
						  {
							  validPTS = readTimeStamp(&packet[tsbase], PTS);
							  if (validPTS)
							  {
								  if (pid == m_pcrPid)
								  {
									  m_pcrPerPTSCount = 0;
								  }
								  if (!m_haveBaseTime && (m_playMode == PlayMode_retimestamp_Ionly))
								  {
									  m_haveBaseTime = true;
									  m_baseTime = ((PTS - ((long long)(90000 / (m_apparentFrameRate*rm)))) & 0x1FFFFFFFFLL);
									  m_segmentBaseTime = m_baseTime;
									  AAMPLOG_TRACE("have baseTime %llx from pid %x PTS", m_baseTime, pid);
								  }
								  timeOffset = PTS - m_baseTime;
								  if (m_playRate < 0) timeOffset = -timeOffset;
								  if ((pid == m_pcrPid) && (m_playMode == PlayMode_retimestamp_Ionly))
								  {
									  long long interFrameDelay = 90000 / (m_apparentFrameRate*rm);
									  if (!m_haveUpdatedFirstPTS)
									  {
										  if (m_currRateAdjustedPCR != 0)
										  {
											  rateAdjustedPTS = ((m_currRateAdjustedPCR + 10000) & 0x1FFFFFFFFLL);
											  AAMPLOG_TRACE("Updated rateAdjustedPTS to %lld m_currRateAdjustedPCR %lld", rateAdjustedPTS, m_currRateAdjustedPCR);
										  }
										  else
										  {
											  rateAdjustedPTS = PTS;
											  AAMPLOG_TRACE("Updated rateAdjustedPTS to %lld m_currRateAdjustedPCR %lld", rateAdjustedPTS, m_currRateAdjustedPCR);
										  }
										  m_haveUpdatedFirstPTS = true;
									  }
									  else
									  {
										  rateAdjustedPTS = ((m_currRateAdjustedPTS + interFrameDelay) & 0x1FFFFFFFFLL);
										  AAMPLOG_TRACE("Updated rateAdjustedPTS to %lld m_currRateAdjustedPTS %lld interFrameDelay %lld", rateAdjustedPTS, m_currRateAdjustedPTS, interFrameDelay);
										  /*Don't increment pts with interFrameDelay if already done for the segment.*/
										  if (m_framesProcessedInSegment > 0)
										  {
											  AAMPLOG_TRACE("Not incrementing pts with interFrameDelay as already done for the segment");
											  if (m_playRate != 0)
											  {
												  rateAdjustedPTS = m_currRateAdjustedPTS + ((PTS - m_lastPTSOfSegment) / m_playRate);
											  }
											  else
											  {
												  rateAdjustedPTS = m_currRateAdjustedPTS + (PTS - m_lastPTSOfSegment);
											  }
										  }

										  if (updatePCR)
										  {
											  long long rateAdjustedPCR = ((rateAdjustedPTS - 10000) & 0x1FFFFFFFFLL);

											  m_currRateAdjustedPCR = rateAdjustedPCR;
										  }
									  }
									  m_currRateAdjustedPTS = rateAdjustedPTS;
								  }
								  else
								  {
									  rateAdjustedPTS = (((long long)(timeOffset / m_absPlayRate + 0.5) + m_segmentBaseTime) & 0x1FFFFFFFFLL);
								  }
								  if (pid == m_pcrPid)
								  {
									  m_throttlePTS = rateAdjustedPTS;
									  AAMPLOG_TRACE("Updated throttlePTS to %" PRIu64, m_throttlePTS.value);
								  }
								  writeTimeStamp(&packet[tsbase], packet[tsbase] >> 4, rateAdjustedPTS);
								  m_lastPTSOfSegment = PTS;
								  m_framesProcessedInSegment++;

								  AAMPLOG_TRACE("rateAdjustedPTS %lld (%lld ms)", rateAdjustedPTS, rateAdjustedPTS / 90);
							  }
							  tsbase += 5;
						  }
						  if (packet[payload + 7] & 0x40)
						  {
							  if (validPTS)
							  {
								  bool validDTS = false;
								  if ((pid == m_pcrPid) && (m_playMode == PlayMode_retimestamp_Ionly))
								  {
									  rateAdjustedDTS = rateAdjustedPTS - (2 * 750);
									  validDTS = true;
								  }
								  else
								  {
									  bool validDTS = readTimeStamp(&packet[tsbase], DTS);
									  if (validDTS)
									  {
										  timeOffset = DTS - m_baseTime;
										  if (m_playRate < 0) timeOffset = -timeOffset;
										  rateAdjustedDTS = (((long long)(timeOffset / m_absPlayRate + 0.5) + m_segmentBaseTime) & 0x1FFFFFFFFLL);
									  }
								  }
								  if (validDTS)
								  {
									  writeTimeStamp(&packet[tsbase], packet[tsbase] >> 4, rateAdjustedDTS);
								  }
							  }
							  tsbase += 5;
						  }
						  if (packet[payload + 7] & 0x02)
						  {
							  // CRC flag is set.  Following the PES header will be the CRC for the previous PES packet
							  AAMPLOG_WARN("Warning: PES packet has CRC flag set");
						  }
						  payload = payload + 9 + pesHeaderDataLen;
						  (void)tsbase; // Avoid a warning as the last value set may not be used.
					  }
				  }
			  }
			  if (m_scanForFrameSize && (m_videoPid != -1) && (pid == m_videoPid))
			  {
				  int j, jmax;

				  if (m_scanRemainderSize)
				  {
					  unsigned char *remainder = m_scanRemainder;
					  int copyLen;

					  int srcCapacity = (int)(packetEnd - (packet + payload));
					  if (srcCapacity > 0)
					  {
						  copyLen = 2 * m_scanRemainderLimit + (m_scanRemainderLimit - m_scanRemainderSize);
						  if (copyLen > packetEnd - (packet + payload))
						  {
							  AAMPLOG_INFO("scan copyLen adjusted from %d to %d", copyLen, (int)(packetEnd - (packet + payload)));
							  copyLen = (int)(packetEnd - (packet + payload));
						  }
						  memcpy(remainder + m_scanRemainderSize, packet + payload, copyLen);
						  m_scanRemainderSize += copyLen;

						  if (m_scanRemainderSize >= m_scanRemainderLimit * 3)
						  {
							  for (j = 0; j < m_scanRemainderLimit; ++j)
							  {
								  if ((remainder[j] == 0x00) && (remainder[j + 1] == 0x00) && (remainder[j + 2] == 0x01))
								  {
									  processStartCode(&remainder[j], m_scanForFrameSize, 3 * m_scanRemainderLimit - j, j);
									  memcpy(packet + payload, remainder + m_scanRemainderLimit, 2 * m_scanRemainderLimit);
								  }
							  }

							  m_scanRemainderSize = 0;
						  }
					  }
					  else
					  {
						  m_scanRemainderSize = 0;
						  AAMPLOG_INFO("TSProcessor::reTimestamp: scan skipped");
						  //TEMP
						  if (m_scanSkipPacketsEnabled){
							  FILE *pFile = fopen("/opt/trick-scan.dat", "wb");
							  if (pFile)
							  {
								  fwrite(packet, 1, m_packetSize - m_ttsSize, pFile);
								  fclose(pFile);
								  AAMPLOG_INFO("scan skipped: packet writing to /opt/trick-scan.dat");
							  }
						  }
						  //TEMP
					  }
				  }

				  if (m_scanForFrameSize)
				  {
					  // We need to stop scanning at the point
					  // where we can no longer access all needed start code bytes.
					  jmax = m_packetSize - m_scanRemainderLimit - m_ttsSize;

					  for (j = payload; j < jmax; ++j)
					  {
						  if ((packet[j] == 0x00) && (packet[j + 1] == 0x00) && (packet[j + 2] == 0x01))
						  {
							  processStartCode(&packet[j], m_scanForFrameSize, jmax - j, j);

							  if (!m_scanForFrameSize)
							  {
								  break;
							  }
						  }
					  }

					  if (m_scanForFrameSize)
					  {
						  unsigned char* packetScanPosition = packet + m_packetSize - m_scanRemainderLimit - m_ttsSize;
						  if (packetScanPosition < packetEnd)
						  {
							  m_scanRemainderSize = m_scanRemainderLimit < packetEnd - packetScanPosition ? m_scanRemainderLimit : (int)(packetEnd - packetScanPosition);
						  }
						  else
						  {
							  AAMPLOG_INFO("Scan reached out of bound packet packetScanPosition=%p", packetScanPosition);
							  m_scanRemainderSize = 0;
							  packetScanPosition = packetEnd;
						  }
						  memcpy(m_scanRemainder, packetScanPosition, m_scanRemainderSize);
					  }
				  }
			  }
		  }
	  }
	  if (updatePCR)
	  {
		  // If we repeat an I-frame as part of achieving the desired rate, we need to make sure
		  // the PCR values don't repeat
		  if (m_currRateAdjustedPCR <= m_prevRateAdjustedPCR)
		  {
			  m_currRateAdjustedPCR = ((long long)(m_currRateAdjustedPCR + (90000 / (m_apparentFrameRate*rm) + m_pcrPerPTSCount * 8)) & 0x1FFFFFFFFLL);
		  }
		  AAMPLOG_TRACE("m_currRateAdjustedPCR %lld(%lld ms) diff %lld ms", m_currRateAdjustedPCR, m_currRateAdjustedPCR / 90, (m_currRateAdjustedPCR - m_prevRateAdjustedPCR) / 90);
		  m_prevRateAdjustedPCR = m_currRateAdjustedPCR;
		  writePCR(&packet[6], m_currRateAdjustedPCR, ((m_absPlayRate >= 4.0) ? true : false));
	  }
	  packet += (m_packetSize - m_ttsSize);
	}
}

/**
 * @brief Set to the playback mode.
 *
 * @note Not relevant for demux operations
 */
void TSProcessor::setPlayMode(PlayMode mode)
{
	AAMPLOG_INFO("setting playback mode to %s", (mode == PlayMode_normal) ? "PlayMode_normal" :
		(mode == PlayMode_retimestamp_IPB) ? "PlayMode_retimestamp_IPB" :
		(mode == PlayMode_retimestamp_IandP) ? "PlayMode_retimestamp_IandP" :
		(mode == PlayMode_retimestamp_Ionly) ? "PlayMode_retimestamp_Ionly" :
		"PlayMode_reverse_GOP");
	m_playModeNext = mode;
}

/**
 * @brief Abort TSProcessor operations and return blocking calls immediately
 * @note Make sure that caller holds m_mutex before invoking this function
 */
void TSProcessor::abortUnlocked(std::unique_lock<std::mutex>& lock)
{
	m_enabled = false;
	m_basePTSCond.notify_one();
	while (m_processing)
	{
		m_throttleCond.notify_one();
		AAMPLOG_INFO("Waiting for processing to end");
		m_throttleCond.wait(lock);
	}
}

/**
 * @brief Abort current operations and return all blocking calls immediately.
 */
void TSProcessor::abort()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	abortUnlocked(lock);
}

/**
 * @brief Set the playback rate.
 *
 * @note mode is not relevant for demux operations
 */
void TSProcessor::setRate(double rate, PlayMode mode)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_havePAT = false;
	m_havePMT = false;
	abortUnlocked(lock);
	m_playRateNext = rate;
	AAMPLOG_INFO("set playback rate to %f", m_playRateNext);
	setPlayMode(mode);
	m_enabled = true;
	m_startPosition = -1.0;
	m_last_frame_time = 0;
}

/**
 * @brief Enable/ disable throttle
 */
void TSProcessor::setThrottleEnable(bool enable)
{
	AAMPLOG_INFO("TSProcessor::setThrottleEnable enable=%d", enable);
	m_throttle = enable;
}

/**
 * @brief generate PAT and PMT based on media components
 */
bool TSProcessor::generatePATandPMT(bool trick, unsigned char **buff, int *buflen, bool bHandleMCTrick)
{
	bool result = false;
	int i, j;
	int prognum, pmtVersion, pmtPid, pcrPid;
	int pmtSectionLen = 0;
	bool pcrPidFound;
	int audioComponentCount;
	const RecordingComponent* audioComponents;

	audioComponentCount = this->audioComponentCount;
	audioComponents = this->audioComponents;

	prognum = m_program;
	pmtVersion = m_versionPMT;
	pmtPid = m_pmtPid;
	pcrPid = m_pcrPid;


	if (videoComponentCount == 0)
	{
		AAMPLOG_DEBUG("no video, so keep audio in trick mode PMT");
		trick = false;
	}
	if ((videoComponentCount == 0) && (audioComponentCount == 0))
	{
		AAMPLOG_ERR("generatePATandPMT: insufficient stream information - no PAT/PMT?");
	}

	if (videoComponentCount > 0)
	{
		m_isH264 = (videoComponents[0].elemStreamType == 0x1B);
		m_scanRemainderLimit = (m_isH264 ? SCAN_REMAINDER_SIZE_H264 : SCAN_REMAINDER_SIZE_MPEG2);
	}

	if (pmtVersion == -1)
	{
		pmtVersion = 1;
	}

	// Establish pmt and pcr
	pcrPidFound = false;
	if (pmtPid == -1)
	{
		// Choose a pmt pid if not known and check pcr pid.  Pids 0-F are reserved
		for (pmtPid = 0x10; pmtPid < 0x1fff; ++pmtPid)
		{
			if (pmtPid == pcrPid)
			{
				continue;
			}
			for (i = 0; i < videoComponentCount; ++i)
			{
				if (pcrPid == videoComponents[i].pid) pcrPidFound = true;
				if (pmtPid == videoComponents[i].pid) break;
			}
			if (i < videoComponentCount) continue;

			if (!trick)
			{
				for (i = 0; i < audioComponentCount; ++i)
				{
					if (pcrPid == audioComponents[i].pid) pcrPidFound = true;
					if (pmtPid == audioComponents[i].pid) break;
				}
				if (pcrPidFound)
				{
					AAMPLOG_INFO("It is possibly MC Channel..");
					m_isMCChannel = true;
				}
				if (i < audioComponentCount) continue;
			}
			break;
		}
	}
	else
	{
		// Check pcr pid
		for (i = 0; i < videoComponentCount; ++i)
		{
			if (pcrPid == videoComponents[i].pid) pcrPidFound = true;
		}

		if ((!trick) && (!pcrPidFound))
		{
			for (i = 0; i < audioComponentCount; ++i)
			{
				if (pcrPid == audioComponents[i].pid) pcrPidFound = true;
			}
			if (pcrPidFound)
			{
				AAMPLOG_INFO("It is possibly MC Channel..");
				m_isMCChannel = true;
			}
		}
	}

	if (bHandleMCTrick && (!trick))
	{
		AAMPLOG_DEBUG("For MC channel where in audio is the PCR PID, ensure that VID PID is used as PCR PID during trick mode and change the PMT Version.");
		pcrPidFound = false;
		pmtVersion++;
	}

	if (!pcrPidFound)
	{
		/* If it is MC channel where in audio is the PCR PID && trick is true (ie no audio during trick), then the VID PID will become new PCR PID; So please update the PMT Version */
		if (trick)
		{
			AAMPLOG_INFO("If it is MC channel where in audio is the PCR PID && trick is true (ie no audio during trick), then the VID PID will become new PCR PID; So update the PMT Version by 1");
			pmtVersion++;
		}

		// With older recordings, the pcr pid was set incorrectly in the
		// meta-data - it was actually the original pmt pid.  If the pcrPid
		// wasn't found in the components, fall back to a pes pid.
		if (videoComponentCount)
		{
			pcrPid = videoComponents[0].pid;
		}
		else if (!trick && audioComponentCount)
		{
			pcrPid = audioComponents[0].pid;
		}
		else
		{
			pcrPid = 0x1fff;
		}
	}

	if ((pmtPid < 0x1fff) && (pcrPid < 0x1fff))
	{
		AAMPLOG_DEBUG("using pmt pid %04x pcr pid %04X", pmtPid, pcrPid);

		m_pcrPid = pcrPid;

		int pmtSize = 17 + m_ttsSize;
		pmtSize += videoComponentCount * 5;
		if (!trick)
		{
			for (i = 0; i < audioComponentCount; ++i)
			{
				pmtSize += 5;
				int nameLen = audioComponents[i].associatedLanguage ? (int)strlen(audioComponents[i].associatedLanguage) : 0;
				if (nameLen)
				{
					pmtSize += (3 + nameLen);
				}
			}
		}

		pmtSize += 4; //crc

		AAMPLOG_DEBUG("pmt payload size %d bytes", pmtSize);
		int pmtPacketCount = 1;
		i = pmtSize - (m_packetSize - 17 - m_ttsSize);
		while (i > 0)
		{
			++pmtPacketCount;
			i -= (m_packetSize - m_ttsSize - 4 - 4);  // TTS header, 4 byte TS header, 4 byte CRC
		}
		if (pmtPacketCount > 1)
		{
			AAMPLOG_WARN("================= pmt requires %d packets =====================", pmtPacketCount);
		}

		int patpmtLen = (pmtPacketCount + 1)*m_packetSize*sizeof(unsigned char);
		unsigned char *patpmt = (unsigned char*)malloc(patpmtLen);
		AAMPLOG_TRACE("patpmtLen %d, patpmt %p", patpmtLen, patpmt);
		if (patpmt)
		{
			int temp;
			uint32_t crc;
			int version = 1;
			unsigned char *patPacket = &patpmt[0];

			if (prognum == 0)
			{
				// If program is not known use 1
				prognum = 1;
			}

			// Generate PAT
			i = 0;
			if (m_ttsSize)
			{
				memset(patPacket, 0, m_ttsSize);
				i += m_ttsSize;
			}
			patPacket[i + 0] = 0x47; // Sync Byte
			patPacket[i + 1] = 0x60; // Payload Start=yes; Prio=0; 5 bits PId=0
			patPacket[i + 2] = 0x00; // 8 bits LSB PID = 0
			patPacket[i + 3] = 0x10; // 2 bits Scrambling = no; 2 bits adaptation field = no adaptation; 4 bits continuity counter

			patPacket[i + 4] = 0x00; // Payload start=yes, hence this is the offset to start of section
			patPacket[i + 5] = 0x00; // Start of section, Table ID = 0 (PAT)
			patPacket[i + 6] = 0xB0; // 4 bits fixed = 1011; 4 bits MSB section length
			patPacket[i + 7] = 0x0D; // 8 bits LSB section length (length = remaining bytes following this field including CRC)

			patPacket[i + 8] = 0x00; // TSID : Don't care
			patPacket[i + 9] = 0x01; // TSID : Don't care

			temp = version << 1;
			temp = temp & 0x3E; //Masking first 2 bits and last one bit : 0011 1110 (3E)
			patPacket[i + 10] = 0xC1 | temp; //C1 : 1100 0001 : setting reserved bits as 1, current_next_indicator as 1

			patPacket[i + 11] = 0x00; // Section #
			patPacket[i + 12] = 0x00; // Last section #

			// 16 bit program number of stream
			patPacket[i + 13] = (prognum >> 8) & 0xFF;
			patPacket[i + 14] = prognum & 0xFF;

			// PMT PID
			// Reserved 3 bits are set to 1
			patPacket[i + 15] = 0xE0;
			// Copying bits 8 through 12..
			patPacket[i + 15] |= (unsigned char)((pmtPid >> 8) & 0x1F);
			//now copying bits 0 through 7..
			patPacket[i + 16] = (unsigned char)(0xFF & pmtPid);

			// 4 bytes of CRC
			crc = aamp_ComputeCRC32(&patPacket[i + 5], 12);
			patPacket[i + 17] = (crc >> 24) & 0xFF;
			patPacket[i + 18] = (crc >> 16) & 0xFF;
			patPacket[i + 19] = (crc >> 8) & 0xFF;
			patPacket[i + 20] = crc & 0xFF;

			// Fill stuffing bytes for rest of TS packet
			for (i = 21 + m_ttsSize; i < m_packetSize; i++)
			{
				patPacket[i] = 0xFF;
			}

			// Generate PMT
			unsigned char *pmtPacket = &patpmt[m_packetSize];

			i = 0;
			if (m_ttsSize)
			{
				memset(pmtPacket, 0, m_ttsSize);
				i += m_ttsSize;
			}
			pmtPacket[i + 0] = 0x47;
			pmtPacket[i + 1] = 0x60;
			pmtPacket[i + 1] |= (unsigned char)((pmtPid >> 8) & 0x1F);
			pmtPacket[i + 2] = (unsigned char)(0xFF & pmtPid);
			pmtPacket[i + 3] = 0x10; // 2 bits Scrambling = no; 2 bits adaptation field = no adaptation; 4 bits continuity counter

			pmtSectionLen = pmtSize - m_ttsSize - 8;
			pmtPacket[i + 4] = 0x00;
			pmtPacket[i + 5] = 0x02;
			pmtPacket[i + 6] = (0xB0 | ((pmtSectionLen >> 8) & 0xF));
			pmtPacket[i + 7] = (pmtSectionLen & 0xFF); //lower 8 bits of Section length

			// 16 bit program number of stream
			pmtPacket[i + 8] = (prognum >> 8) & 0xFF;
			pmtPacket[i + 9] = prognum & 0xFF;

			temp = pmtVersion << 1;
			temp = temp & 0x3E; //Masking first 2 bits and last one bit : 0011 1110 (3E)
			pmtPacket[i + 10] = 0xC1 | temp; //C1 : 1100 0001 : setting reserved bits as 1, current_next_indicator as 1

			pmtPacket[i + 11] = 0x00;
			pmtPacket[i + 12] = 0x00;

			pmtPacket[i + 13] = 0xE0;
			pmtPacket[i + 13] |= (unsigned char)((pcrPid >> 8) & 0x1F);
			pmtPacket[i + 14] = (unsigned char)(0xFF & pcrPid);
			pmtPacket[i + 15] = 0xF0;
			pmtPacket[i + 16] = 0x00; //pgm info length.  No DTCP descr here..

			int pi = i + 17;
			unsigned char byte;
			for (j = 0; j < videoComponentCount; ++j)
			{
				int videoPid = videoComponents[j].pid;
				m_videoPid = videoPid;
				putPmtByte(pmtPacket, pi, videoComponents[j].elemStreamType, pmtPid);
				byte = (0xE0 | (unsigned char)((videoPid >> 8) & 0x1F));
				putPmtByte(pmtPacket, pi, byte, pmtPid);
				byte = (unsigned char)(0xFF & videoPid);
				putPmtByte(pmtPacket, pi, byte, pmtPid);
				putPmtByte(pmtPacket, pi, 0xF0, pmtPid);
				putPmtByte(pmtPacket, pi, 0x00, pmtPid);
			}
			if (!trick)
			{
				for (j = 0; j < audioComponentCount; ++j)
				{
					int audioPid = audioComponents[j].pid;
					int nameLen = audioComponents[j].associatedLanguage ? (int)strlen(audioComponents[j].associatedLanguage) : 0;
					putPmtByte(pmtPacket, pi, audioComponents[j].elemStreamType, pmtPid);
					byte = (0xE0 | (unsigned char)((audioPid >> 8) & 0x1F));
					putPmtByte(pmtPacket, pi, byte, pmtPid);
					byte = (unsigned char)(0xFF & audioPid);
					putPmtByte(pmtPacket, pi, byte, pmtPid);
					putPmtByte(pmtPacket, pi, 0xF0, pmtPid);
					if (nameLen)
					{
						putPmtByte(pmtPacket, pi, (3 + nameLen), pmtPid);
						putPmtByte(pmtPacket, pi, 0x0A, pmtPid);
						putPmtByte(pmtPacket, pi, (1 + nameLen), pmtPid);
						for (int k = 0; k < nameLen; ++k)
						{
							putPmtByte(pmtPacket, pi, audioComponents[j].associatedLanguage[k], pmtPid);
						}
					}
					putPmtByte(pmtPacket, pi, 0x00, pmtPid);
				}
			}
			// Calculate crc
			unsigned char *crcData = &patpmt[m_packetSize + m_ttsSize + 5];
			int crcLenTotal = pmtSize - m_ttsSize - 5 - 4;
			int crcLen = ((crcLenTotal > (m_packetSize - m_ttsSize - 5)) ? (m_packetSize - m_ttsSize - 5) : crcLenTotal);
			crc = 0xffffffff;
			while (crcLenTotal)
			{
				crc = aamp_ComputeCRC32(crcData, crcLen, crc);
				crcData += crcLen;
				crcLenTotal -= crcLen;
				if (crcLenTotal < crcLen)
				{
					crcLen = crcLenTotal;
				}
			}
			putPmtByte(pmtPacket, pi, ((crc >> 24) & 0xFF), pmtPid);
			putPmtByte(pmtPacket, pi, ((crc >> 16) & 0xFF), pmtPid);
			putPmtByte(pmtPacket, pi, ((crc >> 8) & 0xFF), pmtPid);
			putPmtByte(pmtPacket, pi, (crc & 0xFF), pmtPid);

			// Fill stuffing bytes for rest of TS packet
			for (i = pi; i < m_packetSize; i++)
			{
				pmtPacket[i] = 0xFF;
			}

			AAMPLOG_TRACE("generated PAT and PMT:");

			*buflen = patpmtLen;
			*buff = patpmt;

			if (trick)
			{
				// Setup pid filter for trick mode.  Block all pids except for
				// pat, pmt, pcr, video
				memset(m_pidFilterTrick, 0, sizeof(m_pidFilterTrick));
				AAMPLOG_TRACE("pass pat %04x, pmt %04x pcr %04x", 0, pmtPid, pcrPid);
				m_pidFilterTrick[pcrPid] = 1;
				for (i = 0; i < videoComponentCount; ++i)
				{
					int videoPid = videoComponents[i].pid;
					AAMPLOG_TRACE("video %04x", videoPid);
					m_pidFilterTrick[videoPid] = 1;
				}
			}
			else
			{
				// Setup pid filter.  Block all pids except for
				// pcr, video, audio
				memset(m_pidFilter, 0, sizeof(m_pidFilter));
				AAMPLOG_TRACE("pass pat %04x, pcr %04x", 0, pcrPid);
				m_pidFilter[pcrPid] = 1;
				for (i = 0; i < videoComponentCount; ++i)
				{
					int videoPid = videoComponents[i].pid;
					AAMPLOG_TRACE("video %04x", videoPid);
					m_pidFilter[videoPid] = 1;
				}
				for (i = 0; i < audioComponentCount; ++i)
				{
					int audioPid = audioComponents[i].pid;
					AAMPLOG_TRACE("audio %04x", audioPid);
					m_pidFilter[audioPid] = 1;
				}
			}

			result = true;
		}
	}

	m_patCounter = m_pmtCounter = 0;

	AAMPLOG_INFO("TSProcessor::generatePATandPMT: trick %d prognum %d pmtpid: %X pcrpid: %X pmt section len %d video %d audio %d",
		trick, prognum, pmtPid, pcrPid, pmtSectionLen,
		videoComponentCount, audioComponentCount);

	return result;
}

/**
 * @brief Appends a byte to PMT buffer
 */
void TSProcessor::putPmtByte(unsigned char* &pmt, int& index, unsigned char byte, int pmtPid)
{
	int i;
	pmt[index++] = byte;
	if (index > m_packetSize - 1)
	{
		pmt += m_packetSize;
		i = 0;
		if (m_ttsSize)
		{
			memset(pmt, 0, m_ttsSize);
			i += m_ttsSize;
		}
		pmt[i + 0] = 0x47;
		pmt[i + 1] = (unsigned char)((pmtPid >> 8) & 0x1F);
		pmt[i + 2] = (unsigned char)(0xFF & pmtPid);
		pmt[i + 3] = 0x10;  // 2 bits Scrambling = no; 2 bits adaptation field = no adaptation; 4 bits continuity counter
		index = 4;
	}
}

/**
 * @brief Read timestamp; refer ISO 13818-1
 * @retval true if timestamp present.
 */
bool TSProcessor::readTimeStamp(unsigned char *p, long long& TS)
{
	bool result = true;

	if ((p[4] & 0x01) != 1)
	{
		result = false;
		AAMPLOG_WARN("TS:============ TS p[4] bit 0 not 1");
	}
	if ((p[2] & 0x01) != 1)
	{
		result = false;
		AAMPLOG_WARN("TS:============ TS p[2] bit 0 not 1");
	}
	if ((p[0] & 0x01) != 1)
	{
		result = false;
		AAMPLOG_WARN("TS:============ TS p[0] bit 0 not 1");
	}
	switch ((p[0] & 0xF0) >> 4)
	{
	case 1:
	case 2:
	case 3:
		break;
	default:
		result = false;
		AAMPLOG_WARN("TS:============ TS p[0] YYYY bits have value %X", p[0]);
		break;
	}

	TS = ((((long long)(p[0] & 0x0E)) << 30) >> 1) |
		(((long long)(p[1] & 0xFF)) << 22) |
		((((long long)(p[2] & 0xFE)) << 15) >> 1) |
		(((long long)(p[3] & 0xFF)) << 7) |
		(((long long)(p[4] & 0xFE)) >> 1);

	return result;
}

/**
 * @brief Write time-stamp to buffer
 */
void TSProcessor::writeTimeStamp(unsigned char *p, int prefix, long long TS)
{
	p[0] = (((prefix & 0xF) << 4) | (((TS >> 30) & 0x7) << 1) | 0x01);
	p[1] = ((TS >> 22) & 0xFF);
	p[2] = ((((TS >> 15) & 0x7F) << 1) | 0x01);
	p[3] = ((TS >> 7) & 0xFF);
	p[4] = ((((TS)& 0x7F) << 1) | 0x01);
}

/**
 * @brief Read PCR from a buffer
 */
long long TSProcessor::readPCR(unsigned char *p)
{
	long long PCR = (((long long)(p[0] & 0xFF)) << (33 - 8)) |
		(((long long)(p[1] & 0xFF)) << (33 - 8 - 8)) |
		(((long long)(p[2] & 0xFF)) << (33 - 8 - 8 - 8)) |
		(((long long)(p[3] & 0xFF)) << (33 - 8 - 8 - 8 - 8)) |
		(((long long)(p[4] & 0xFF)) >> 7);
	return PCR;
}

/**
 * @brief Write PCR to a buffer
 */
void TSProcessor::writePCR(unsigned char *p, long long PCR, bool clearExtension)
{
	p[0] = ((PCR >> (33 - 8)) & 0xFF);
	p[1] = ((PCR >> (33 - 8 - 8)) & 0xFF);
	p[2] = ((PCR >> (33 - 8 - 8 - 8)) & 0xFF);
	p[3] = ((PCR >> (33 - 8 - 8 - 8 - 8)) & 0xFF);
	if (!clearExtension)
	{
		p[4] = ((PCR & 0x01) << 7) | (p[4] & 0x7F);
	}
	else
	{
		p[4] = ((PCR & 0x01) << 7) | (0x7E);
		p[5] = 0x00;
	}
}

/**
 * @brief Function to set offsetflag. if the value is false, no need to apply offset while doing pts restamping
 */
void TSProcessor::setApplyOffsetFlag(bool enable)
{
	AAMPLOG_INFO("m_applyOffset=%s",enable?"TRUE":"FALSE");
	m_applyOffset = enable;
}
/**
 * @struct MBAddrIncCode
 * @brief holds macro block address increment codes
 */
struct MBAddrIncCode
{
	int numBits;
	int code;
};

static MBAddrIncCode macroblockAddressIncrementCodes[34] =
{
	{ 1, 0x001 },  /*  1 */
	{ 3, 0x003 },  /*  2 */
	{ 3, 0x002 },  /*  3 */
	{ 4, 0x003 },  /*  4 */
	{ 4, 0x002 },  /*  5 */
	{ 5, 0x003 },  /*  6 */
	{ 5, 0x002 },  /*  7 */
	{ 7, 0x007 },  /*  8 */
	{ 7, 0x006 },  /*  9 */
	{ 8, 0x00B },  /* 10 */
	{ 8, 0x00A },  /* 11 */
	{ 8, 0x009 },  /* 12 */
	{ 8, 0x008 },  /* 13 */
	{ 8, 0x007 },  /* 14 */
	{ 8, 0x006 },  /* 15 */
	{ 10, 0x017 },  /* 16 */
	{ 10, 0x016 },  /* 17 */
	{ 10, 0x015 },  /* 18 */
	{ 10, 0x014 },  /* 19 */
	{ 10, 0x013 },  /* 20 */
	{ 10, 0x012 },  /* 21 */
	{ 11, 0x023 },  /* 22 */
	{ 11, 0x022 },  /* 23 */
	{ 11, 0x021 },  /* 24 */
	{ 11, 0x020 },  /* 25 */
	{ 11, 0x01F },  /* 26 */
	{ 11, 0x01E },  /* 27 */
	{ 11, 0x01D },  /* 28 */
	{ 11, 0x01C },  /* 29 */
	{ 11, 0x01B },  /* 30 */
	{ 11, 0x01A },  /* 31 */
	{ 11, 0x019 },  /* 32 */
	{ 11, 0x018 },  /* 33 */
	{ 11, 0x008 }   /* escape */
};

static unsigned char nullPFrameHeader[] =
{
	0x47, 0x40, 0x00, 0x10, 0x00, 0x00, 0x01, 0xE0,
	0x00, 0x00, 0x84, 0xC0, 0x0A, 0x31, 0x00, 0x01,
	0x00, 0x01, 0x11, 0x00, 0x01, 0x00, 0x01, 0x00,
	0x00, 0x01, 0x00, 0x01, 0xD7, 0xFF, 0xFB, 0x80,
	0x00, 0x00, 0x01, 0xB5, 0x83, 0x3F, 0xF3, 0x5D,
	0x80
};

#define FLUSH_SLICE_BITS()                                \
			  while ( bitcount > 8 )                              \
							{                                                   \
		 slice[i]= (((accum<<(32-bitcount))>>24)&0xFF);   \
		 ++i;                                             \
		 bitcount -= 8;                                   \
							}
#define FLUSH_ALL_SLICE_BITS()                            \
			  while ( bitcount > 0 )                              \
							{                                                   \
		 slice[i]= (((accum<<(32-bitcount))>>24)&0xFF);   \
		 ++i;                                             \
		 bitcount -= 8;                                   \
							}

/**
 * @brief Create a Null P frame
 * @retval Buffer containing P frame
 */
unsigned char* TSProcessor::createNullPFrame(int width, int height, int *nullPFrameLen)
{
	unsigned char *nullPFrame = 0;
	int requiredLen = 0;
	int blockWidth, skipWidth, escapeCount;
	int skipCode, skipCodeNumBits;
	int sliceBitLen, sliceLen, sliceCount;
	int numTSPackets;
	unsigned char slice[16];
	int i, j, bitcount;
	uint32_t accum;

	// Start of Video (19) + Picture (9) + Picture coding extension (9) minus TS packet header
	requiredLen = sizeof(nullPFrameHeader) - 4;

	blockWidth = (width + 15) / 16;
	skipWidth = blockWidth - 1;
	escapeCount = 0;
	while (skipWidth > 33)
	{
		escapeCount += 1;
		skipWidth -= 33;
	}
	skipCodeNumBits = macroblockAddressIncrementCodes[skipWidth - 1].numBits;
	skipCode = macroblockAddressIncrementCodes[skipWidth - 1].code;

	sliceBitLen = 32;  // slice start
	sliceBitLen += 5; // quantiser_scale_code (00001)
	sliceBitLen += 1; // extra_slice_bit (0)
	sliceBitLen += 1; // macroblock_address_inc (1)
	sliceBitLen += 3; // macroblock_type (001) [MC not coded]
	sliceBitLen += 1; // motion_code[0][0][0] (1) [+0 Horz]
	sliceBitLen += 1; // motion_code[0][0][1] (1) [+0 Vert]
	sliceBitLen += (escapeCount * 11 + skipCodeNumBits);  // macroblock_address_inc for frame blockWidth-1
	sliceBitLen += 3; // macroblock_type (001) [MC not coded]
	sliceBitLen += 1; // motion_code[0][0][0] (1) [+0 Horz]
	sliceBitLen += 1; // motion_code[0][0][1] (1) [+0 Vert]

	// Ensure there is at least one pad bit at end
	if ((sliceBitLen % 8) == 0) ++sliceBitLen;

	// Convert to bytes
	sliceLen = (sliceBitLen + 7) / 8;

	// Determine required number of slices for frame height
	sliceCount = (height + 15) / 16;

	// Calculate total required payload size
	requiredLen += sliceCount*sliceLen;

	// Calculate number of required TS packets
	numTSPackets = 0;
	while (requiredLen > 0)
	{
		++numTSPackets;
		requiredLen += (4 + m_ttsSize);
		requiredLen -= m_packetSize;
	}

	// Build slice
	slice[0] = 0x00;
	slice[1] = 0x00;
	slice[2] = 0x01;
	slice[3] = 0x01;
	slice[4] = 0x0A;  //quantiser_scale_factor (0000 1) extra_slice_bit (0),
	//macroblock_address_inc (1), first bit of macroblock_type (001)
	i = 5;
	accum = 0x07;     //last two bits of macroblock_type (001),
	//motion_code[0][0][0] (1) [+0 Horz]
	//motion_code[0][0][1] (1) [+0 Vert]
	bitcount = 4;
	for (j = 0; j < escapeCount; ++j)
	{
		accum <<= 11;
		accum |= 0x008; //escape: 000 0000 1000
		bitcount += 11;
		FLUSH_SLICE_BITS();
	}
	accum <<= skipCodeNumBits;
	accum |= skipCode;
	bitcount += skipCodeNumBits;
	FLUSH_SLICE_BITS();
	accum <<= 5;
	accum |= 0x07; //macroblock_type (001)
	//motion_code[0][0][0] (1) [+0 Horz]
	//motion_code[0][0][1] (1) [+0 Vert]
	bitcount += 5;
	FLUSH_SLICE_BITS();
	if (bitcount == 8)
	{
		// No zero pad bits yet, add one
		accum <<= 1;
		bitcount += 1;
	}
	FLUSH_ALL_SLICE_BITS();
	assert(i == sliceLen);

	if (numTSPackets > 0)
	{
		nullPFrame = (unsigned char *)malloc(((size_t)numTSPackets)*m_packetSize);
	}

	i = 0;
	if (nullPFrame)
	{
		if (m_ttsSize)
		{
			memset(&nullPFrame[i], 0, m_ttsSize);
			i += m_ttsSize;
		}
		memcpy(&nullPFrame[i], nullPFrameHeader, sizeof(nullPFrameHeader));
		nullPFrame[i + 1] = ((nullPFrame[i + 1] & 0xE0) | ((m_videoPid >> 8) & 0x1F));
		nullPFrame[i + 2] = (m_videoPid & 0xFF);
		i += sizeof(nullPFrameHeader);
		for (j = 1; j <= sliceCount; ++j)
		{
			slice[3] = (j & 0xFF);
			memcpy(&nullPFrame[i], slice, sliceLen);
			i += sliceLen;
			if ((i%m_packetSize) < sliceLen)
			{
				int excess = (i%m_packetSize);
				memmove(&nullPFrame[i - excess + m_ttsSize + 4], &nullPFrame[i - excess], excess);
				if (m_ttsSize)
				{
					memset(&nullPFrame[i - excess], 0, m_ttsSize);
					i += m_ttsSize;
				}
				nullPFrame[i - excess + 0] = 0x47;
				nullPFrame[i - excess + 1] = ((m_videoPid >> 8) & 0xFF);
				nullPFrame[i - excess + 2] = (m_videoPid & 0xFF);
				nullPFrame[i - excess + 3] = 0x10;
				i += 4;
			}
		}
		memset(&nullPFrame[i], 0xFF, m_packetSize - (i%m_packetSize));
		*nullPFrameLen = (numTSPackets*m_packetSize);
	}

	return nullPFrame;
}

// Parse through the sequence parameter set data to determine the frame size

/**
 * @brief process sequence parameter set and update state variables
 * @retval true if SPS is processed successfully
 */
bool TSProcessor::processSeqParameterSet(unsigned char *p, int length)
{
	bool result = false;
	int profile_idc;
	int seq_parameter_set_id;
	int mask = 0x80;

	profile_idc = p[0];

	// constraint_set0_flag : u(1)
	// constraint_set1_flag : u(1)
	// constraint_set2_flag : u(1)
	// constraint_set3_flag : u(1)
	// constraint_set4_flag : u(1)
	// constraint_set5_flag : u(1)
	// reserved_zero_2bits : u(2)
	// level_idc : u(8)

	p += 3;

	seq_parameter_set_id = getUExpGolomb(p, mask);

	m_SPS[seq_parameter_set_id].separateColorPlaneFlag = 0;
	switch (profile_idc)
	{
	case 44:  case 83:  case 86:
	case 100: case 110: case 118:
	case 122: case 128: case 244:
	{
		int chroma_format_idx = getUExpGolomb(p, mask);
		if (chroma_format_idx == 3)
		{
			// separate_color_plane_flag
			m_SPS[seq_parameter_set_id].separateColorPlaneFlag = getBits(p, mask, 1);
		}
		// bit_depth_luma_minus8
		getUExpGolomb(p, mask);
		// bit_depth_chroma_minus8
		getUExpGolomb(p, mask);
		// qpprime_y_zero_transform_bypass_flag
		getBits(p, mask, 1);

		int seq_scaling_matrix_present_flag = getBits(p, mask, 1);
		if (seq_scaling_matrix_present_flag)
		{
			int imax = ((chroma_format_idx != 3) ? 8 : 12);
			for (int i = 0; i < imax; ++i)
			{
				int seq_scaling_list_present_flag = getBits(p, mask, 1);
				if (seq_scaling_list_present_flag)
				{
					if (i < 6)
					{
						processScalingList(p, mask, 16);
					}
					else
					{
						processScalingList(p, mask, 64);
					}
				}
			}
		}
	}
	break;
	}

	// log2_max_frame_num_minus4
	int log2_max_frame_num_minus4 = getUExpGolomb(p, mask);
	m_SPS[seq_parameter_set_id].log2MaxFrameNumMinus4 = log2_max_frame_num_minus4;

	int pic_order_cnt_type = getUExpGolomb(p, mask);
	m_SPS[seq_parameter_set_id].picOrderCountType = pic_order_cnt_type;
	if (pic_order_cnt_type == 0)
	{
		// log2_max_pic_order_cnt_lsb_minus4
		int log2_max_pic_order_cnt_lsb_minus4 = getUExpGolomb(p, mask);
		m_SPS[seq_parameter_set_id].log2MaxPicOrderCntLsbMinus4 = log2_max_pic_order_cnt_lsb_minus4;
		m_SPS[seq_parameter_set_id].maxPicOrderCount = (2 << (log2_max_pic_order_cnt_lsb_minus4 + 4));
	}
	else if (pic_order_cnt_type == 1)
	{
		// delta_pic_order_always_zero_flag
		getBits(p, mask, 1);
		// offset_for_non_ref_pic
		getSExpGolomb(p, mask);
		// offset_for_top_top_bottom_field
		getSExpGolomb(p, mask);

		int num_ref_frames_in_pic_order_cnt_cycle = getUExpGolomb(p, mask);
		for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i)
		{
			// offset_for_ref_frame[i]
			getUExpGolomb(p, mask);
		}
	}

	// max_num_ref_frames
	getUExpGolomb(p, mask);

	// gaps_in_frame_num_value_allowed_flag
	getBits(p, mask, 1);

	int pic_width_in_mbs_minus1 = getUExpGolomb(p, mask);

	int pic_height_in_map_units_minus1 = getUExpGolomb(p, mask);

	int frame_mbs_only_flag = getBits(p, mask, 1);
	m_SPS[seq_parameter_set_id].frameMBSOnlyFlag = frame_mbs_only_flag;

	m_frameWidth = (pic_width_in_mbs_minus1 + 1) * 16;

	m_frameHeight = (pic_height_in_map_units_minus1 + 1) * 16;

	m_isInterlaced = (frame_mbs_only_flag == 0);
	m_isInterlacedKnown = true;

	if (m_isInterlaced)
	{
		m_frameHeight *= 2;

		if (!frame_mbs_only_flag)
		{
			// mb_adaptive_frame_field_flag
			getBits(p, mask, 1);
		}

		if (m_playMode == PlayMode_retimestamp_Ionly)
		{
			// direct_8x8_inference_flag
			getBits(p, mask, 1);

			int frame_cropping_flag = getBits(p, mask, 1);
			if (frame_cropping_flag)
			{
				// frame_crop_left_offset
				getUExpGolomb(p, mask);
				// frame_crop_right_offset
				getUExpGolomb(p, mask);
				// frame_crop_top_offset
				getUExpGolomb(p, mask);
				// frame_crop_bottom_offset
				getUExpGolomb(p, mask);
			}

			int vui_parameters_present_flag = getBits(p, mask, 1);
			if (vui_parameters_present_flag)
			{
				int aspect_ratio_info_present_flag = getBits(p, mask, 1);
				if (aspect_ratio_info_present_flag)
				{
					(void)getBits(p, mask, 8); // aspect_ratio_idc
				}

				int overscan_info_present_flag = getBits(p, mask, 1);
				if (overscan_info_present_flag)
				{
					// overscan_appropriate_flag
					getBits(p, mask, 1);
				}

				int video_signal_type_present_flag = getBits(p, mask, 1);
				if (video_signal_type_present_flag)
				{
					// video_format
					getBits(p, mask, 3);
					// video_full_range_flag
					getBits(p, mask, 1);
					int color_description_present_flag = getBits(p, mask, 1);
					if (color_description_present_flag)
					{
						// color_primaries
						getBits(p, mask, 8);
						// transfer_characteristics
						getBits(p, mask, 8);
						// matrix_coefficients
						getBits(p, mask, 8);
					}
				}

				int chroma_info_present_flag = getBits(p, mask, 1);
				if (chroma_info_present_flag)
				{
					// chroma_sample_loc_type_top_field
					getUExpGolomb(p, mask);
					// chroma_sample_loc_type_bottom_field
					getUExpGolomb(p, mask);
				}

				int timing_info_present_flag = getBits(p, mask, 1);
				if (timing_info_present_flag)
				{
					unsigned char *timeScaleP;
					int timeScaleMask;

					/* unsigned int num_units_in_tick = */
					(void) getBits(p, mask, 32);
					timeScaleP = p;
					timeScaleMask = mask;
					/*unsigned int time_scale = */
					(void) getBits(p, mask, 32);

					//CID:94243,97970 - Removed the num_units_in_tick,time_scale variable which is initialized but not used
					unsigned int trick_time_scale = m_apparentFrameRate * 2 * 1000;
					AAMPLOG_DEBUG("put trick_time_scale=%d at %p mask %X", trick_time_scale, timeScaleP, timeScaleMask);
					putBits(timeScaleP, timeScaleMask, 32, trick_time_scale);

					result = true;
				}
			}
		}
	}

	AAMPLOG_TRACE("TSProcessor: H.264 sequence frame size %dx%d interlaced=%d update SPS %d", m_frameWidth, m_frameHeight, m_isInterlaced, result);

	return result;
}

/**
 * @brief Parse through the picture parameter set to get required items
 */
void TSProcessor::processPictureParameterSet(unsigned char *p, int length)
{
	int mask = 0x80;
	int pic_parameter_set_id;
	int seq_parameter_set_id;
	H264PPS *pPPS = 0;

	pic_parameter_set_id = getUExpGolomb(p, mask);
	seq_parameter_set_id = getUExpGolomb(p, mask);

	pPPS = &m_PPS[pic_parameter_set_id];
	pPPS->spsId = seq_parameter_set_id;
}

/**
 * @brief Consume all bits used by the scaling list
 */
void TSProcessor::processScalingList(unsigned char *& p, int& mask, int size)
{
	int nextScale = 8;
	int lastScale = 8;
	for (int j = 0; j < size; ++j)
	{
		if (nextScale)
		{
			int deltaScale = getSExpGolomb(p, mask);
			nextScale = (lastScale + deltaScale + 256) % 256;
		}
		lastScale = (nextScale == 0) ? lastScale : nextScale;
	}
}

/**
 * @brief get bits based on mask and count
 * @retval value of bits
 */
unsigned int TSProcessor::getBits(unsigned char *& p, int& mask, int bitCount)
{
	int bits = 0;
	while (bitCount)
	{
		--bitCount;
		bits <<= 1;
		if (*p & mask)
		{
			bits |= 1;
		}
		mask >>= 1;
		if (mask == 0)
		{
			++p;
			mask = 0x80;
		}
	}
	return bits;
}

/**
 * @brief Put bits based on mask and count
 */
void TSProcessor::putBits(unsigned char *& p, int& mask, int bitCount, unsigned int value)
{
	unsigned int putmask;

	putmask = (1 << (bitCount - 1));
	while (bitCount)
	{
		--bitCount;
		*p &= ~mask;
		if (value & putmask)
		{
			*p |= mask;
		}
		mask >>= 1;
		putmask >>= 1;
		if (mask == 0)
		{
			++p;
			mask = 0x80;
		}
	}
}

/**
 * @brief Gets unsigned EXP Golomb
 */
unsigned int TSProcessor::getUExpGolomb(unsigned char *& p, int& mask)
{
	int codeNum = 0;
	int leadingZeros = 0;
	int factor = 2;
	bool bitClear;
	do
	{
		bitClear = !(*p & mask);
		mask >>= 1;
		if (mask == 0)
		{
			++p;
			mask = 0x80;
		}
		if (bitClear)
		{
			++leadingZeros;
			codeNum += (factor >> 1);
			factor <<= 1;
		}
	} while (bitClear);
	if (leadingZeros)
	{
		codeNum += getBits(p, mask, leadingZeros);
	}
	return codeNum;
}

/**
 * @brief Getss signed EXP Golomb
 */
int TSProcessor::getSExpGolomb(unsigned char *& p, int& bit)
{
	unsigned int u = getUExpGolomb(p, bit);
	int n = (u + 1) >> 1;
	if (!(u & 1))
	{
		n = -n;
	}
	return n;
}

/**
 * @brief Get audio components
 */
void TSProcessor::getAudioComponents(const RecordingComponent** audioComponentsPtr, int &count)
{
	count = audioComponentCount;
	*audioComponentsPtr = audioComponents;
}

/**
 * @fn Change Muxed Audio Track
 * @param[in] AudioTrackIndex
 */
void TSProcessor::ChangeMuxedAudioTrack(unsigned char index)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	AAMPLOG_WARN("Track changed from %d to %d", m_AudioTrackIndexToPlay, index);
	m_AudioTrackIndexToPlay = index;
}

/**
 * @fn Select Audio Track
 * @param[out] bestTrackIndex
 */
int TSProcessor::SelectAudioIndexToPlay()
{
	int bestTrack = -1;
	int bestScore = -1;
	for(int i=0; i<audioComponentCount ; i++)
	{
		int score = 0;
		std::string trackLanguage;
		if(audioComponents[i].associatedLanguage)
		{
			trackLanguage = audioComponents[i].associatedLanguage;
		}
		StreamOutputFormat audioFormat = getStreamFormatForCodecType(audioComponents[i].elemStreamType);
		if(!FilterAudioCodecBasedOnConfig(audioFormat))
		{
			GetLanguageCode(trackLanguage);
			if(aamp->preferredLanguagesList.size() > 0)
			{
				auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), trackLanguage);
				if(iter != aamp->preferredLanguagesList.end())
				{ // track is in preferred language list
					int distance = (int)std::distance(aamp->preferredLanguagesList.begin(),iter);
					score += (aamp->preferredLanguagesList.size()-distance)*100000; // big bonus for language match
				}
			}

			if( aamp->preferredCodecList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredCodecList.begin(), aamp->preferredCodecList.end(), GetAudioFormatStringForCodec(audioFormat) );
				if(iter != aamp->preferredCodecList.end())
				{ // track is in preferred codec list
					int distance = (int)std::distance(aamp->preferredCodecList.begin(),iter);
					score += (aamp->preferredCodecList.size()-distance)*100; //  bonus for codec match
				}
			}
			else if(audioFormat != FORMAT_UNKNOWN)
			{
				score += audioFormat;
			}
		}

		AAMPLOG_TRACE("TSProcessor > track#%d score = %d lang : %s", i+1, score, trackLanguage.c_str());
		if(score > bestScore)
		{
			bestScore = score;
			bestTrack = i;
		}
	}
	return bestTrack;
}


/**
 * @brief Function to filter the audio codec based on the configuration
 * @param[in] audioFormat
 * @param[out] bool ignoreProfile - true/false
 */
bool TSProcessor::FilterAudioCodecBasedOnConfig(StreamOutputFormat audioFormat)
{
	bool ignoreProfile = false;
	bool bDisableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
	bool bDisableAC3 = ISCONFIGSET(eAAMPConfig_DisableAC3);
	// if EC3 disabled, implicitly disable ATMOS
	bool bDisableATMOS = (bDisableEC3) ? true : ISCONFIGSET(eAAMPConfig_DisableATMOS);

	switch (audioFormat)
	{
		case FORMAT_AUDIO_ES_AC3:
			if (bDisableAC3)
			{
				ignoreProfile = true;
			}
			break;

		case FORMAT_AUDIO_ES_ATMOS:
			if (bDisableATMOS)
			{
				ignoreProfile = true;
			}
			break;

		case FORMAT_AUDIO_ES_EC3:
			if (bDisableEC3)
			{
				ignoreProfile = true;
			}
			break;

		default:
			break;
	}

	return ignoreProfile;
}

/**
 * @brief Function to get the language code
 * @param[in] string - language
 */
void TSProcessor::GetLanguageCode(std::string& lang)
{
	lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
}

/**
 * @brief Function to set the group-ID
 * @param[in] string - id
 */
void TSProcessor::SetAudioGroupId(std::string& id)
{
	if(!id.empty())
	{
		m_audioGroupId = id;
	}
}
