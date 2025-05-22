/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#include "playerisobmffbuffer.h"
#include <string.h>

#define READ_BMDT64(buf) \
		ReadUint64(buf); buf+=8;

#define READ_64(buf) \
		ReadUint64(buf); buf+=8;

#define IS_TYPE(value, type) \
		(value[0]==type[0] && value[1]==type[1] && value[2]==type[2] && value[3]==type[3])

/**
 *  @brief IsoBmffBuffer destructor
 */
PlayerIsoBmffBuffer::~PlayerIsoBmffBuffer()
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
void PlayerIsoBmffBuffer::setBuffer(uint8_t *buf, size_t sz)
{
	buffer = buf;
	bufSize = sz;
}

/**
 *	@fn parseBuffer
 *  @param[in] correctBoxSize - flag to correct the box size
 *	@param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
 *  @brief Parse ISOBMFF boxes from buffer
 */
bool PlayerIsoBmffBuffer::parseBuffer(bool correctBoxSize, int newTrackId)
{
	size_t curOffset = 0;
	while (curOffset < bufSize)
	{
		player_isobmff::IsoBmffBox *box = player_isobmff::IsoBmffBox::constructBox(buffer+curOffset, (uint32_t)(bufSize - curOffset), correctBoxSize, newTrackId);
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
 *  @brief Get list of box handles in a parsed buffer
 */
player_isobmff::IsoBmffBox* PlayerIsoBmffBuffer::getChunkedfBox() const
{
	return this->chunkedBox;
}

/**
 *  @brief Print ISOBMFF boxes
 */
void PlayerIsoBmffBuffer::printBoxesInternal(const std::vector<player_isobmff::IsoBmffBox*> *boxes)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		player_isobmff::IsoBmffBox *box = boxes->at(i);
		MW_LOG_WARN("Offset[%u] Type[%s] Size[%u]", box->getOffset(), box->getType(), box->getSize());
		if (IS_TYPE(box->getType(), player_isobmff::IsoBmffBox::TFDT))
		{
			player_isobmff::TfdtIsoBmffBox *tfdtBox = dynamic_cast<TfdtIsoBmffBox *>(box);
			if(tfdtBox) {
				MW_LOG_WARN("****Base Media Decode Time: %" PRIu64, tfdtBox->getBaseMDT());
			}
		}
		else if (IS_TYPE(box->getType(), player_isobmff::IsoBmffBox::MVHD))
		{
			player_isobmff::MvhdIsoBmffBox *mvhdBox = dynamic_cast<MvhdIsoBmffBox *>(box);
			if(mvhdBox) {
				MW_LOG_WARN("**** TimeScale from MVHD: %u", mvhdBox->getTimeScale());
			}
		}
		else if (IS_TYPE(box->getType(), player_isobmff::IsoBmffBox::MDHD))
		{
			player_isobmff::MdhdIsoBmffBox *mdhdBox = dynamic_cast<MdhdIsoBmffBox *>(box);
			if(mdhdBox) {
				MW_LOG_WARN("**** TimeScale from MDHD: %u", mdhdBox->getTimeScale());
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
void PlayerIsoBmffBuffer::printBoxes()
{
	printBoxesInternal(&boxes);
}

/**
 *  @brief Check if buffer is an initialization segment
 */
bool PlayerIsoBmffBuffer::isInitSegment()
{
	bool foundFtypBox = false;
	for (size_t i = 0; i < boxes.size(); i++)
	{
		player_isobmff::IsoBmffBox *box = boxes.at(i);
		if (IS_TYPE(box->getType(), player_isobmff::IsoBmffBox::FTYP))
		{
			foundFtypBox = true;
			break;
		}
	}
	return foundFtypBox;
}

/**
 *  @brief Get mdat buffer size
 */
bool PlayerIsoBmffBuffer::getMdatBoxSize(size_t &size)
{
	return getBoxSizeInternal(&boxes, player_isobmff::IsoBmffBox::MDAT, size);
}

/**
 *  @brief get ISOBMFF box size of a type
 */
bool PlayerIsoBmffBuffer::getBoxSizeInternal(const std::vector<player_isobmff::IsoBmffBox*> *boxes, const char *name, size_t &size)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		player_isobmff::IsoBmffBox *box = boxes->at(i);
		if (IS_TYPE(box->getType(), name))
		{
			size = box->getSize();
			return true;
		}
	}
	return false;
}

/**
 *  @brief Get mdat buffer handle and size from parsed buffer
 */
bool PlayerIsoBmffBuffer::parseMdatBox(uint8_t *buf, size_t &size)
{
	return parseBoxInternal(&boxes, player_isobmff::IsoBmffBox::MDAT, buf, size);
}

#define BOX_HEADER_SIZE 8

/**
 *  @brief parse ISOBMFF boxes of a type in a parsed buffer
 */
bool PlayerIsoBmffBuffer::parseBoxInternal(const std::vector<player_isobmff::IsoBmffBox*> *boxes, const char *name, uint8_t *buf, size_t &size)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		player_isobmff::IsoBmffBox *box = boxes->at(i);
		MW_LOG_TRACE("Offset[%u] Type[%s] Size[%u]", box->getOffset(), box->getType(), box->getSize());
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
