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
* @file isobmffbuffer.cpp
* @brief Source file for ISO Base Media File Format Buffer
*/

#include "isobmffbuffer.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include <inttypes.h>
#include <string.h>

static Box *findBoxInVector(const char * box_type, const std::vector<Box*> *boxes);

/**
 *  @brief IsoBmffBuffer destructor
 */
IsoBmffBuffer::~IsoBmffBuffer()
{
	for (unsigned int i=(unsigned int)boxes.size(); i>0;)
	{
		--i;
		SAFE_DELETE(boxes[i]);
		boxes.pop_back();
	}
	boxes.clear();
}

/**
 *  @brief Set buffer
 */
void IsoBmffBuffer::setBuffer(uint8_t *buf, size_t sz)
{
	buffer = buf;
	bufSize = sz;
}

/**
*  	@fn ParseChunkData
*  	@param[in] name - name of the track
*  	@param[in,out] unParsedBuffer - Total unparsedbuffer
*  	@param[in] timeScale - timescale of the track
*	@param[out] parsedBufferSize - parsed buffer size
*  	@param[in,out] unParsedBufferSize -uunparsed or remaining buffer size
*	@param[out] fpts - fragment pts value
*  	@param[out] fduration - fragment duration
*	@return true if parsed or false
*  	@brief Parse ISOBMFF boxes from buffer
*/
bool IsoBmffBuffer::ParseChunkData(const char* name, char* &unParsedBuffer, uint32_t timeScale,
	size_t & parsedBufferSize, size_t &unParsedBufferSize, double& fpts, double &fduration)
{
	size_t mdatCount = 0;
	size_t parsedBoxCount = 0;
	getMdatBoxCount(mdatCount);
	if(!mdatCount)
	{
		return false;
	}
	AAMPLOG_TRACE("[%s] MDAT count found: %zu",  name, mdatCount );
	parsedBoxCount = getParsedBoxesSize();
	uint32_t boxOffset = 0;
	std::string boxTypeStr = "";
	uint32_t boxSize = 0;
	if(getChunkedfBoxMetaData(boxOffset, boxTypeStr, boxSize))
	{
		parsedBoxCount--;
		AAMPLOG_TRACE("[%s] MDAT Chunk Found - Actual Parsed Box Count: %zu", name,parsedBoxCount);
		AAMPLOG_TRACE("[%s] Chunk Offset[%u] Chunk Type[%s] Chunk Size[%u]", name, boxOffset, boxTypeStr.c_str(), boxSize);
	}
	if(mdatCount)
	{
		int lastMDatIndex = UpdateBufferData(parsedBoxCount, unParsedBuffer, unParsedBufferSize, parsedBufferSize);

		uint64_t fPts = 0;
		double totalChunkDuration = getTotalChunkDuration( lastMDatIndex);

		//get PTS of buffer
		bool bParse = getFirstPTS(fPts);
		if (bParse)
		{
			AAMPLOG_TRACE("[%s] fPts %" PRIu64,name, fPts);
		}
		fpts = (double) fPts/(timeScale*1.0);
		fduration = totalChunkDuration/(timeScale*1.0);
	}
	return true;
}
/**
 *	@fn parseBuffer
 *  @param[in] correctBoxSize - flag to correct the box size
 *	@param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
 *  @brief Parse ISOBMFF boxes from buffer
 */
bool IsoBmffBuffer::parseBuffer(bool correctBoxSize, int newTrackId)
{
	size_t curOffset = 0;
	while (curOffset < bufSize)
	{
		Box *box = Box::constructBox(buffer+curOffset, (uint32_t)(bufSize - curOffset), correctBoxSize, newTrackId);
		if( ((bufSize - curOffset) < 4) || ( (bufSize - curOffset) < box->getSize()) )
		{
			chunkedBox = box;
		}
		box->setOffset((uint32_t)curOffset);
		boxes.push_back(box);
		curOffset += box->getSize();
	}
	return !!(boxes.size());
}

/**
 *  @brief Get mdat buffer handle and size from parsed buffer
 */
bool IsoBmffBuffer::parseMdatBox(uint8_t *buf, size_t &size)
{
	return parseBoxInternal(&boxes, Box::MDAT, buf, size);
}

#define BOX_HEADER_SIZE 8

/**
 *  @brief parse ISOBMFF boxes of a type in a parsed buffer
 */
bool IsoBmffBuffer::parseBoxInternal(const std::vector<Box*> *boxes, const char *name, uint8_t *buf, size_t &size)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		AAMPLOG_TRACE("Offset[%u] Type[%s] Size[%u]", box->getOffset(), box->getType(), box->getSize());
		if (IS_TYPE(box->getType(), name))
		{
			size_t offset = box->getOffset() + BOX_HEADER_SIZE;
			size = box->getSize() - BOX_HEADER_SIZE;
			memcpy(buf, buffer + offset, size);
			return true;
		}
	}
	return false;
}

/**
 *  @brief Get mdat buffer size
 */
bool IsoBmffBuffer::getMdatBoxSize(size_t &size)
{
	return getBoxSizeInternal(&boxes, Box::MDAT, size);
}

/**
 *  @brief get ISOBMFF box size of a type
 */
bool IsoBmffBuffer::getBoxSizeInternal(const std::vector<Box*> *boxes, const char *name, size_t &size)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), name))
		{
			size = box->getSize();
			return true;
		}
	}
	return false;
}

/**
 *  @brief Restamp PTS in a buffer
 */
void IsoBmffBuffer::restampPTS(uint64_t offset, uint64_t basePts, uint8_t *segment, uint32_t bufSz)
{
	uint32_t curOffset = 0;
	while (curOffset < bufSz)
	{
		uint8_t *buf = segment + curOffset;
		uint32_t size = READ_U32(buf);
		uint8_t type[5];
		READ_U8(type, buf, 4);
		type[4] = '\0';

		if (IS_TYPE(type, Box::MOOF) || IS_TYPE(type, Box::TRAF))
		{
			restampPTS(offset, basePts, buf, size);
		}
		else if (IS_TYPE(type, Box::TFDT))
		{
			uint8_t version = READ_VERSION(buf);
			uint32_t flags  = READ_FLAGS(buf);

			(void)flags; // Avoid a warning.

			if (1 == version)
			{
				uint64_t pts = ReadUint64(buf);
				pts -= basePts;
				pts += offset;
				WriteUint64(buf, pts);
			}
			else
			{
				uint32_t pts = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
				pts -= (uint32_t)basePts;
				pts += (uint32_t)offset;
				WRITE_U32(buf, pts);
			}
		}
		curOffset += size;
	}
}

void IsoBmffBuffer::restampPtsInternal(int64_t offset, uint8_t *segment, size_t bufSz)
{
	size_t curOffset = 0;
	while (curOffset < bufSz)
	{
		uint8_t *buf = segment + curOffset;
		uint32_t size = READ_U32(buf);
		uint8_t type[5];
		READ_U8(type, buf, 4);
		type[4] = '\0';

		if (IS_TYPE(type, Box::MOOF) || IS_TYPE(type, Box::TRAF))
		{
			restampPtsInternal(offset, buf, size);
		}
		else if (IS_TYPE(type, Box::TFDT))
		{
			uint8_t version = READ_VERSION(buf);
			uint32_t flags  = READ_FLAGS(buf);

			(void)flags; // Avoid a warning.

			if (1 == version)
			{
				uint64_t pts = ReadUint64(buf);
				if (!firstPtsSaved)
				{
					beforePTS = pts;
				}
				pts += offset;
				WriteUint64(buf, pts);
				if (!firstPtsSaved)
				{
					firstPtsSaved = true;
					afterPTS = pts;
				}
			}
			else
			{
				uint32_t pts = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
				if (!firstPtsSaved)
				{
					beforePTS = pts;
				}
				pts += (uint32_t)offset;
				WRITE_U32(buf, pts);
				if (!firstPtsSaved )
				{
					afterPTS = pts;
					firstPtsSaved = true;
				}
			}
		}
		else
		{
			// Any other box type
		}
		curOffset += size;
	}
}

void IsoBmffBuffer::restampPts(int64_t offset)
{
	restampPtsInternal(offset, buffer, bufSize);
}

void IsoBmffBuffer::setPtsAndDuration(uint64_t pts, uint64_t duration)
{
	size_t index{0};

	// This is an I-frame media segment, so there will only be one moof with one traf
	auto moof{getBox(Box::MOOF, index)};
	if (moof)
	{
		auto traf{findBoxInVector(Box::TRAF, moof->getChildren())};
		if (traf)
		{
			auto tfdt{dynamic_cast<TfdtBox *>(findBoxInVector(Box::TFDT, traf->getChildren()))};
			if (tfdt)
			{
				tfdt->setBaseMDT(pts);
			}
			else
			{
				AAMPLOG_WARN("tfdt box unexpectedly missing");
			}

			auto trun{dynamic_cast<TrunBox *>(findBoxInVector(Box::TRUN, traf->getChildren()))};
			auto tfhd{dynamic_cast<TfhdBox *>(findBoxInVector(Box::TFHD, traf->getChildren()))};
			// According to the IsoBmff spec a tfhd is mandatory, and can exist without a trun,
			// but in that case the code assumes there will be no duration to update.
			if (trun && tfhd)
			{
				if (!updateSampleDurationInternal(duration, *trun, *tfhd))
				{
					AAMPLOG_WARN("Sample duration not set");
				}
			}
			else
			{
				AAMPLOG_WARN("trun (%p) or tfhd (%p) box unexpectedly missing", trun, tfhd);
			}
		}
		else
		{
			AAMPLOG_WARN("traf box unexpectedly missing");
		}
	}
	else
	{
		AAMPLOG_WARN("moof box unexpectedly missing");
	}
}

/**
 *  @brief Release ISOBMFF boxes parsed
 */
void IsoBmffBuffer::destroyBoxes()
{
	for (unsigned int i=(unsigned int)boxes.size(); i>0;)
	{
		--i;
		SAFE_DELETE(boxes[i]);
		boxes.pop_back();
	}
	boxes.clear();
}

/**
 *  @brief Get first PTS of buffer
 */
bool IsoBmffBuffer::getFirstPTSInternal(const std::vector<Box*> *boxes, uint64_t &pts)
{
	bool ret = false;
	for (size_t i = 0; (false == ret) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::TFDT))
		{
			TfdtBox *tfdtBox = dynamic_cast<TfdtBox *>(box);
			if(tfdtBox)
			{
				pts = tfdtBox->getBaseMDT();
				ret = true;
				break;
			}
		}
		if (box->hasChildren())
		{
			ret = getFirstPTSInternal(box->getChildren(), pts);
		}
	}
	return ret;
}

/**
 *  @brief Get track id from trak box
 */
bool IsoBmffBuffer::getTrackIdInternal(const std::vector<Box*> *boxes, uint32_t &track_id)
{
	bool ret = false;
	for (size_t i = 0; (false == ret) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::TRAK))
		{
			try {
				TrakBox *trakBox = dynamic_cast<TrakBox *>(box);
				if(trakBox)
				{
					track_id = trakBox->getTrack_Id();
					ret = true;
					break;
				}
			} catch (std::bad_cast& bc){
				//do nothing
			}
		}
		if (box->hasChildren())
		{
			ret = getTrackIdInternal(box->getChildren(), track_id);
		}
	}
	return ret;
}

/**
 *  @brief Get first PTS of buffer
 */
bool IsoBmffBuffer::getFirstPTS(uint64_t &pts)
{
	return getFirstPTSInternal(&boxes, pts);
}

/**
 *  @brief Print PTS of buffer
 */
void IsoBmffBuffer::PrintPTS(void)
{
	return printPTSInternal(&boxes);
}

/**
 *  @brief Get track_id from the trak box
 */
bool IsoBmffBuffer::getTrack_id(uint32_t &track_id)
{
	return getTrackIdInternal(&boxes, track_id);
}

/**
 *  @brief Get TimeScale value of buffer
 */
bool IsoBmffBuffer::getTimeScaleInternal(const std::vector<Box*> *boxes, uint32_t &timeScale, bool &foundMdhd)
{
	bool ret = false;
	for (size_t i = 0; (false == foundMdhd) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::MVHD))
		{
			MvhdBox *mvhdBox = dynamic_cast<MvhdBox *>(box);
			if(mvhdBox){
				timeScale = mvhdBox->getTimeScale();
				ret = true;
			}
		}
		else if (IS_TYPE(box->getType(), Box::MDHD))
		{
			MdhdBox *mdhdBox = dynamic_cast<MdhdBox *>(box);
			if(mdhdBox) {
				timeScale = mdhdBox->getTimeScale();
				ret = true;
				foundMdhd = true;
			}
		}
		if (box->hasChildren())
		{
			ret = getTimeScaleInternal(box->getChildren(), timeScale, foundMdhd);
		}
	}
	return ret;
}

/**
 *  @brief Get TimeScale value of buffer
 */
bool IsoBmffBuffer::getTimeScale(uint32_t &timeScale)
{
	bool foundMdhd = false;
	return getTimeScaleInternal(&boxes, timeScale, foundMdhd);
}

/**
 *  @brief Print ISOBMFF boxes
 */
void IsoBmffBuffer::printBoxesInternal(const std::vector<Box*> *boxes)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		AAMPLOG_WARN("Offset[%u] Type[%s] Size[%u]", box->getOffset(), box->getType(), box->getSize());
		if (IS_TYPE(box->getType(), Box::TFDT))
		{
			TfdtBox *tfdtBox = dynamic_cast<TfdtBox *>(box);
			if(tfdtBox) {
				AAMPLOG_WARN("****Base Media Decode Time: %" PRIu64, tfdtBox->getBaseMDT());
			}
		}
		else if (IS_TYPE(box->getType(), Box::MVHD))
		{
			MvhdBox *mvhdBox = dynamic_cast<MvhdBox *>(box);
			if(mvhdBox) {
				AAMPLOG_WARN("**** TimeScale from MVHD: %u", mvhdBox->getTimeScale());
			}
		}
		else if (IS_TYPE(box->getType(), Box::MDHD))
		{
			MdhdBox *mdhdBox = dynamic_cast<MdhdBox *>(box);
			if(mdhdBox) {
				AAMPLOG_WARN("**** TimeScale from MDHD: %u", mdhdBox->getTimeScale());
			}
		}

		if (box->hasChildren())
		{
			printBoxesInternal(box->getChildren());
		}
	}
}


/**
 *  @brief Print ISOBMFF boxes
 */
void IsoBmffBuffer::printBoxes()
{
	printBoxesInternal(&boxes);
}

/**
 *  @brief Check if buffer is an initialization segment
 */
bool IsoBmffBuffer::isInitSegment()
{
	bool foundFtypBox = false;
	for (size_t i = 0; i < boxes.size(); i++)
	{
		Box *box = boxes.at(i);
		if (IS_TYPE(box->getType(), Box::FTYP))
		{
			foundFtypBox = true;
			break;
		}
	}
	return foundFtypBox;
}

/**
 *  @brief Get emsg informations
 */
bool IsoBmffBuffer::getEMSGInfoInternal(const std::vector<Box*> *boxes, uint8_t* &message, uint32_t &messageLen, char * &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id, bool &foundEmsg)
{
	bool ret = false;
	for (size_t i = 0; (false == foundEmsg) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::EMSG))
		{
			EmsgBox *emsgBox = dynamic_cast<EmsgBox *>(box);
			if(emsgBox)
			{
				message = emsgBox->getMessage();
				messageLen = emsgBox->getMessageLen();
				presTime = emsgBox->getPresentationTime();
				timeScale = emsgBox->getTimeScale();
				eventDuration = emsgBox->getEventDuration();
				id = emsgBox->getId();
				schemeIdUri = emsgBox->getSchemeIdUri();
				value = emsgBox->getValue();
				ret = true;
				foundEmsg = true;
			}
		}
	}
	return ret;
}

/**
 *  @brief Get information from EMSG box
 */
bool IsoBmffBuffer::getEMSGData(uint8_t* &message, uint32_t &messageLen, char * &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id)
{
	bool foundEmsg = false;
	return getEMSGInfoInternal(&boxes, message, messageLen, schemeIdUri, value, presTime, timeScale, eventDuration, id, foundEmsg);
}

/**
 *  @brief get ISOBMFF box list of a type in a parsed buffer
 */
bool IsoBmffBuffer::getBoxesInternal(const std::vector<Box*> *boxes, const char *name, std::vector<Box*> *pBoxes)
{
	size_t size =boxes->size();
	//Adjust size when chunked box is available
	if(chunkedBox)
	{
		size -= 1;
	}
	for (size_t i = 0; i < size; i++)
	{
		Box *box = boxes->at(i);

		if (IS_TYPE(box->getType(), name))
		{
			pBoxes->push_back(box);
		}
	}
	return !!(pBoxes->size());
}

/**
 *  @brief Check mdat buffer count in parsed buffer
 */
bool IsoBmffBuffer::getMdatBoxCount(size_t &count)
{
	std::vector<Box*> mdatBoxes;
	bool bParse = false;
	bParse = getBoxesInternal(&boxes ,Box::MDAT, &mdatBoxes);
	count = mdatBoxes.size();
	return bParse;
}

/**
 * @brief Print ISOBMFF mdat boxes in parsed buffer
 */
void IsoBmffBuffer::printMdatBoxes()
{
	std::vector<Box*> mdatBoxes;
	(void)getBoxesInternal(&boxes ,Box::MDAT, &mdatBoxes);
	printBoxesInternal(&mdatBoxes);
}

/**
 * @brief Get list of box handle in parsed buffer using name
 */
bool IsoBmffBuffer::getTypeOfBoxes(const char *name, std::vector<Box*> &stBoxes)
{
	bool bParse = false;
	bParse = getBoxesInternal(&boxes ,name, &stBoxes);
	return bParse;
}

/**
 *  @brief Get list of box handles in a parsed buffer
 */
Box* IsoBmffBuffer::getChunkedfBox() const
{
	return this->chunkedBox;
}

/**
 *  @brief Get list of box handles in a parsed buffer
 */
std::vector<Box*> *IsoBmffBuffer::getParsedBoxes()
{
	return &this->boxes;
}

/**
 *  @brief Get list of box handles in a parsed buffer
 */
size_t IsoBmffBuffer::getParsedBoxesSize()
{
	return this->boxes.size();
}
/**
 *  @brief Get list of box handles in a parsed buffer
 */
bool IsoBmffBuffer::getChunkedfBoxMetaData(uint32_t &offset, std::string &type, uint32_t &size)
{
	bool ret = false;
	Box *pBox = getChunkedfBox();
	if(pBox)
	{
		offset = pBox->getOffset();
		type =  pBox->getType();
		size =  pBox->getSize();
		ret = true;
	}
	return ret;

}
/**
 *  @brief Get list of box handles in a parsed buffer
 */
int IsoBmffBuffer::UpdateBufferData(size_t parsedBoxCount, char* &unParsedBuffer, size_t &unParsedBufferSize, size_t & parsedBufferSize)
{
	std::vector<Box*> *pBoxes = getParsedBoxes();
	size_t mdatCount;
	int lastMDatIndex = -1;
	getMdatBoxCount(mdatCount);
	if(pBoxes && mdatCount)
	{
		//Get Last MDAT box
		for( int i=(int)parsedBoxCount-1; i>=0; i-- )
		{
			Box *box = pBoxes->at(i);
			if (IS_TYPE(box->getType(), Box::MDAT))
			{
				lastMDatIndex = i;

				AAMPLOG_TRACE("Last MDAT Index : %d", lastMDatIndex);

				//Calculate unparsed buffer based on last MDAT
				unParsedBuffer += (box->getOffset()+box->getSize()); //increment buffer pointer to chunk offset
				unParsedBufferSize -= (box->getOffset()+box->getSize()); //decrease by parsed buffer size

				parsedBufferSize -= unParsedBufferSize; //get parsed buf size
				AAMPLOG_TRACE("parsedBufferSize : %zu updated unParsedBufferSize: %zu Total Buf Size processed: %zu",parsedBufferSize,unParsedBufferSize,parsedBufferSize+unParsedBufferSize);
				break;
			}
		}
	}
	return lastMDatIndex;
}


/**
 *  @brief Get list of box handles in a parsed buffer
 */
double IsoBmffBuffer::getTotalChunkDuration(int lastMDatIndex)
{
	double totalChunkDuration = 0.0;
	uint64_t fDuration = 0;
	std::vector<Box*> *pBoxes = getParsedBoxes();
	for(int i=0;i<lastMDatIndex;i++)
	{
		Box *box = pBoxes->at(i);
		AAMPLOG_TRACE("Type: %s", box->getType());
		if (IS_TYPE(box->getType(), Box::MOOF))
		{
			getSampleDuration(box, fDuration);
			totalChunkDuration += fDuration;
			AAMPLOG_TRACE("fDuration = %" PRIu64 ", totalChunkDuration = %f", fDuration, totalChunkDuration);
		}
	}
	return totalChunkDuration;
}

/**
 *  @brief Get box handle in parsed buffer using name, starting at index
 */
Box*  IsoBmffBuffer::getBox(const char *name, size_t &index)
{
	Box *pBox = NULL;
	if (index >= boxes.size())
	{
		AAMPLOG_ERR("Index passed is too big (%zu >= %zu)", index, boxes.size());
	}
	else
	{
		for (size_t i = index; i < boxes.size(); i++)
		{
			pBox = boxes.at(i);
			if (IS_TYPE(pBox->getType(), name))
			{
				index = i;
				break;
			}
			pBox = NULL;
		}
	}
	return pBox;
}

/**
 *  @brief Get box handle in parsed buffer using index
 */
Box* IsoBmffBuffer::getBoxAtIndex(size_t index)
{
	if(index != -1)
		return boxes.at(index);
	else
		return NULL;
}

/**
 * @brief Get the Child Box object
 */
Box* IsoBmffBuffer::getChildBox(Box *parent, const char *name, size_t &index)
{
	Box *pBox{nullptr};

	if (parent && parent->hasChildren())
	{
		auto children{parent->getChildren()};
		for (size_t i = index; i < children->size(); ++i)
		{
			pBox = children->at(i);
			if (IS_TYPE(pBox->getType(), name))
			{
				index = i;
				break;
			}
			pBox = nullptr;
		}
	}
	return pBox;
}

/**
 *  @brief Print ISOBMFF box PTS
 */
void IsoBmffBuffer::printPTSInternal(const std::vector<Box*> *boxes)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);

		if (IS_TYPE(box->getType(), Box::TFDT))
		{
			TfdtBox *tfdtBox = dynamic_cast<TfdtBox *>(box);
			if(tfdtBox)
			{
				AAMPLOG_WARN("****Base Media Decode Time: %" PRIu64, tfdtBox->getBaseMDT());
			}
		}

		if (box->hasChildren())
		{
			printBoxesInternal(box->getChildren());
		}
	}
}

/**
 *  @brief Look for a specific box in a vector of boxes
 */
static Box *findBoxInVector(const char * box_type, const std::vector<Box*> *boxes)
{
	auto it = find_if(boxes->begin(), boxes->end(), [box_type](Box* b){ return IS_TYPE(b->getType(), box_type);});
	return (it != boxes->end()) ? *it : nullptr;
}

/**
 *  @brief Get ISOBMFF box Sample Duration
 */
uint64_t IsoBmffBuffer::getSampleDurationInternal(const std::vector<Box*> *boxes)
{
	if(boxes == nullptr)
		return 0;

	uint64_t duration = 0;

	auto sidx{dynamic_cast<SidxBox *>(findBoxInVector(Box::SIDX, boxes))};
	if(sidx)
	{
		// If present, SIDX duration takes precedence over TRUN/TFHD
		duration = sidx->getSampleDuration();
	}

	if(duration == 0)
	{
		auto traf{findBoxInVector(Box::TRAF, boxes)};
		if ((traf) && traf->hasChildren())
		{
			auto trunBox{dynamic_cast<TrunBox *>(findBoxInVector(Box::TRUN, traf->getChildren()))};
			if (trunBox)
			{
				duration = trunBox->getSampleDuration();
				if (0 == duration)
				{
					auto sample_count = trunBox->getSampleCount();
					auto tfhdBox{dynamic_cast<TfhdBox *>(findBoxInVector(Box::TFHD, traf->getChildren()))};
					if (tfhdBox)
					{
						duration = tfhdBox->getDefaultSampleDuration() * sample_count;
					}
				}
			}
		}
	}
	return duration;
}

/**
 *  @brief Get ISOBMFF box Sample Duration
 */
void IsoBmffBuffer::getSampleDuration(Box *box, uint64_t &fduration)
{
	fduration = 0;

	if (box && (box->hasChildren()))
	{
		fduration = getSampleDurationInternal(box->getChildren());
	}
}

/**
 *  @brief Get ISOBMFF box PTS
 */
uint64_t IsoBmffBuffer::getPtsInternal(const std::vector<Box*> *boxes)
{
	uint64_t retValue = 0;
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);

		if (IS_TYPE(box->getType(), Box::TFDT))
		{
			TfdtBox *tfdtBox =  dynamic_cast<TfdtBox *>(box);
			if(tfdtBox)
			{
				AAMPLOG_WARN("****Base Media Decode Time: %" PRIu64, tfdtBox->getBaseMDT());
				retValue = tfdtBox->getBaseMDT();
			}
			break;
		}

		if (box->hasChildren())
		{
			retValue = getPtsInternal(box->getChildren());
			break;
		}
	}
	return retValue;
}

/**
 *  @brief Get ISOBMFF box PTS
 */
void IsoBmffBuffer::getPts(Box *box, uint64_t &fpts)
{
	if (box->hasChildren())
	{
		fpts = getPtsInternal(box->getChildren());
	}
}

bool IsoBmffBuffer::updateSampleDurationInternal(uint64_t duration, TrunBox& trun, TfhdBox& tfhd)
{
	bool durationPresent{false};

	// If the SAMPLE_DURATION_PRESENT flag is set in the TRUN, the sample duration should be updated.
	// If the DEFAULT_SAMPLE_DURATION_PRESENT flag is set in the TFHD, the default sample duration should be updated.

	if (trun.sampleDurationPresent())
	{
		trun.setFirstSampleDuration(duration);
		durationPresent = true;
	}

	if (tfhd.defaultSampleDurationPresent())
	{
		tfhd.setDefaultSampleDuration(duration);
		durationPresent = true;
	}

	return durationPresent;
}

/**
 * @brief Truncate the mdat data to the first sample and update the tables in all relevant boxes
 *
*/
void IsoBmffBuffer::truncate(void)
{
	size_t index{0};
	// Find the moof.  NB there is no specific MoofBox implemented, it's just a generic container box
	uint64_t duration{};

	std::vector<TrunBox *>trunList{};
	SencBox *senc{};
	SaizBox *saiz{};
	TfhdBox *tfhd{};

	bool found {false};

	while(auto moof{getBox(Box::MOOF, index)})
	{
		uint64_t localDuration{};

		getSampleDuration(moof, localDuration);
		duration += localDuration;

		if(!found)
		{
			trunList.clear();
			senc = nullptr;
			saiz = nullptr;
			tfhd = nullptr;

			// Hierarchy - moof->traf->trun/saiz/senc/tfhd
			// trak & traf are also generic containers

			size_t localIndex{0};
			auto traf{getChildBox(moof, Box::TRAF, localIndex)};
			if (traf != nullptr)
			{
				localIndex = 0;
				while (auto trun{dynamic_cast<TrunBox *>(getChildBox(traf, Box::TRUN, localIndex))})
				{
					trunList.push_back(trun);
					++localIndex;
				}
				localIndex = 0;
				senc = dynamic_cast<SencBox *>(getChildBox(traf, Box::SENC, localIndex));
				localIndex = 0;
				saiz = dynamic_cast<SaizBox *>(getChildBox(traf, Box::SAIZ, localIndex));
				localIndex = 0;
				tfhd = dynamic_cast<TfhdBox *>(getChildBox(traf, Box::TFHD, localIndex));
			}

			if (!trunList.empty() && tfhd)
			{
				found = true;
			}
		}
		// Increment the index to continue searching from the next box
		++index;
	}

	if(found)
	{
		// Find the first mdat box
		size_t localIndex{0};
		auto mdat{dynamic_cast<MdatBox *>(getBox(Box::MDAT, localIndex))};
		if(mdat)
		{
			if (!updateSampleDurationInternal(duration, *trunList[0], *tfhd))
			{
				AAMPLOG_WARN("Sample duration not set");
			}

			bool setToSkip{false};

			// NB multiple trun boxes is theoretical
			for (auto trun : trunList)
			{
				if (!setToSkip)
				{
					// Truncate the first trun, erase subsequent
					trun->truncate();
					setToSkip = true;
					continue;
				}

				if(setToSkip)
				{
					// Replace TRUN box buffer data with a SKIP box
					// NOTE THAT THIS WILL NOT REMOVE THE TRUNBOX OBJECT, IT'S JUST THE BUFFER DATA THAT IS REPLACED
					// (and the type)
					trun->rewriteAsSkipBox();
				}
			}
			auto newMdatSize{std::max(trunList[0]->getFirstSampleSize(), tfhd->getDefaultSampleSize()) + SIZEOF_SIZE_AND_TAG};

			// May not have been located
			uint32_t firstSampleSize{0};
			if (saiz)
			{
				firstSampleSize = saiz->getFirstSampleInfoSize();
				saiz->truncate();
			}
			if (senc)
			{
				senc->truncate(firstSampleSize);
			}

			mdat->truncate(newMdatSize);

			// Change buffer size
			bufSize = size_t(mdat->getOffset() + newMdatSize);
		}
	}
}

/**
 * @brief Get the total segment duration in units of timescale.
 *
*/
uint64_t IsoBmffBuffer::getSegmentDuration()
{
	size_t parsedBoxCount = 0;
	uint64_t fDuration = 0;
	uint64_t totalChunkDuration = 0;

	parsedBoxCount = boxes.size();

	if (parsedBoxCount)
	{
		int lastMDatIndex = -1;
		//Get Last MDAT box
		for( int i=(int)parsedBoxCount-1; i>=0; i-- )
		{
			Box *box = boxes.at(i);
			if (IS_TYPE(box->getType(), Box::MDAT))
			{
				lastMDatIndex = i;
				break;
			}
		}

		for(int i=0;i<lastMDatIndex;i++)
		{
			Box *box = boxes.at(i);
			if (IS_TYPE(box->getType(), Box::MOOF))
			{
				getSampleDuration(box, fDuration);
				totalChunkDuration += fDuration;
			}
		}
	}

	return totalChunkDuration;
}

/**
 * @brief Find the MDHD & MVHD boxes and set the timescale
 *
*/
bool IsoBmffBuffer::setTrickmodeTimescale(uint32_t timescale)
{
	bool retval{false};
	size_t index{0};

	auto moov{getBox(Box::MOOV, index)};

	if (moov != nullptr)
	{
		index = 0;
		auto mvhd{dynamic_cast<MvhdBox *>(getChildBox(moov, Box::MVHD, index))};

		index = 0;
		auto trak{getChildBox(moov, Box::TRAK, index)};

		if (trak != nullptr)
		{
			index = 0;
			auto mdia {getChildBox(trak, Box::MDIA, index) };

			if (mdia != nullptr)
			{
				index = 0;
				auto mdhd{dynamic_cast<MdhdBox *>(getChildBox(mdia, Box::MDHD, index))};

				if (mdhd != nullptr && mvhd != nullptr)
				{
					AAMPLOG_INFO("Set mdhd & mvhd timescale to %d", timescale);
					mdhd->setTimeScale(timescale);
					mvhd->setTimeScale(timescale);
					retval = true;
				}
				else
				{
					// Both boxes are mandatory, so this should never happen
					if (mdhd == nullptr)
					{
						AAMPLOG_WARN("mdhd box not found in mdia box");
					}
					if (mvhd == nullptr)
					{
						AAMPLOG_WARN("mvhd box not found in moov box");
					}
				}
			}
			else
			{
				AAMPLOG_WARN("mdia box not found in trak box");
			}
		}
		else
		{
			AAMPLOG_WARN("trak box not found in moov box");
		}
	}
	else
	{
		AAMPLOG_WARN("No MOOV box within buffer");
	}

	return retval;
}

bool IsoBmffBuffer::setMediaHeaderDuration(uint64_t duration)
{
	bool retval{false};
	size_t index{0};
	auto moov{getBox(Box::MOOV, index)};

	if (moov != nullptr)
	{
		index = 0;
		auto trak{getChildBox(moov, Box::TRAK, index)};

		if (trak != nullptr)
		{
			index = 0;
			auto mdia {getChildBox(trak, Box::MDIA, index) };

			if (mdia != nullptr)
			{
				index = 0;
				auto mdhd{dynamic_cast<MdhdBox *>(getChildBox(mdia, Box::MDHD, index))};

				if (mdhd != nullptr)
				{
					AAMPLOG_INFO("Setting mdhd duration from %" PRIu64 " to %" PRIu64, mdhd->getDuration(), duration);
					mdhd->setDuration(duration);
					retval = true;
				}
			}
			else
			{
				AAMPLOG_WARN("mdia box not found in trak box");
			}
		}
		else
		{
			AAMPLOG_WARN("trak box not found in moov box");
		}
	}
	else
	{
		AAMPLOG_WARN("No MOOV box within buffer");
	}
	return retval;
}
