/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file Mp4DemuxAdapter.h
 * @brief Adapter class to wrap new MP4Demux API for use with gst test harness
 *
 * This adapter provides backward compatibility with the old mp4demux.hpp API
 * while using the newer, more robust MP4Demux implementation from mp4demux/.
 */

#ifndef __MP4_DEMUX_ADAPTER_H__
#define __MP4_DEMUX_ADAPTER_H__

#include "MP4Demux.h"
#include "AampDemuxDataTypes.h"
#include "DemuxDataTypes.h"
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <memory>
#include <vector>
#include <cstring>

/**
 * @brief Adapter class wrapping MP4Demux to provide legacy API compatibility
 *
 * This class wraps the new MP4Demux implementation and provides the old API
 * surface that the gst test harness expects, including:
 * - count(), getLen(), getPts(), getDts(), getDuration(), getPtr()
 * - getDrmMetadata(), setCaps(), getProtectionEvent(), getNumProtectionEvents()
 * - Static AdjustMediaDecodeTime() method
 */
class Mp4DemuxAdapter
{
public:
	/**
	 * @brief Constructor
	 */
	Mp4DemuxAdapter();

	/**
	 * @brief Destructor
	 */
	~Mp4DemuxAdapter();

	// Delete copy and move operations
	Mp4DemuxAdapter(const Mp4DemuxAdapter&) = delete;
	Mp4DemuxAdapter& operator=(const Mp4DemuxAdapter&) = delete;
	Mp4DemuxAdapter(Mp4DemuxAdapter&&) = delete;
	Mp4DemuxAdapter& operator=(Mp4DemuxAdapter&&) = delete;

	/**
	 * @brief Parse MP4 data
	 * @param ptr Pointer to MP4 data
	 * @param len Length of data
	 * @return true if parsing succeeded, false on error
	 */
	bool Parse(const void* ptr, size_t len);

	/**
	 * @brief Get timescale value
	 * @return Media timescale
	 */
	uint32_t getTimeScale() const;

	/**
	 * @brief Get number of parsed samples
	 * @return Number of samples
	 */
	int count() const;

	/**
	 * @brief Get pointer to sample data
	 * @param part Sample index
	 * @return Pointer to sample data
	 */
	const uint8_t* getPtr(int part) const;

	/**
	 * @brief Get sample data length
	 * @param part Sample index
	 * @return Length of sample data in bytes
	 */
	size_t getLen(int part) const;

	/**
	 * @brief Get presentation timestamp
	 * @param part Sample index
	 * @return PTS in seconds
	 */
	double getPts(int part) const;

	/**
	 * @brief Get decode timestamp
	 * @param part Sample index
	 * @return DTS in seconds
	 */
	double getDts(int part) const;

	/**
	 * @brief Get sample duration
	 * @param part Sample index
	 * @return Duration in seconds
	 */
	double getDuration(int part) const;

	/**
	 * @brief Get DRM metadata for a sample
	 * @param sampleIndex Sample index
	 * @return GstStructure containing DRM metadata (caller must free)
	 */
	GstStructure* getDrmMetadata(int sampleIndex) const;

	/**
	 * @brief Set caps on appsrc based on codec info
	 * @param appsrc GStreamer app source element
	 */
	void setCaps(GstAppSrc* appsrc) const;

	/**
	 * @brief Get number of protection events
	 * @return Number of protection events
	 */
	size_t getNumProtectionEvents() const;

	/**
	 * @brief Get protection event
	 * @param which Event index
	 * @return GstEvent protection event (caller must manage lifecycle)
	 */
	GstEvent* getProtectionEvent(int which) const;

	/**
	 * @brief Adjust media decode time for PTS restamping
	 * @param ptr Pointer to MP4 data
	 * @param len Length of data
	 * @param pts_restamp_delta PTS adjustment delta
	 * @return Base media decode time
	 *
	 * This static method provides invasive PTS restamping for DAI use cases.
	 */
	static uint64_t AdjustMediaDecodeTime(uint8_t* ptr, size_t len, int64_t pts_restamp_delta);

private:
	/**
	 * @brief Helper to create GstBuffer from data
	 * @param data Data pointer
	 * @param size Data size
	 * @return Newly allocated GstBuffer
	 */
	static GstBuffer* CreateGstBuffer(gconstpointer data, gsize size);

	/**
	 * @brief Convert CipherType to FourCC string
	 * @param cipher Cipher type
	 * @return FourCC string representation
	 */
	static const char* CipherTypeToString(CipherType cipher);

	std::unique_ptr<Mp4Demux> mDemux;
	std::vector<AampMediaSample> mSamples;
	std::vector<MediaProtectionInfo> mProtectionEvents;
	MediaCodecInfo mCodecInfo;
	uint32_t mTimeScale;

	// Cache for protection GstEvents (created lazily)
	mutable std::vector<GstEvent*> mGstProtectionEvents;
};

#endif /* __MP4_DEMUX_ADAPTER_H__ */
