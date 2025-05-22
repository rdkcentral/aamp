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
* @file playerisobmffbuffer.h
* @brief Header file for ISO Base Media File Format Buffer
*/

#ifndef __PLAYERISOBMFFBUFFER_H__
#define __PLAYERISOBMFFBUFFER_H__

#include "playerisobmffbox.h"
#include <stddef.h>
#include <vector>
#include <string>
#include <cstdint>
#include <inttypes.h>

using namespace player_isobmff;

/**
 * @class PlayerIsoBmffBuffer
 * @brief Class for ISO BMFF Buffer
 */
class PlayerIsoBmffBuffer
{
private:
	std::vector<player_isobmff::IsoBmffBox*> boxes;	//ISOBMFF boxes of associated buffer
	uint8_t *buffer;
	size_t bufSize;
	player_isobmff::IsoBmffBox* chunkedBox; //will hold one element only
	size_t mdatCount;

	/**
	 * @fn printBoxesInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @return void
	 */
	void printBoxesInternal(const std::vector<player_isobmff::IsoBmffBox*> *boxes);

	/**
	 * @fn parseBoxInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[in] name - box name to get
	 * @param[out] buf - mdat buffer pointer
	 * @param[out] size - size of mdat buffer
	 * @return bool
	 */
	bool parseBoxInternal(const std::vector<player_isobmff::IsoBmffBox*> *boxes, const char *name, uint8_t *buf, size_t &size);

	/**
	 * @fn getBoxSizeInternal
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[in] name - box name to get
	 * @param[out] size - size of mdat buffer
	 * @return bool
	 */
	bool getBoxSizeInternal(const std::vector<player_isobmff::IsoBmffBox*> *boxes, const char *name, size_t &size);

public:
	/**
	 * @brief PlayerIsoBmffBuffer constructor
	 */
	PlayerIsoBmffBuffer(): boxes(), buffer(NULL), bufSize(0), chunkedBox(NULL), mdatCount(0)
	{
	}

	/**
	 * @fn ~PlayerIsoBmffBuffer
	 */
	~PlayerIsoBmffBuffer();

	PlayerIsoBmffBuffer(const PlayerIsoBmffBuffer&) = delete;
	PlayerIsoBmffBuffer& operator=(const PlayerIsoBmffBuffer&) = delete;
	
	/**
	 * @fn getChunkedfBox
	 *
	 * @return Box handle if Chunk box found in a parsed buffer. NULL otherwise
	 */
	player_isobmff::IsoBmffBox* getChunkedfBox() const;

	/**
	 * @fn UpdateBufferData
	 * @return true if parsed or false
	 */
	int UpdateBufferData(size_t parsedBoxCount, char* &unParsedBuffer, size_t &unParsedBufferSize, size_t & parsedBufferSize);

	/**
	 * @fn setBuffer
	 *
	 * @param[in] buf - buffer pointer
	 * @param[in] sz - buffer size
	 * @return void
	 */
	void setBuffer(uint8_t *buf, size_t sz);

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
};

#endif /* __PLAYERISOBMFFBUFFER_H__ */
