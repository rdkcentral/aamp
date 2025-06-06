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
* @file isobmffbox.cpp
* @brief Source file for ISO Base Media File Format Boxes
*/

#include "playerisobmffbox.h"
#include <stddef.h>
#include <inttypes.h>

#define READ_BMDT64(buf) \
		ReadUint64FromBuffer(buf); buf+=8;

#define READ_64(buf) \
		ReadUint64FromBuffer(buf); buf+=8;

#define IS_TYPE(value, type) \
		(value[0]==type[0] && value[1]==type[1] && value[2]==type[2] && value[3]==type[3])

const uint32_t TRUN_FLAG_DATA_OFFSET_PRESENT                    = 0x0001;
const uint32_t TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT             = 0x0004;
const uint32_t TRUN_FLAG_SAMPLE_DURATION_PRESENT                = 0x0100;
const uint32_t TRUN_FLAG_SAMPLE_SIZE_PRESENT                    = 0x0200;
const uint32_t TRUN_FLAG_SAMPLE_FLAGS_PRESENT                   = 0x0400;
const uint32_t TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT = 0x0800;

const uint32_t TFHD_FLAG_BASE_DATA_OFFSET_PRESENT               = 0x00001;
const uint32_t TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT       = 0x00002;
const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT        = 0x00008;
const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT            = 0x00010;
// const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT           = 0x00020;
// const uint32_t TFHD_FLAG_DURATION_IS_EMPTY                   = 0x10000;
// const uint32_t TFHD_FLAG_DEFAULT_BASE_IS_MOOF                = 0x20000;

const uint32_t SAIZ_FLAG_AUX_INFO_TYPE_PRESENT                  = 0x0001;

/**
 *  @brief Read a string from buffer and return it
 */
int ReadCStringLenFromBuffer(const uint8_t* buffer, uint32_t bufferLen)
{
	int retLen = -1;
	if(buffer && bufferLen > 0)
	{
		for(int i=0; i < bufferLen; i++)
		{
			if(buffer[i] == '\0')
			{
				retLen = i+1;
				break;
			}
		}
	}
	return retLen;
}

/**
 *  @brief Utility function to read 8 bytes from a buffer
 */
uint64_t ReadUint64FromBuffer(uint8_t *buf)
{
	uint64_t val = PLAYER_READ_U32(buf);
	val = (val<<32) | (uint32_t)PLAYER_READ_U32(buf);
	return val;
}

/**
 *  @brief Utility function to write 8 bytes to a buffer
 */
void WriteUint64ToBuffer(uint8_t *dst, uint64_t val)
{
	uint32_t msw = (uint32_t)(val>>32);
	PLAYER_WRITE_U32(dst, msw); dst+=4;
	PLAYER_WRITE_U32(dst, val);
}

namespace player_isobmff {
/**
 *  @brief Box constructor
 */
IsoBmffBox::IsoBmffBox(uint32_t sz, const char btype[4]) : offset(0), size(sz), type{}, base(nullptr)
{
	memcpy(type,btype,4);
}

/**
 *  @brief Set box's offset from the beginning of the buffer
 */
void IsoBmffBox::setOffset(uint32_t os)
{
	offset = os;
}

/**
 *  @brief Get box offset
 */
uint32_t IsoBmffBox::getOffset() const
{
	return offset;
}

/**
 *  @brief To check if box has any child boxes
 */
bool IsoBmffBox::hasChildren() const
{
	return false;
}

/**
 *  @brief Get children of this box
 */
const std::vector<IsoBmffBox*> *IsoBmffBox::getChildren() const
{
	return NULL;
}

/**
 *  @brief Get box size
 */
uint32_t IsoBmffBox::getSize() const
{
	return size;
}

/**
 *  @brief Get box type
 */
const char *IsoBmffBox::getType() const
{
	return type;
}

/**
 * @fn constructBox
 * @brief Static function to construct a Box object
 * @param[in] hdr - pointer to box
 * @param[in] maxSz - box size
 * @param[in] correctBoxSize - flag to check if box size needs to be corrected
 * @param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
 * @return newly constructed Box object
 */
IsoBmffBox* IsoBmffBox::constructBox(uint8_t *hdr, uint32_t maxSz, bool correctBoxSize, int newTrackId)
{
	L_RESTART:
	uint8_t *hdr_start = hdr;

	uint32_t size = 0;
	uint8_t type[5];
	if(maxSz < 4)
	{
		MW_LOG_TRACE("Box data < 4 bytes. Can't determine Size & Type");
		return new IsoBmffBox(maxSz, (const char *)"UKWN");
	}
	else if(maxSz >= 4 && maxSz < 8)
	{
		MW_LOG_TRACE("Box Size between >4 but <8 bytes. Can't determine Type");
		//size = PLAYER_READ_U32(hdr);
		return new IsoBmffBox(maxSz, (const char *)"UKWN");
	}
	else
	{
		size = PLAYER_READ_U32(hdr);
		PLAYER_READ_U8(type, hdr, 4);
		type[4] = '\0';
	}

	if (size > maxSz)
	{
		if(correctBoxSize)
		{
			//Fix box size to handle cases like receiving whole file for HTTP range requests
			MW_LOG_WARN("Box[%s] fixing size error:size[%u] > maxSz[%u]", type, size, maxSz);
			hdr = hdr_start;
			PLAYER_WRITE_U32(hdr,maxSz);
			goto L_RESTART;
		}
		else
		{
#ifdef PLAYER_DEBUG_BOX_CONSTRUCT
			MW_LOG_WARN("Box[%s] Size error:size[%u] > maxSz[%u]",type, size, maxSz);
#endif
		}
	}
	else if (IS_TYPE(type, MOOV))
	{
		return GenericContainerIsoBmffBox::constructContainer(size, MOOV, hdr, newTrackId);
	}
	else if (IS_TYPE(type, TRAK))
	{
		return TrakIsoBmffBox::constructTrakBox(size,hdr, newTrackId);
	}
	else if (IS_TYPE(type, MDIA))
	{
		return GenericContainerIsoBmffBox::constructContainer(size, MDIA, hdr, newTrackId);
	}
	else if (IS_TYPE(type, MOOF))
	{
		return GenericContainerIsoBmffBox::constructContainer(size, MOOF, hdr, newTrackId);
	}
	else if (IS_TYPE(type, TRAF))
	{
		return GenericContainerIsoBmffBox::constructContainer(size, TRAF, hdr, newTrackId);
	}
	else if (IS_TYPE(type, TFHD))
	{
		return TfhdIsoBmffBox::constructTfhdBox(size,  hdr);
	}
	else if (IS_TYPE(type, TFDT))
	{
		return TfdtIsoBmffBox::constructTfdtBox(size,  hdr);
	}
	else if (IS_TYPE(type, TRUN))
	{
		return TrunIsoBmffBox::constructTrunBox(size,  hdr);
	}
	else if (IS_TYPE(type, MVHD))
	{
		return MvhdIsoBmffBox::constructMvhdBox(size,  hdr);
	}
	else if (IS_TYPE(type, MDHD))
	{
		return MdhdIsoBmffBox::constructMdhdBox(size,  hdr);
	}
	else if (IS_TYPE(type, EMSG))
	{
		return EmsgIsoBmffBox::constructEmsgBox(size, hdr);
	}
	else if (IS_TYPE(type, PRFT))
	{
		return PrftIsoBmffBox::constructPrftBox(size,  hdr);
	}
	else if( IS_TYPE(type, SIDX) )
	{
		return SidxIsoBmffBox::constructSidxBox(size, hdr);
	}
	else if (IS_TYPE(type, MDAT))
	{
		return MdatIsoBmffBox::constructMdatBox(size, hdr);
	}
	else if (IS_TYPE(type, SENC))
	{
		return SencIsoBmffBox::constructSencBox(size, hdr);
	}
	else if (IS_TYPE(type, SAIZ))
	{
		return SaizIsoBmffBox::constructSaizBox(size, hdr);
	}

	return new IsoBmffBox(size, (const char *)type);
}

/**
 * @brief rewriteAsSkipBox - overwrite buffer data and rewrte box type as skip
 * @params none
 * @return none
 */
void IsoBmffBox::rewriteAsSkipBox(void)
{
	// buffer
	memcpy(base + 4, IsoBmffBox::SKIP, 4);
	// internal type
	memcpy(type, IsoBmffBox::SKIP, 5);
}

/**
 *  @brief GenericContainerBox constructor
 */
GenericContainerIsoBmffBox::GenericContainerIsoBmffBox(uint32_t sz, const char btype[4]) : IsoBmffBox(sz, btype), children()
{

}

/**
 *  @brief GenericContainerBox destructor
 */
GenericContainerIsoBmffBox::~GenericContainerIsoBmffBox()
{
	for (unsigned int i = (unsigned int)children.size(); i>0;)
	{
		--i;
		SAFE_DELETE(children.at(i));
		children.pop_back();
	}
	children.clear();
}

/**
 *  @brief Add a box as a child box
 */
void GenericContainerIsoBmffBox::addChildren(IsoBmffBox *box)
{
	children.push_back(box);
}

/**
 *  @brief To check if box has any child boxes
 */
bool GenericContainerIsoBmffBox::hasChildren() const
{
	return true;
}

/**
 *  @brief Get children of this box
 */
const std::vector<IsoBmffBox*> *GenericContainerIsoBmffBox::getChildren() const
{
	return &children;
}


/**
 * @fn constructContainer
 * @brief Static function to construct a GenericContainerBox object
 * @param[in] sz - box size
 * @param[in] btype - box type
 * @param[in] ptr - pointer to box
 * @param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
 * @return newly constructed GenericContainerBox object
 */
GenericContainerIsoBmffBox* GenericContainerIsoBmffBox::constructContainer(uint32_t sz, const char btype[4], uint8_t *ptr,   int newTrackId)
{
	GenericContainerIsoBmffBox *cbox = new GenericContainerIsoBmffBox(sz, btype);
	uint32_t curOffset = sizeof(uint32_t) + sizeof(uint32_t); //Sizes of size & type fields
	while (curOffset < sz)
	{
		IsoBmffBox *box = IsoBmffBox::constructBox(ptr, sz-curOffset, false, newTrackId );
		box->setOffset(curOffset);
		cbox->addChildren(box);
		curOffset += box->getSize();
		ptr += box->getSize();
	}
	return cbox;
}

/**
 *  @brief FullBox constructor
 */
FullIsoBmffBox::FullIsoBmffBox(uint32_t sz, const char btype[4], uint8_t ver, uint32_t f) : IsoBmffBox(sz, btype), version(ver), flags(f)
{

}

/**
 *  @brief MvhdBox constructor
 */
MvhdIsoBmffBox::MvhdIsoBmffBox(uint32_t sz, uint32_t tScale, uint8_t* tScale_loc) : FullIsoBmffBox(sz, IsoBmffBox::MVHD, 0, 0), timeScale(tScale), timeScale_loc(tScale_loc)
{

}

/**
 *  @brief MvhdBox constructor
 */
MvhdIsoBmffBox::MvhdIsoBmffBox(FullIsoBmffBox &fbox, uint32_t tScale, uint8_t* tScale_loc) : FullIsoBmffBox(fbox), timeScale(tScale), timeScale_loc(tScale_loc)
{

}

/**
 *  @brief Set TimeScale value
 */
void MvhdIsoBmffBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
	if (nullptr != timeScale_loc)
	{
		PLAYER_WRITE_U32(timeScale_loc, tScale);
	}
}

/**
 *  @brief Get TimeScale value
 */
uint32_t MvhdIsoBmffBox::getTimeScale()
{
	return timeScale;
}

/**
 *  @brief Static function to construct a MvhdIsoBmffBox object
 */
MvhdIsoBmffBox* MvhdIsoBmffBox::constructMvhdBox(uint32_t sz, uint8_t *ptr)
{
	auto start{ptr};
	uint8_t version = PLAYER_READ_VERSION(ptr);
	uint32_t flags  = PLAYER_READ_FLAGS(ptr);
	uint32_t tScale;

	uint32_t skip = sizeof(uint32_t)*2;
	if (1 == version)
	{
		//Skipping creation_time &modification_time
		skip = sizeof(uint64_t)*2;
	}
	ptr += skip;

	uint8_t* tScale_loc{ptr};
	tScale = PLAYER_READ_U32(ptr);

	FullIsoBmffBox fbox(sz, IsoBmffBox::MVHD, version, flags);
	fbox.setBase(start);
	return new MvhdIsoBmffBox(fbox, tScale, tScale_loc);
}

/**
 *  @brief MdhdBox constructor
 */
MdhdIsoBmffBox::MdhdIsoBmffBox(uint32_t sz, uint32_t tScale, uint8_t* tScale_loc) : FullIsoBmffBox(sz, IsoBmffBox::MDHD, 0, 0), timeScale(tScale), timeScale_loc(tScale_loc)
{

}

/**
 *  @brief MdhdIsoBmffBox constructor
 */
MdhdIsoBmffBox::MdhdIsoBmffBox(FullIsoBmffBox &fbox, uint32_t tScale, uint8_t* tScale_loc) : FullIsoBmffBox(fbox), timeScale(tScale), timeScale_loc(tScale_loc)
{

}

/**
 *  @brief Set TimeScale value
 */
void MdhdIsoBmffBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
	if (nullptr != timeScale_loc)
	{
		PLAYER_WRITE_U32(timeScale_loc, tScale);
	}
}

/**
 *  @brief Get TimeScale value
 */
uint32_t MdhdIsoBmffBox::getTimeScale()
{
	return timeScale;
}

/**
 *  @brief Static function to construct a MdhdIsoBmffBox object
 */
MdhdIsoBmffBox* MdhdIsoBmffBox::constructMdhdBox(uint32_t sz, uint8_t *ptr)
{
	auto start{ptr};
	uint8_t version = PLAYER_READ_VERSION(ptr);
	uint32_t flags  = PLAYER_READ_FLAGS(ptr);
	uint32_t tScale;

	uint32_t skip = sizeof(uint32_t)*2;
	if (1 == version)
	{
		//Skipping creation_time &modification_time
		skip = sizeof(uint64_t)*2;
	}
	ptr += skip;

	uint8_t* tScale_loc{ptr};
	tScale = PLAYER_READ_U32(ptr);

	FullIsoBmffBox fbox(sz, IsoBmffBox::MDHD, version, flags);
	fbox.setBase(start);
	return new MdhdIsoBmffBox(fbox, tScale, tScale_loc);
}

/**
 * @brief TfdtIsoBmffBox constructor
 */
TfdtIsoBmffBox::TfdtIsoBmffBox(uint32_t sz, uint64_t mdt, uint8_t* mdt_loc) : FullIsoBmffBox(sz, IsoBmffBox::TFDT, 0, 0), baseMDT(mdt), baseMDT_loc(mdt_loc)
{

}

/**
 *  @brief TfdtIsoBmffBox constructor
 */
TfdtIsoBmffBox::TfdtIsoBmffBox(FullIsoBmffBox &fbox, uint64_t mdt, uint8_t* mdt_loc) : FullIsoBmffBox(fbox), baseMDT(mdt), baseMDT_loc(mdt_loc)
{

}

/**
 *  @brief Set BaseMediaDecodeTime value
 */
void TfdtIsoBmffBox::setBaseMDT(uint64_t mdt)
{
	baseMDT = mdt;
	if (nullptr != baseMDT_loc)
	{
		if (1 == version)
		{
			WriteUint64ToBuffer(baseMDT_loc, mdt);
		}
		else
		{
			PLAYER_WRITE_U32(baseMDT_loc, static_cast<uint32_t>(mdt));
		}
	}
}

/**
 *  @brief Get BaseMediaDecodeTime value
 */
uint64_t TfdtIsoBmffBox::getBaseMDT()
{
	return baseMDT;
}

/**
 *  @brief Static function to construct a TfdtIsoBmffBox object
 */
TfdtIsoBmffBox* TfdtIsoBmffBox::constructTfdtBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = PLAYER_READ_VERSION(ptr);
	uint32_t flags  = PLAYER_READ_FLAGS(ptr);
	uint8_t* mdt_loc{ptr};
	uint64_t mdt;

	if (1 == version)
	{
		mdt = READ_BMDT64(ptr);
	}
	else
	{
		mdt = (uint32_t)PLAYER_READ_U32(ptr);
	}
	FullIsoBmffBox fbox(sz, IsoBmffBox::TFDT, version, flags);
	return new TfdtIsoBmffBox(fbox, mdt, mdt_loc);
}

/**
 *  @brief EmsgIsoBmffBox constructor
 */
EmsgIsoBmffBox::EmsgIsoBmffBox(uint32_t sz, uint32_t tScale, uint32_t evtDur, uint32_t _id) : FullIsoBmffBox(sz, IsoBmffBox::EMSG, 0, 0)
	, timeScale(tScale), eventDuration(evtDur), id(_id)
	, presentationTimeDelta(0), presentationTime(0)
	, schemeIdUri(nullptr), value(nullptr), messageData(nullptr), messageLen(0)
{

}

/**
 *  @brief EmsgIsoBmffBox constructor
 */
EmsgIsoBmffBox::EmsgIsoBmffBox(FullIsoBmffBox &fbox, uint32_t tScale, uint32_t evtDur, uint32_t _id, uint64_t presTime, uint32_t presTimeDelta) : FullIsoBmffBox(fbox)
	, timeScale(tScale), eventDuration(evtDur), id(_id)
	, presentationTimeDelta(presTimeDelta), presentationTime(presTime)
	, schemeIdUri(nullptr), value(nullptr), messageData(nullptr), messageLen(0)
{

}

/**
 *  @brief EmsgIsoBmffBox dtor
 */
EmsgIsoBmffBox::~EmsgIsoBmffBox()
{
	if (messageData)
	{
		free(messageData);
	}

	if (schemeIdUri)
	{
		free(schemeIdUri);
	}

	if (value)
	{
		free(value);
	}
}

/**
 *  @brief Set TimeScale value
 */
void EmsgIsoBmffBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}

/**
 *  @brief Get schemeIdUri
 */
uint32_t EmsgIsoBmffBox::getTimeScale()
{
	return timeScale;
}

/**
 *  @brief Set eventDuration value
 */
void EmsgIsoBmffBox::setEventDuration(uint32_t evtDur)
{
	eventDuration = evtDur;
}

/**
 *  @brief Get eventDuration
 */
uint32_t EmsgIsoBmffBox::getEventDuration()
{
	return eventDuration;
}

/**
 *  @brief Set id
 */
void EmsgIsoBmffBox::setId(uint32_t _id)
{
	id = _id;
}

/**
 *  @brief Get id
 */
uint32_t EmsgIsoBmffBox::getId()
{
	return id;
}

/**
 *  @brief Set presentationTimeDelta
 */
void EmsgIsoBmffBox::setPresentationTimeDelta(uint32_t presTimeDelta)
{
	presentationTimeDelta = presTimeDelta;
}

/**
 *  @brief Get presentationTimeDelta
 */
uint32_t EmsgIsoBmffBox::getPresentationTimeDelta()
{
	return presentationTimeDelta;
}

/**
 *  @brief Set presentationTime
 */
void EmsgIsoBmffBox::setPresentationTime(uint64_t presTime)
{
	presentationTime = presTime;
}

/**
 *  @brief Get presentationTime
 */
uint64_t EmsgIsoBmffBox::getPresentationTime()
{
	return presentationTime;
}

/**
 *  @brief Set schemeIdUri
 */
void EmsgIsoBmffBox::setSchemeIdUri(char * scheme_uri)
{
	schemeIdUri = scheme_uri;
}

/**
 *  @brief Get schemeIdUri
 */
char * EmsgIsoBmffBox::getSchemeIdUri() const
{
	return schemeIdUri;
}

/**
 *  @brief Set value
 */
void EmsgIsoBmffBox::setValue(uint8_t* schemeIdValue)
{
	value = schemeIdValue;
}

/**
 *  @brief Get value
 */
uint8_t* EmsgIsoBmffBox::getValue()
{
	return value;
}

/**
 *  @brief Set Message
 */
void EmsgIsoBmffBox::setMessage(uint8_t* message, uint32_t len)
{
	messageData = message;
	messageLen = len;
}

/**
 *  @brief Get Message
 */
uint8_t* EmsgIsoBmffBox::getMessage()
{
	return messageData;
}

/**
 *  @brief Get Message length
 */
uint32_t EmsgIsoBmffBox::getMessageLen()
{
	return messageLen;
}

/**
 *  @brief Static function to construct a EmsgIsoBmffBox object
 */
EmsgIsoBmffBox* EmsgIsoBmffBox::constructEmsgBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = PLAYER_READ_VERSION(ptr);
	uint32_t flags  = PLAYER_READ_FLAGS(ptr);
	// Calculating remaining size,
	// flags(3bytes)+ version(1byte)+ box_header(type and size)(8bytes)
	uint32_t remainingSize = sz - ((sizeof(uint32_t))+(sizeof(uint64_t)));

	uint64_t presTime = 0;
	uint32_t presTimeDelta = 0;
	uint32_t tScale;
	uint32_t evtDur;
	uint32_t boxId;

	char * schemeId = nullptr;
	uint8_t* schemeIdValue = nullptr;

	uint8_t* message = nullptr;
	FullIsoBmffBox fbox(sz, IsoBmffBox::EMSG, version, flags);

	/*
	 * Extraction is done as per https://aomediacodec.github.io/id3-emsg/
	 */
	if (1 == version)
	{
		tScale = PLAYER_READ_U32(ptr);
		// Read 64 bit value
		presTime = READ_64(ptr);
		evtDur = PLAYER_READ_U32(ptr);
		boxId = PLAYER_READ_U32(ptr);
		remainingSize -=  ((sizeof(uint32_t)*3) + sizeof(uint64_t));
		int schemeIdLen = ReadCStringLenFromBuffer(ptr, remainingSize);
		if(schemeIdLen > 0)
		{
			schemeId = (char*) malloc(sizeof(char)*schemeIdLen);
			PLAYER_READ_U8(schemeId, ptr, schemeIdLen);
			remainingSize -= (sizeof(uint8_t) * schemeIdLen);
			int schemeIdValueLen = ReadCStringLenFromBuffer(ptr, remainingSize);
			if (schemeIdValueLen > 0)
			{
				schemeIdValue = (uint8_t*) malloc(sizeof(uint8_t)*schemeIdValueLen);
				PLAYER_READ_U8(schemeIdValue, ptr, schemeIdValueLen);
				remainingSize -= (sizeof(uint8_t) * schemeIdValueLen);
			}
		}
	}
	else if(0 == version)
	{
		int schemeIdLen = ReadCStringLenFromBuffer(ptr, remainingSize);
		if(schemeIdLen > 0)
		{
			schemeId = (char*) malloc(sizeof(char)*schemeIdLen);
			PLAYER_READ_U8(schemeId, ptr, schemeIdLen);
			remainingSize -= (sizeof(uint8_t) * schemeIdLen);
			int schemeIdValueLen = ReadCStringLenFromBuffer(ptr, remainingSize);
			if (schemeIdValueLen > 0)
			{
				schemeIdValue = (uint8_t*) malloc(sizeof(uint8_t)*schemeIdValueLen);
				PLAYER_READ_U8(schemeIdValue, ptr, schemeIdValueLen);
				remainingSize -= (sizeof(uint8_t) * schemeIdValueLen);
			}
		}
		tScale = PLAYER_READ_U32(ptr);
		presTimeDelta = PLAYER_READ_U32(ptr);
		evtDur = PLAYER_READ_U32(ptr);
		boxId = PLAYER_READ_U32(ptr);
		remainingSize -= (sizeof(uint32_t)*4);
	}
	else
	{
		MW_LOG_WARN("Unsupported emsg box version");
		return new EmsgIsoBmffBox(fbox, 0, 0, 0, 0, 0);
	}

	EmsgIsoBmffBox* retBox = new EmsgIsoBmffBox(fbox, tScale, evtDur, boxId, presTime, presTimeDelta);
	if(retBox)
	{
		// Extract remaining message
		if(remainingSize > 0)
		{
			message = (uint8_t*) malloc(sizeof(uint8_t)*remainingSize);
			PLAYER_READ_U8(message, ptr, remainingSize);
			if(message)
			{
				retBox->setMessage(message, remainingSize);
			}
		}

		// Save schemeIdUri and value if present
		if (schemeId)
		{
			retBox->setSchemeIdUri(schemeId);
			if(schemeIdValue)
			{
				retBox->setValue(schemeIdValue);
			}
		}
		else
		{
			if(schemeIdValue)
			{
				free(schemeIdValue);
			}
		}
	}
	else
	{
		if (schemeId)
		{
			free(schemeId);
		}
		if(schemeIdValue)
		{
			free(schemeIdValue);
		}
	}
	return retBox;
}

/**
 *  @brief TrunIsoBmffBox constructor
 */
TrunIsoBmffBox::TrunIsoBmffBox(uint32_t sz, uint64_t sampleDuration,uint32_t sampleCount, uint8_t *sampleCountLoc, uint8_t* firstSampleDurationLoc, uint32_t firstSampleSize, uint32_t flags)
		: FullIsoBmffBox(sz, IsoBmffBox::TRUN, 0, 0),
		duration(sampleDuration),
		sample_count(sampleCount),
		sample_count_loc(sampleCountLoc),
		first_sample_duration_loc(firstSampleDurationLoc),
		mFirstSampleSize(firstSampleSize),
		mFlags(flags)
{
}

/**
 *  @brief TrunIsoBmffBox constructor
 */
TrunIsoBmffBox::TrunIsoBmffBox(FullIsoBmffBox &fbox, uint64_t sampleDuration,uint32_t sampleCount, uint8_t *sampleCountLoc, uint8_t* firstSampleDurationLoc, uint32_t firstSampleSize, uint32_t flags)
		: FullIsoBmffBox(fbox),
		duration(sampleDuration),
		sample_count(sampleCount),
		sample_count_loc(sampleCountLoc),
		first_sample_duration_loc(firstSampleDurationLoc),
		mFirstSampleSize(firstSampleSize),
		mFlags(flags)
{
}

/**
 *  @brief Set SampleDuration value
 */
void TrunIsoBmffBox::setFirstSampleDuration(uint64_t sampleDuration)
{
	duration = sampleDuration;
	if (nullptr != first_sample_duration_loc)
	{
		PLAYER_WRITE_U32(first_sample_duration_loc, sampleDuration);
	}
}

/**
 *  @brief Get sampleDuration value
 */
uint64_t TrunIsoBmffBox::getSampleDuration()
{
	return duration;
}

/**
 *  @brief Get SampleCount value
 */
uint32_t TrunIsoBmffBox::getSampleCount()
{
	return sample_count;
}

/**
 *  @brief Static function to construct a TrunIsoBmffBox object
 */
TrunIsoBmffBox* TrunIsoBmffBox::constructTrunBox(uint32_t sz, uint8_t *ptr)
{
	// The size and tag have been read, so the pointer has advanced after that value
	auto start = ptr;

	uint8_t version = PLAYER_READ_VERSION(ptr);
	uint32_t flags  = PLAYER_READ_FLAGS(ptr);
	uint32_t sample_count = 0;
	uint32_t sample_duration = 0;
	uint32_t sample_size = 0;
	uint64_t totalSampleDuration = 0;

	uint32_t record_fields_count = 0;

	uint32_t firstSampleSize{};

	// count the number of bits set to 1 in the second byte of the flags
	for (unsigned int i=0; i<8; i++)
	{
		if (flags & (1<<(i+8))) ++record_fields_count;
	}

	uint8_t *sampleCountLoc = ptr;
	sample_count = PLAYER_READ_U32(ptr);
	if(flags & TRUN_FLAG_DATA_OFFSET_PRESENT)
	{
		ptr += sizeof(uint32_t);   // skip data offset
	}
	if(flags & TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT)
	{
		ptr += sizeof(uint32_t);   // skip first sample flags
		// bit(4) reserved=0;
		// unsigned int(2) is_leading;
		// unsigned int(2) sample_depends_on;
		// unsigned int(2) sample_is_depended_on;
		// unsigned int(2) sample_has_redundancy;
		// bit(3) sample_padding_value;
		// bit(1) sample_is_non_sync_sample;
		// unsigned int(16) sample_degradation_priority;
	}

	// Used by truncate operation
	//uint32_t bytesPerSample = ((flags & TRUN_FLAG_SAMPLE_DURATION_PRESENT)? 4 : 0) +
	//						  ((flags & TRUN_FLAG_SAMPLE_SIZE_PRESENT)? 4 : 0) +
	//						  ((flags & TRUN_FLAG_SAMPLE_FLAGS_PRESENT)? 4 : 0) +
	//						  ((flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)? 4 : 0);
	uint8_t *firstSampleDurationLoc = nullptr;

	for (unsigned int i=0; i<sample_count; i++)
	{
		if (flags & TRUN_FLAG_SAMPLE_DURATION_PRESENT)
		{
			if (i==0)
			{
				firstSampleDurationLoc = ptr;
			}
			sample_duration = PLAYER_READ_U32(ptr);
			totalSampleDuration += sample_duration;
		}
		if (flags & TRUN_FLAG_SAMPLE_SIZE_PRESENT)
		{
			sample_size = PLAYER_READ_U32(ptr);
			// Will be unhelpful for truncating if this is not present
			if (i==0)
			{
				firstSampleSize = sample_size;
			}
		}
		if (flags & TRUN_FLAG_SAMPLE_FLAGS_PRESENT)
		{
			ptr += sizeof(uint32_t);   // skip sample flags
		}
		if (flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)
		{
			ptr += sizeof(uint32_t);   // skip sample composition time offset
		}
	}
	FullIsoBmffBox fbox(sz, IsoBmffBox::TRUN, version, flags);
	fbox.setBase(start);
	return new TrunIsoBmffBox(fbox, totalSampleDuration, sample_count, sampleCountLoc, firstSampleDurationLoc, firstSampleSize, flags);
}

/**
 * @fun TrunBox::truncate
 * @brief Reduce the number of samples to 1 and insert a skip box if there is enough space
*/
void TrunIsoBmffBox::truncate(void)
{
	if (sample_count > 1)
	{
		uint32_t bytesPerSample = ((flags & TRUN_FLAG_SAMPLE_DURATION_PRESENT)? 4 : 0) +
								((flags & TRUN_FLAG_SAMPLE_SIZE_PRESENT)? 4 : 0) +
								((flags & TRUN_FLAG_SAMPLE_FLAGS_PRESENT)? 4 : 0) +
								((flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)? 4 : 0);
		uint32_t tableSize{sample_count * bytesPerSample};
		// Calculate new trun size and write
		auto oldTrunSize = getSize();
		auto newTrunSize = getSize() - tableSize + bytesPerSample;
		sample_count = 1;
		PLAYER_WRITE_U32(sample_count_loc, sample_count);

		// If there is room to insert a skip box
		if ((oldTrunSize - newTrunSize) >= PLAYER_SIZEOF_SIZE_AND_TAG)
		{
			setSize(newTrunSize);
			SkipIsoBmffBox skip{tableSize - bytesPerSample, getBase() + getSize()};
		}
		else
		{
			MW_LOG_INFO("No room for a skip box");
		}
	}
	// Else no need to truncate
}

/**
 * @fn getFirstSampleSize
 *
 * @return The size of the first sample
 */
uint32_t TrunIsoBmffBox::getFirstSampleSize()
{
	return mFirstSampleSize;
}

/**
 * @fn getFirstSampleSize
 *
 * @return true if SAMPLE_DURATION_PRESENT is enabled, false otherwise
 */
bool TrunIsoBmffBox::sampleDurationPresent()
{
	return ((flags & TRUN_FLAG_SAMPLE_DURATION_PRESENT) != 0);
}

/**
 *  @brief TfhdIsoBmffBox constructor
 */
TfhdIsoBmffBox::TfhdIsoBmffBox(uint32_t sz, uint64_t default_duration, uint8_t* default_duration_location, uint32_t default_sample_size, uint32_t flags)
	: FullIsoBmffBox(sz, IsoBmffBox::TFHD, 0, 0),
	mDefaultSampleDuration(default_duration),
	default_sample_duration_location(default_duration_location),
	mDefaultSampleSize(default_sample_size),
	mFlags(flags)
{

}

/**
 *  @brief TfhdIsoBmffBox constructor
 */
TfhdIsoBmffBox::TfhdIsoBmffBox(FullIsoBmffBox &fbox, uint64_t default_duration, uint8_t* default_duration_location, uint32_t default_sample_size, uint32_t flags)
	: FullIsoBmffBox(fbox),
	mDefaultSampleDuration(default_duration),
	default_sample_duration_location(default_duration_location),
	mDefaultSampleSize(default_sample_size),
	mFlags(flags)
{

}

bool TfhdIsoBmffBox::defaultSampleDurationPresent(void)
{
	return (mFlags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT) != 0;
}

uint64_t TfhdIsoBmffBox::getDefaultSampleDuration()
{
	return mDefaultSampleDuration;
}

void TfhdIsoBmffBox::setDefaultSampleDuration(uint64_t default_duration)
{
	mDefaultSampleDuration = default_duration;
	if (nullptr != default_sample_duration_location)
	{
		PLAYER_WRITE_U32(default_sample_duration_location, default_duration);
	}
}

uint32_t TfhdIsoBmffBox::getDefaultSampleSize(void)
{
    return mDefaultSampleSize;
}

/**
 *  @brief Static function to construct a TfdtIsoBmffBox object
 */
TfhdIsoBmffBox* TfhdIsoBmffBox::constructTfhdBox(uint32_t sz, uint8_t *ptr)
{
	auto start = ptr;
	uint8_t version = PLAYER_READ_VERSION(ptr); // 8
	uint32_t flags  = PLAYER_READ_FLAGS(ptr); //24

	uint32_t DefaultSampleDuration{0};
	uint32_t DefaultSampleSize{0};
	uint8_t* DefaultSampleDuration_loc{nullptr};

	ptr += sizeof(uint32_t);       // skip track id

	if (flags & TFHD_FLAG_BASE_DATA_OFFSET_PRESENT)
	{
		ptr += sizeof(uint64_t);   // skip base data offset
	}

	if (flags & TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT)
	{
		ptr += sizeof(uint32_t);   // skip sample description index
	}

	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT)
	{
		DefaultSampleDuration_loc = ptr;
		DefaultSampleDuration = PLAYER_READ_U32(ptr);
	}

	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT)
	{
		DefaultSampleSize = PLAYER_READ_U32(ptr);
	}

	FullIsoBmffBox fbox(sz, IsoBmffBox::TFHD, version, flags);
	fbox.setBase(start);

	return new TfhdIsoBmffBox(fbox, DefaultSampleDuration, DefaultSampleDuration_loc, DefaultSampleSize, flags);
}

/**
 *  @brief PrftIsoBmffBox constructor
 */
PrftIsoBmffBox::PrftIsoBmffBox(uint32_t sz, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime) : FullIsoBmffBox(sz, IsoBmffBox::PRFT, 0, 0), track_id(trackId), ntp_ts(ntpTs), media_time(mediaTime)
{

}

/**
 *  @brief PrftIsoBmffBox constructor
 */
PrftIsoBmffBox::PrftIsoBmffBox(FullIsoBmffBox &fbox, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime) : FullIsoBmffBox(fbox), track_id(trackId), ntp_ts(ntpTs), media_time(mediaTime)
{

}

/**
 *  @brief Set Track Id value
 */
void PrftIsoBmffBox::setTrackId(uint32_t trackId)
{
	track_id = trackId;
}

/**
 *  @brief Get Track id value
 */
uint32_t PrftIsoBmffBox::getTrackId()
{
	return track_id;
}

/**
 *  @brief Set NTP Ts value
 */
void PrftIsoBmffBox::setNtpTs(uint64_t ntpTs)
{
	ntp_ts = ntpTs;
}

/**
 *  @brief Get ntp Timestamp value
 */
uint64_t PrftIsoBmffBox::getNtpTs()
{
	return ntp_ts;
}

/**
 * @brief Set Sample Duration value
 */
void PrftIsoBmffBox::setMediaTime(uint64_t mediaTime)
{
	media_time = mediaTime;
}

/**
 *  @brief Get SampleDuration value
 */
uint64_t PrftIsoBmffBox::getMediaTime()
{
	return media_time;
}

/**
 *  @brief Static function to construct a PrftIsoBmffBox object
 */
PrftIsoBmffBox* PrftIsoBmffBox::constructPrftBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = PLAYER_READ_VERSION(ptr); //8
	uint32_t flags  = PLAYER_READ_FLAGS(ptr); //24

	uint32_t track_id = 0; // reference track ID
	track_id = PLAYER_READ_U32(ptr);
	uint64_t ntp_ts = 0; // NTP time stamp
	ntp_ts = READ_64(ptr);
	uint64_t pts = 0; //media time
	pts = READ_64(ptr);

	FullIsoBmffBox fbox(sz, IsoBmffBox::PRFT, version, flags);

	return new PrftIsoBmffBox(fbox, track_id, ntp_ts, pts);
}

/**
 * @fn constructTrakBox
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box
 * @param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
 * @return newly constructed trak object
 */
TrakIsoBmffBox* TrakIsoBmffBox::constructTrakBox(uint32_t sz, uint8_t *ptr, int newTrackId)
{
	TrakIsoBmffBox *cbox = new TrakIsoBmffBox(sz);
	uint32_t curOffset = sizeof(uint32_t) + sizeof(uint32_t); //Sizes of size & type fields
	while (curOffset < sz)
	{
		IsoBmffBox *box = IsoBmffBox::constructBox(ptr, sz-curOffset);
		box->setOffset(curOffset);

		if (IS_TYPE(box->getType(),TKHD))
		{
			uint8_t *tkhd_start = ptr;
			ptr += curOffset;
			uint8_t version = PLAYER_READ_VERSION(ptr);
			uint32_t skip = 3; // size of flags
			if (1 == version)
			{
				skip += sizeof(uint64_t) * 2; //CreationTime + ModificationTime
			}
			else
			{
				skip += sizeof(uint32_t) * 2; //CreationTime + ModificationTime
			}
			ptr += skip;
			if(-1 != newTrackId)
			{
				PLAYER_WRITE_U32(ptr, newTrackId);
			}
			cbox->track_id = PLAYER_READ_U32(ptr);
			ptr = tkhd_start;
		}
		cbox->addChildren(box);
		curOffset += box->getSize();
		ptr += box->getSize();
	}
	return cbox;
}

/**
 *  @brief SidxIsoBmffBox constructor
 */
SidxIsoBmffBox::SidxIsoBmffBox(FullIsoBmffBox &fbox, uint32_t currTimeScale, uint64_t sidxDuration) : FullIsoBmffBox(fbox), timeScale(currTimeScale), duration(sidxDuration)
{

}

/**
 *  @brief SidxIsoBmffBox constructor
 */
SidxIsoBmffBox::SidxIsoBmffBox(uint32_t sz, uint32_t tScale, uint64_t sidxDuration) : FullIsoBmffBox(sz, IsoBmffBox::SIDX, 0, 0), timeScale(tScale), duration(sidxDuration)
{

}

/**
 *  @brief Get TimeScale value
 */
uint32_t SidxIsoBmffBox::getTimeScale()
{
	return timeScale;
}

/**
 *  @brief Set TimeScale value
 */
void SidxIsoBmffBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}

/**
 *  @brief Get sampleDuration value
 */
uint64_t SidxIsoBmffBox::getSampleDuration()
{
    return duration;
}

/**
 *  @brief Static function to construct a SidxBox object
 */
SidxIsoBmffBox* SidxIsoBmffBox::constructSidxBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = PLAYER_READ_VERSION(ptr); //8
	uint32_t flags  = PLAYER_READ_FLAGS(ptr); //24
	uint32_t reference_ID = PLAYER_READ_U32(ptr); (void) reference_ID; // 32
	uint32_t currTimeScale = PLAYER_READ_U32(ptr); //32
	uint32_t duration = 0x00;
	if ( version == 0)
	{
		PLAYER_READ_U32(ptr); // earliest_presentation_time;
		PLAYER_READ_U32(ptr); // first_offset
	}
	else
	{
		READ_64(ptr); // earliest_presentation_time;
		READ_64(ptr); // first_offset;
	}
	PLAYER_READ_U16(ptr);  //unused
	uint16_t refCount = PLAYER_READ_U16(ptr);
	for(uint16_t i = 0; i < refCount; i++)
	{
		PLAYER_READ_U32(ptr); // refType, size
		duration += PLAYER_READ_U32(ptr);
		PLAYER_READ_U32(ptr); // startsWithSAP, sapType, sapDeltaTime
	}
	FullIsoBmffBox fbox(sz, IsoBmffBox::SIDX, version, flags);
	return new SidxIsoBmffBox(fbox, currTimeScale,duration);
}


/**
 * @fn constructMdatIsoBmffBox
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box

 * @return newly constructed mdat object
 */
MdatIsoBmffBox* MdatIsoBmffBox::constructMdatBox(uint32_t sz, uint8_t *ptr)
{
	MdatIsoBmffBox *cbox = new MdatIsoBmffBox(sz, ptr);
	// The size and tag have been read, so the pointer has advanced after that value
	cbox->setBase(ptr);

	return cbox;
}

/**
 * @fn MdatBox::truncate
 * @param[in] newSize - new mdat box size
 * @return None
*/

void MdatIsoBmffBox::truncate(uint32_t newSize)
{
	setSize(newSize);
}

/**
 * @fn SencIsoBmffBox
 *
 * @param[in] fbox - box object
 * @param[in] sampleCountLoc - sample count location
 * @param[in] numSamples - number of samples
 */
SencIsoBmffBox::SencIsoBmffBox(FullIsoBmffBox &fbox, uint8_t *sampleCountLoc, uint32_t numSamples): sampleCountLoc(sampleCountLoc), numSamples(numSamples), FullIsoBmffBox(fbox)
{
}

/**
 * @fn constructSencIsoBmffBox
 *
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box
 * @return newly constructed SencBox object
 */
SencIsoBmffBox* SencIsoBmffBox::constructSencBox(uint32_t sz, uint8_t *ptr)
{
	auto start = ptr;

	uint8_t version = PLAYER_READ_VERSION(ptr); // 8 bits
	uint32_t flags  = PLAYER_READ_FLAGS(ptr); // 24 bits

	auto sample_count_loc{ptr};
	uint32_t sample_count = PLAYER_READ_U32(ptr);

	FullIsoBmffBox fbox(sz, IsoBmffBox::SENC, version, flags);
	fbox.setBase(start);

	return new SencIsoBmffBox(fbox, sample_count_loc, sample_count);
}

/**
 * @fn sencIsoBmffBox::truncate
 * @brief Reduce the number of samples to 1, write a new skip box over the remainder of the table
 *
 * @param[in] firstSampleSize - Size of the first sample
*/
void SencIsoBmffBox::truncate(uint32_t firstSampleSize)
{
	if (numSamples > 1)
	{
		if (firstSampleSize)
		{
			auto newEnd{sampleCountLoc + sizeof(uint32_t) + firstSampleSize};
			auto oldSize{getSize()};
			auto newSize{static_cast<uint32_t>(newEnd - getBase())};

			if ((oldSize - newSize) >= PLAYER_SIZEOF_SIZE_AND_TAG)
			{
				PLAYER_WRITE_U32(getBase(), newSize);
				SkipIsoBmffBox skip{oldSize - newSize, newEnd};
			}
			else
			{
				MW_LOG_INFO("No room for a skip box");
			}
		}

		numSamples = 1;
		PLAYER_WRITE_U32(sampleCountLoc, numSamples);
	}
}

/**
 * @fn SaizIsoBmffBox
 *
 * @param[in] fbox - box object
 * @param[in] sampleCountLoc - location of the sample count
 * @param[in] numSamples - number of samples
 * @param[in] firstSampleInfoSize - Size of the first sample
 */
SaizIsoBmffBox::SaizIsoBmffBox(FullIsoBmffBox &fbox, uint8_t *sampleCountLoc, uint32_t numSamples, uint32_t firstSampleInfoSize)
		: sampleCountLoc(sampleCountLoc),
		numSamples(numSamples),
		firstSampleInfoSize(firstSampleInfoSize),
		FullIsoBmffBox(fbox)
{
}

/**
 * @fn constructSaizIsoBmffBox
 *
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box
 * @return newly constructed SaizBox object
 */
SaizIsoBmffBox* SaizIsoBmffBox::constructSaizBox(uint32_t sz, uint8_t *ptr)
{
	auto start = ptr;
	uint8_t version = PLAYER_READ_VERSION(ptr); // 8
	uint32_t flags = PLAYER_READ_FLAGS(ptr); // 24

	if (flags & SAIZ_FLAG_AUX_INFO_TYPE_PRESENT)
	{
		ptr += sizeof(uint32_t);   // skip aux_info_type
		ptr += sizeof(uint32_t);   // skip aux_info_type_parameter
	}

	uint8_t default_sample_info_size{*ptr++};

	uint8_t *sample_count_loc{ptr};
	uint32_t sample_count = PLAYER_READ_U32(ptr);

	uint8_t sample_info_size = (0 == default_sample_info_size) ? *ptr : default_sample_info_size;

	FullIsoBmffBox fbox(sz, IsoBmffBox::SAIZ, version, flags);
	fbox.setBase(start);

	return new SaizIsoBmffBox(fbox, sample_count_loc, sample_count, sample_info_size);
}

/**
 * @fn SaizIsoBmffBox::getFirstSampleInfoSize
*/
uint32_t SaizIsoBmffBox::getFirstSampleInfoSize(void)
{
	return firstSampleInfoSize;
}


/**
 * @fn SaizIsoBmffBox::truncate
*/
void SaizIsoBmffBox::truncate(void)
{
	auto oldSize{getSize()};
	auto newSize{oldSize - (numSamples - 1)};

	// Need min 8 bytes to insert a skip box
	if ((oldSize - newSize) >= PLAYER_SIZEOF_SIZE_AND_TAG)
	{
		PLAYER_WRITE_U32(getBase(), newSize);
		SkipIsoBmffBox skip{oldSize - newSize, getBase() + newSize};
	}
	else
	{
		MW_LOG_INFO("No room for a skip box");
		// Not truncating the table, just setting the sample count to 1
	}

	numSamples = 1;
	PLAYER_WRITE_U32(sampleCountLoc, 1);
}
}
