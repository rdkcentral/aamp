/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
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
* @file isobmffbuffer.h
* @brief Header file for ISO Base Media File Format Buffer
*/

#ifndef __ISOBMFFBUFFER_H__
#define __ISOBMFFBUFFER_H__

#include "isobmffbox.h"
#include <stddef.h>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "IsoBmffLog.h"

/**
 * @class IsoBmffBuffer
 * @brief Class for ISO BMFF Buffer
 */
class IsoBmffBuffer
{
private:
	std::vector<std::unique_ptr<Box>> boxes;	//ISOBMFF boxes of associated buffer
	const uint8_t *buffer;
	size_t bufSize;
	Box* chunkedBox; //will hold one element only
	size_t mdatCount;
	bool readOnlyBuffer;
	IsoBmff::Logger mLogger;

	/**
	 * @fn getFirstPTSInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[out] pts - pts value
	 * @return true if parse was successful. false otherwise
	 */
	bool getFirstPTSInternal(const std::vector<std::unique_ptr<Box>> *boxes, uint64_t &pts);

	/**
	 * @fn getTrackIdInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[out] track_id - track_id
	 * @return true if parse was successful. false otherwise
	 */
	bool getTrackIdInternal (const std::vector<std::unique_ptr<Box>> *boxes, uint32_t &track_id);

	/**
	 * @fn getTimeScaleInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[out] timeScale - TimeScale value
	 * @param[out] foundMdhd - flag indicates if MDHD box was seen
	 * @return true if parse was successful. false otherwise
	 */
	bool getTimeScaleInternal(const std::vector<std::unique_ptr<Box>> *boxes, uint32_t &timeScale, bool &foundMdhd);

	/**
	 * @fn printBoxesInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @return void
	 */
	void printBoxesInternal(const std::vector<std::unique_ptr<Box>> *boxes);

	/**
	 * @fn parseBoxInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[in] name - box name to get
	 * @param[out] buf - mdat buffer pointer
	 * @param[out] size - size of mdat buffer
	 * @return bool
	 */
	bool parseBoxInternal(const std::vector<std::unique_ptr<Box>> *boxes, const char *name, uint8_t *buf, size_t &size);

	/**
	 * @fn getBoxSizeInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[in] name - box name to get
	 * @param[out] size - size of mdat buffer
	 * @return bool
	 */
	bool getBoxSizeInternal(const std::vector<std::unique_ptr<Box>> *boxes, const char *name, size_t &size);

	/**
	 * @fn getBoxesInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[in] name - box name to get
	 * @param[out] pBoxes - size of mdat buffer
	 * @return bool
	 */
	bool getBoxesInternal(const std::vector<std::unique_ptr<Box>> *boxes, const char *name, std::vector<Box*> *pBoxes);

	/**
	 * @fn restampPtsInternal
	 *
	 * @brief Private method to restamp PTS in a buffer
	 *
	 * @param[in] offset - pts offset
	 * @param[in] segment - buffer pointer
	 * @param[in] bufSz - buffer size
	 */
	void restampPtsInternal(int64_t offset, uint8_t *segment, size_t bufSz);

	/**
	 * @fn updateSampleDurationInternal
	 *
	 * @brief Private method to update the sample duration in the relevant boxes,
	 *        if the sample duration is already present in those boxes.
	 *
	 * @param[in] duration - duration to set
	 * @param[in] trun - Track fragment run box in which to update the duration, if present
	 * @param[in] tfhd - Track fragment header box in which to update the duration, if present
	 * @return true if the sample duration was updated in at least one box; false otherwise
	 */
	bool updateSampleDurationInternal(uint64_t duration, TrunBox& trun, TfhdBox& tfhd);

	/**
	 * @fn getBoxInfoInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[in] name - box name to get
	 * @param[in] index - index of box in a parsed buffer
	 * @param[out] start - start offset of box
	 * @param[out] size - size of box
	 * @return bool true if box found at index, false otherwise
	 */
	bool getBoxInfoInternal(const char *name, size_t index, size_t &start, size_t &size);

public:
	/**
	 * @brief IsoBmffBuffer default constructor (null logger — no logging)
	 */
	IsoBmffBuffer(): boxes(), buffer(nullptr), bufSize(0), chunkedBox(nullptr), mdatCount(0), readOnlyBuffer(false), beforePTS(0), afterPTS(0), firstPtsSaved(false), mLogger{}
	{
	}

	/**
	 * @brief IsoBmffBuffer constructor with injected logger
	 *
	 * @param[in] logger - Logger to use for diagnostic output
	 */
	explicit IsoBmffBuffer(IsoBmff::Logger logger)
		: boxes(), buffer(nullptr), bufSize(0), chunkedBox(nullptr),
		  mdatCount(0), readOnlyBuffer(false), beforePTS(0), afterPTS(0),
		  firstPtsSaved(false), mLogger(std::move(logger))
	{
	}

	/**
	 * @fn ~IsoBmffBuffer
	 */
	~IsoBmffBuffer();

	IsoBmffBuffer(const IsoBmffBuffer&) = delete;
	IsoBmffBuffer& operator=(const IsoBmffBuffer&) = delete;

	/**
	 * @fn getParsedBoxesSize
	 * @return size of parsed boxes
	 */
	size_t getParsedBoxesSize();

	/**
	 * @fn getChunkedfBoxMetaData
	 * @return true if parsed or false
	 */
	bool getChunkedfBoxMetaData(uint32_t &offset, std::string &type, uint32_t &size);

	/**
	 * @fn UpdateBufferData
	 * @return true if parsed or false
	 */
	int UpdateBufferData(size_t parsedBoxCount, uint8_t* &unParsedBuffer, size_t &unParsedBufferSize, size_t & parsedBufferSize);

	/**
	 * @fn getTotalChunkDurationInTicks
	 * @param[in] lastMDatIndex - index of mdat box w.r.t to the full mp4 box
	 * @return total chunk duration in ticks, 0 if no chunk duration found
	 */
	uint64_t getTotalChunkDurationInTicks(int lastMDatIndex);

	/**
	 * @fn setBuffer
	 *
	 * @param[in] buffer - buffer vector reference (non-const, may be modified)
	 * @return void
	 */
	void setBuffer(std::vector<uint8_t>& buffer);

	/**
	 * @fn setBuffer
	 *
	 * @brief Set buffer from a const vector.
	 *        Use this overload when the caller only intends to parse and query
	 *        the buffer (e.g. getFirstPTS, isInitSegment, getTimeScale).
	 *        The caller must not call mutating methods (restampPts, truncate,
	 *        setPtsAndDuration, etc.) after using this overload.
	 *
	 * @param[in] buffer - const buffer vector reference
	 * @return void
	 */
	void setBuffer(const std::vector<uint8_t>& buffer);

	/**
	 * @fn setBuffer
	 *
	 * @param[in] buffer - buffer pointer
	 * @param[in] bufferLen - buffer length
	 * @return void
	 */
	void setBuffer(uint8_t* buffer, size_t bufferLen);

	/**
	 * @fn parseBuffer
	 *
	 * @brief Parse the ISO BMFF buffer and create a vector of boxes with the parsed information.
	 *        The method destroyBoxes needs to be called before parseBuffer can be called a second time.
	 *
	 * @param[in] correctBoxSize - flag to indicate if box size needs to be corrected
	 * @param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
	 * @return true if parse was successful. false otherwise
	 */
	bool parseBuffer(bool correctBoxSize = false, int newTrackId = -1);

	/**
	*  	@fn parseBuffer
	*  	@param[in] name - name of the track
	*  	@param[in/out] unParsedBuffer - Total unparsed buffer
	*  	@param[in] timeScale - timescale of the track
	*	@param[out] parsedBufferSize - parsed buffer size
	*  	@param[in/out] unParsedBufferSize - unparsed or remaining buffer size
	*	@param[out] fpts - fragment pts value
	*  	@param[out] fduration - fragment duration
	*	@return true if parsed or false
	*  	@brief Parse ISOBMFF boxes from buffer
	*/
	bool ParseChunkData(const char* name, uint8_t* &unParsedBuffer, uint32_t timeScale,
	 size_t & parsedBufferSize, size_t &unParsedBufferSize, double& fpts, double &fduration);

	/**
	 * @fn restampPTS
	 *
	 * @brief Legacy restamp API kept for compatibility with existing processor
	 *        call paths. Prefer restampPts() for new parser callers.
	 *
	 * @param[in] offset - pts offset
	 * @param[in] basePts - base pts
	 * @param[in] segment - buffer pointer
	 * @param[in] bufSz - buffer size
	 * @return void
	 */
	void restampPTS(uint64_t offset, uint64_t basePts, uint8_t *segment, uint32_t bufSz);

	/**
	 * @fn restampPts
	 *
	 * @brief Restamp PTS in a buffer
	 *        The IsoBmffBuffer parsing is no longer valid after this method is
	 *        called; parseBuffer() has to be called again for the get methods
	 *        to return the right value.
	 *
	 * @param[in] offset - pts offset
	 */
	void restampPts(int64_t offset);

	/**
	 * @fn setPtsAndDuration
	 *
	 * @brief Set the PTS (base media decode time) and sample duration.
	 *        This method assumes that the buffer contains an I-frame media segment,
	 *        consisting of a single sample, so is suitable for trick mode re-stamping.
	 *        If the buffer contains multiple samples or truns, only the first sample
	 *        duration will be set (if flagged as present).
	 *
	 * @param[in] pts - Base media decode time to set
	 * @param[in] duration - Sample duration to set
	 */
	void setPtsAndDuration(uint64_t pts, uint64_t duration);

	/**
	 * @fn getFirstPTS
	 *
	 * @param[out] pts - pts value
	 * @return true if parse was successful. false otherwise
	 */
	bool getFirstPTS(uint64_t &pts);

	/**
	 * @fn getTrack_id
	 *
	 * @param[out] track_id - track-id
	 * @return true if parse was successful. false otherwise
	 */
	bool getTrack_id(uint32_t &track_id);

	/**
	 * @fn PrintPTS
	 * @return tvoid
	 */
	void PrintPTS(void);

	/**
	 * @fn getTimeScale
	 *
	 * @param[out] timeScale - TimeScale value
	 * @return true if parse was successful. false otherwise
	 */
	bool getTimeScale(uint32_t &timeScale);

	/**
	 * @fn destroyBoxes
	 *
	 * @return void
	 */
	void destroyBoxes();

	/**
	 * @fn printBoxes
	 *
	 * @return void
	 */
	void printBoxes();

	/**
	 * @fn isInitSegment
	 *
	 * @return true if buffer is an initialization segment. false otherwise
	 */
	bool isInitSegment();

	/**
 	 * @fn parseMdatBox
	 * @param[out] buf - mdat buffer pointer
	 * @param[out] size - size of mdat buffer
	 * @return true if mdat buffer is available. false otherwise
	 */
	bool parseMdatBox(uint8_t *buf, size_t &size);

	/**
	 * @fn getMdatBoxSize
	 * @param[out] size - size of mdat buffer
	 * @return true if buffer size available. false otherwise
	 */
	bool getMdatBoxSize(size_t &size);

	/**
	 * @fn getEMSGData
	 *
	 * @param[out] message - messageData from EMSG
	 * @param[out] messageLen - messageLen
	 * @param[out] schemeIdUri - schemeIdUri
	 * @param[out] value - value of Id3
	 * @param[out] presTime - Presentation time
	 * @param[out] timeScale - timeScale of ID3 metadata
	 * @param[out] eventDuration - eventDuration value
	 * @param[out] id - ID of metadata
	 * @return true if parse was successful. false otherwise
	 */
	bool getEMSGData(uint8_t* &message, uint32_t &messageLen, char * &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id);

	/**
	 * @fn getEMSGInfoInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[out] message - messageData pointer
	 * @param[out] messageLen - messageLen value
	 * @param[out] schemeIdUri - schemeIdUri
	 * @param[out] value - value of Id3
	 * @param[out] presTime - Presentation time
	 * @param[out] timeScale - timeScale of ID3 metadata
	 * @param[out] eventDuration - eventDuration value
	 * @param[out] id - ID of metadata
	 * @param[out] foundEmsg - flag indicates if EMSG box was seen
	 * @return true if parse was successful. false otherwise
	 */
	bool getEMSGInfoInternal(const std::vector<std::unique_ptr<Box>> *boxes, uint8_t* &message, uint32_t &messageLen, char * &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id, bool &foundEmsg);

	/**
	 * @fn getMdatBoxCount
	 * @param[out] count - mdat box count
	 * @return true if mdat count available. false otherwise
	 */
	bool getMdatBoxCount(size_t &count);

	/**
 	 * @fn printMdatBoxes
	 *
	 * @return void
	 */
	void printMdatBoxes();

	/**
	 * @fn getTypeOfBoxes
	 * @param[in] name - box name to get
	 * @param[out] stBoxes - List of box handles of a type in a parsed buffer
	 * @return true if Box found. false otherwise
	 */
	bool getTypeOfBoxes(const char *name, std::vector<Box*> &stBoxes);

	/**
	 * @fn getChunkedfBox
	 *
	 * @return Box handle if Chunk box found in a parsed buffer. nullptr otherwise
	 */
	Box* getChunkedfBox() const;

	/**
	 * @fn getParsedBoxes
	 *
	 * @return Box handle list if Box found at index given. nullptr otherwise
	 */
	const std::vector<std::unique_ptr<Box>> *getParsedBoxes() const;

	/**
	 * @fn getBox
	 * @param[in] name - box name to get
	 * @param[in,out] index - index of box in a parsed buffer;
	 *                   in: start index to search, out: index of the box found
	 * @return Box handle if Box found at index given. nullptr otherwise
	 */
	Box* getBox(const char *name, size_t &index);

	/**
	 * @fn getBoxAtIndex
	 * @param[out] index - index of box in a parsed buffer
	 * @return Box handle if Box found at index given. nullptr otherwise
	 */
	Box* getBoxAtIndex(size_t index);

	/**
	 * @fn getChildBox
	 * @param [in] parent - parent box to search in
	 * @param[in] name - box name to get
	 * @param[in out] index - index of box in a parsed buffer
	 * in: start index to search, out: index of the box found
	 * index should be 0 for the first call; if subsequent boxes are to be found, index should set to be 1 + the last index returned
	 * @return Box handle if Box found at index given. nullptr otherwise
	 */
	Box* getChildBox(Box *parent, const char *name, size_t &index);

	/**
	 * @fn printPTSInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @return void
	 */
	void printPTSInternal(const std::vector<std::unique_ptr<Box>> *boxes);

	/**
	 * @fn getSampleDuration
	 *
	 * @param[in] box - ISOBMFF box
	 * @param[in] fduration -  duration to get
	 * @return void
	 */
	void getSampleDuration(Box *box, uint64_t &fduration);

	/**
	 * @fn getSampleDurationInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @return uint64_t - duration  value
	 */
	uint64_t getSampleDurationInternal(const std::vector<std::unique_ptr<Box>> *boxes);

	/**
	 * @fn getPts
	 *
	 * @param[in] box - ISOBMFF box
	 * @param[in] fpts -  PTS to get
	 * @return void
	 */
	void getPts(Box *box, uint64_t &fpts);

	/**
 	 * @fn getPtsInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @return uint64_t - PTS value
	 */
	uint64_t getPtsInternal(const std::vector<std::unique_ptr<Box>> *boxes);

	/**
	 * @fn truncate
	 *
	 * @brief For a parsed buffer, truncate it to retain just the first sample: reduce all relevant tables to 1 entry
	 *        and truncate the first mdat box to the first sample. Any boxes following the first mdat are discarded.
	 *
	*/
	void truncate(void);
	size_t getSize(void) const { return bufSize; }

	/**
 	 * @fn getSegmentDuration
	 *
	 * @return uint64_t - Total segment duration in units of timescale.
	 */
	uint64_t getSegmentDuration();
	uint64_t beforePTS;	//For log output
	uint64_t afterPTS;	//For log output
	bool firstPtsSaved;	//Used to log, only the first PTS restamped in a segment.

	/**
 	 * @fn setTrickmodeTimescale
	 *
	 * @brief Set the timescale for trickmode playback
	 * @param[in] timescale - timescale value
	 * @return bool - found the correct boxes
	 */
	bool setTrickmodeTimescale(uint32_t timescale);

	/**
	 * @fn setMediaHeaderDuration
	 *
	 * @brief Set the sample duration in the mdhd box
	 * @param[in] duration - sample duration value
	 * @return true if sample duration set. false otherwise
	 */
	bool setMediaHeaderDuration(uint64_t duration);

	/**
	 * @fn getMdatBoxInfo
	 *
	 * @param[in] index - index of mdat box
	 * @param[out] start - start offset of mdat box
	 * @param[out] size - size of mdat box
	 * @return bool if box found. false otherwise
	 */
	bool getMdatBoxInfo(size_t index, size_t &start, size_t &size);

	/**
	 * @fn getChunkedMdatBoxInfo
	 *
	 * @param[out] start - start offset of chunked mdat box
	 * @param[out] size - size of chunked mdat box
	 * @return true if chunked mdat box is present. false otherwise
	 */
	bool getChunkedMdatBoxInfo(size_t &start, size_t &size) const;

	/**
	 * @fn getLastMdatBoxIndex
	 *
	 * @return index of mdat box w.r.t to the full mp4 box, -1 if index is out of bound
	 */
	int getLastMdatBoxIndex() const;
};
#endif /* __ISOBMFFBUFFER_H__ */
