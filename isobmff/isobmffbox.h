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
* @file isobmffbox.h
* @brief Header file for ISO Base Media File Format Boxes
*/

#ifndef __ISOBMFFBOX_H__
#define __ISOBMFFBOX_H__

#include <cstdint>
#include <vector>
#include <string.h>
#include <string>
#include <cstring>
#include "AampLogManager.h"

// Size of the size and tag fields in IsoBmff
#define SIZEOF_SIZE_AND_TAG    (8)

#define READ_U16(buf) \
	(buf[0] << 8) | buf[1]; buf+=2;

#define READ_U32(buf) \
	((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3]; buf+=4;

#define WRITE_U64(buf, val) \
	buf[0]= val>>56; buf[1]= val>>48; buf[2]= val>>40; buf[3]= val>>32; buf[4]= val>>24; buf[5]= val>>16; buf[6]= val>>8; buf[7]= val;

#define WRITE_U32(buf, val) \
	buf[0]= val>>24; buf[1]= val>>16; buf[2]= val>>8; buf[3]= val;

#define READ_U8(dst, src, sz) \
	memcpy(dst, src, sz); src+=sz;

#define READ_VERSION(buf) \
		buf[0]; buf++;

#define READ_FLAGS(buf) \
		(buf[0] << 16) | (buf[1] << 8) | buf[2]; buf+=3;

/**
 * @fn ReadUint64
 *
 * @param[in] buf - buffer pointer
 * @return bytes read from buffer
 */
uint64_t ReadUint64(uint8_t *buf);

/**
 * @fn WriteUint64
 *
 * @param[in] dst - buffer pointer
 * @param[in] val - value to write
 * @return void
 */
void WriteUint64(uint8_t *dst, uint64_t val);

/**
 * @fn ReadCStringLen
 * @param[in] buffer Buffer to read
 * @param[in] bufferLen String length
 */
int ReadCStringLen(const uint8_t* buffer, uint32_t bufferLen);

#define READ_U64(buf) \
		ReadUint64(buf); buf+=8;

#define IS_TYPE(value, type) \
		(value[0]==type[0] && value[1]==type[1] && value[2]==type[2] && value[3]==type[3])


/**
 * @class Box
 * @brief Base Class for ISO BMFF Box
 */
class Box
{
private:
	uint8_t *base;		/**< Ptr to start of box */
	uint32_t offset;	/**< Offset from the beginning of the segment */
	uint32_t size;		/**< Box Size */
	char type[5]; 		/**< Box Type Including \0 */

/*TODO: Handle special cases separately */
public:
	static constexpr const char *FTYP = "ftyp";
	static constexpr const char *MOOV = "moov";
	static constexpr const char *MVHD = "mvhd";
	static constexpr const char *TRAK = "trak";
	static constexpr const char *MDIA = "mdia";
	static constexpr const char *MDHD = "mdhd";
	static constexpr const char *EMSG = "emsg";

	static constexpr const char *MOOF = "moof";
	static constexpr const char *TFHD = "tfhd";
	static constexpr const char *TRAF = "traf";
	static constexpr const char *TFDT = "tfdt";
	static constexpr const char *TRUN = "trun";
	static constexpr const char *MDAT = "mdat";
	static constexpr const char *TKHD = "tkhd";

	static constexpr const char *STYP = "styp";
	static constexpr const char *SIDX = "sidx";
	static constexpr const char *PRFT = "prft";
	static constexpr const char *SKIP = "skip";
	static constexpr const char *SENC = "senc";
	static constexpr const char *SAIZ = "saiz";

	/**
	 * @fn Box
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 */
	Box(uint32_t sz, const char btype[4]);

	/**
	 * @brief Box destructor
	 */
	virtual ~Box()
	{

	}

	/**
	 * @fn setOffset
	 *
	 * @param[in] os - offset
	 * @return void
	 */
	void setOffset(uint32_t os);

	/**
	 * @fn getOffset
	 *
	 * @return offset of box
	 */
	uint32_t getOffset() const;

	/**
	 * @fn hasChildren
	 * @return true if this box has other boxes as children
	 */
	virtual bool hasChildren() const;

	/**
	 * @fn getChildren
	 *
	 * @return array of child boxes
	 */
	virtual const std::vector<Box*> *getChildren() const;

	/**
	 * @fn truncate
	 */
	virtual void truncate(void) {}
	virtual void truncate(uint32_t param) {}

	/**
	 * @fn getSize
	 *
	 * @return box size
	 */
	uint32_t getSize() const;

	void setSize(uint32_t newSize) { WRITE_U32(base, newSize);
		size = newSize;
	 }

	/**
	 * @fn getType
	 *
	 * @return box type
	 */
	const char *getType() const;

	/**
	 * @fn constructBox
	 *
	 * @param[in] hdr - pointer to box
	 * @param[in] maxSz - box size
	 * @param[in] correctBoxSize - flag to check if box size needs to be corrected
	 * @param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
	 * @return newly constructed Box object
	 */
	static Box* constructBox(uint8_t *hdr, uint32_t maxSz, bool correctBoxSize = false, int newTrackId = -1);

	uint8_t *getBase(void) const { return base; }

	/**
	 * @fn setBase
	 * @brief Store a pointer to the base of the box
	 *
	 * @param[in] start - pointer to the box after reading the size and box type
	 */
	void setBase(uint8_t *start) { base = start  - SIZEOF_SIZE_AND_TAG; }

	/**
	 * @fn rewriteAsSkipBox
	 * @brief Rewrite the box buffer data as a skip box
	 * NB This will not affect the object, it will only change the buffer data
	 * Once this has been called on a box, the box object should not be used
	 */
	void rewriteAsSkipBox(void);

};

/**
 * @class GenericContainerBox
 * @brief Class for ISO BMFF Box container
 * Eg: MOOV, MOOF, TRAK, MDIA
 */
class GenericContainerBox : public Box
{
private:
	std::vector<Box*> children;	// array of child boxes

public:
	/**
	 * @fn GenericContainerBox
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 */
	GenericContainerBox(uint32_t sz, const char btype[4]);

	/**
	 * @fn ~GenericContainerBox
	 */
	virtual ~GenericContainerBox();

	/**
	 * @fn addChildren
	 *
	 * @param[in] box - child box object
	 * @return void
	 */
	void addChildren(Box *box);

	/**
	 * @fn hasChildren
	 *
	 * @return true if this box has other boxes as children
	 */
	bool hasChildren() const override;

	/**
	 * @fn getChildren
	 *
	 * @return array of child boxes
	 */
	const std::vector<Box*> *getChildren() const override;

	/**
	 * @fn constructContainer
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 * @param[in] ptr - pointer to box
	 * @param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
	 * @return newly constructed GenericContainerBox object
	 */
	static GenericContainerBox* constructContainer(uint32_t sz, const char btype[4], uint8_t *ptr, int newTrackId = -1);
};

/**
 * @class TrakBox
 * @brief Class for ISO BMFF TRAK container
 */
class TrakBox : public GenericContainerBox
{
private:
	uint32_t track_id;
public:
	/**
	 * @brief Trak constructor
	 *
	 * @param[in] sz - box size
	 */
	TrakBox(uint32_t sz) : GenericContainerBox(sz, TRAK), track_id(0)
	{
	}
	/**
	 * @fn constructTrakBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @param[in] newTrackId - new track id to overwrite the existing track id, when value is -1, it will not override
	 * @return newly constructed trak object
	 */
	static TrakBox* constructTrakBox(uint32_t sz, uint8_t *ptr, int newTrackId = -1);

	/**
	 * @brief track_id getter
	 *
	 * @return trak_id
	 */
	uint32_t getTrack_Id() { return track_id; }
};
/**
 * @class FullBox
 * @brief Class for single ISO BMFF Box
 * Eg: FTYP, MDHD, MVHD, TFDT
 */
class FullBox : public Box
{
protected:
	uint8_t version;
	uint32_t flags;

public:
	/**
	 * @fn FullBox
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 * @param[in] ver - version value
	 * @param[in] f - flag value
	 */
	FullBox(uint32_t sz, const char btype[4], uint8_t ver, uint32_t f);
};

/**
 * @class MdatBox
 * @brief create an mdat box
*/
class MdatBox: public Box
{
public:

	/**
	 * @fn MdatBox
	 * @brief construct an mdat box header with the specified size at the specified location
	 * @param[in] sz - box size
	 * @param[in] locn - location
	*/
	MdatBox(uint32_t sz, uint8_t *locn) : Box(sz, Box::MDAT){}

	void truncate(uint32_t newSize) override;

	static MdatBox* constructMdatBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @class skipBox
 * @brief Create a skip box
*/
class SkipBox: public Box
{
public:
	/**
	 * @fn skipBox
	 * @brief construct a skip box header with the specified size at the specified location
	 * @param[in] sz - box size
	 * @param[in] locn - location
	*/
	SkipBox(uint32_t sz, uint8_t *locn) : Box(sz, Box::SKIP)
	{
		WRITE_U32(locn, sz);
		memcpy(locn + sizeof(uint32_t), Box::SKIP, std::strlen(Box::SKIP));
	}
};

/**
 * @class MvhdBox
 * @brief Class for ISO BMFF MVHD Box
 */
class MvhdBox : public FullBox
{
private:
	uint32_t timeScale;
	uint8_t* const timeScale_loc;

public:
	/**
	 * @fn MvhdBox
	 *
	 * @param[in] sz - box size
	 * @param[in] tScale - TimeScale value
	 * @param[in] tScale_loc - pointer with the location of the TimeScale in the buffer
	 */
	MvhdBox(uint32_t sz, uint32_t tScale, uint8_t* tScale_loc);

	/**
	 * @fn MvhdBox
	 * @param[in] fbox - box object
	 * @param[in] tScale - TimeScale value
	 * @param[in] tScale_loc - pointer with the location of the TimeScale in the buffer
	 */
	MvhdBox(FullBox &fbox, uint32_t tScale, uint8_t* tScale_loc);

	/**
	 * @fn setTimeScale
	 *
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setTimeScale(uint32_t tScale);

	/**
	 * @fn getTimeScale
	 *
	 * @return TimeScale value
	 */
	uint32_t getTimeScale();

	/**
	 * @fn constructMvhdBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed MvhdBox object
	 */
	static MvhdBox* constructMvhdBox(uint32_t sz, uint8_t *ptr);
};


/**
 * @class MdhdBox
 * @brief Class for ISO BMFF MDHD Box
 */
class MdhdBox : public FullBox
{
private:
	uint32_t timeScale;
	uint8_t* const timeScale_loc;
	uint64_t duration;
	uint8_t* const duration_loc;

public:
	/**
	 * @fn MdhdBox
	 *
	 * @param[in] sz - box size
	 * @param[in] tScale - TimeScale value
	 * @param[in] tScale_loc - pointer with the location of the TimeScale in the buffer
	 * @param[in] dur - duration value
	 * @param[in] dur_loc - pointer with the location of the Duration in the buffer
	 */
	MdhdBox(uint32_t sz, uint32_t tScale, uint8_t* tScale_loc, uint64_t dur, uint8_t* dur_loc);

	/**
	 * @fn MdhdBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] tScale - TimeScale value
	 * @param[in] tScale_loc - pointer with the location of the TimeScale in the buffer
	 * @param[in] dur - duration value
	 * @param[in] dur_loc - pointer with the location of the Duration in the buffer
	 */
	MdhdBox(FullBox &fbox, uint32_t tScale, uint8_t* tScale_loc, uint64_t dur, uint8_t* dur_loc);

	/**
	 * @fn setTimeScale
	 *
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setTimeScale(uint32_t tScale);

	/**
	 * @fn getTimeScale
	 *
	 * @return TimeScale value
	 */
	uint32_t getTimeScale();

	/**
	 * @fn setDuration
	 *
	 * @param[in] dur - duration value
	 * @return void
	 */
	void setDuration(uint64_t dur);

	/**
	 * @fn getDuration
	 *
	 * @return duration value
	 */
	uint64_t getDuration();

	/**
	 * @fn constructMdhdBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed MdhdBox object
	 */
	static MdhdBox* constructMdhdBox(uint32_t sz, uint8_t *ptr);
};


/**
 * @class TfdtBox
 * @brief Class for ISO BMFF TFDT Box
 */
class TfdtBox : public FullBox
{
private:
	uint64_t baseMDT;			// BaseMediaDecodeTime value
	uint8_t* const baseMDT_loc; // BaseMediaDecodeTime location

public:
	/**
	 * @fn TfdtBox
	 *
	 * @param[in] sz - box size
	 * @param[in] mdt - BaseMediaDecodeTime value
	 * @param[in] mdt_loc - BaseMediaDecodeTime location
	 */
	TfdtBox(uint32_t sz, uint64_t mdt, uint8_t* mdt_loc);

	/**
	 * @fn TfdtBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] mdt - BaseMediaDecodeTime value
	 * @param[in] mdt_loc - BaseMediaDecodeTime location
	 */
	TfdtBox(FullBox &fbox, uint64_t mdt, uint8_t* mdt_loc);

	/**
	 * @fn setBaseMDT
	 *
	 * @param[in] mdt - BaseMediaDecodeTime value
	 * @return void
	 */
	void setBaseMDT(uint64_t mdt);

	/**
	 * @fn getBaseMDT
	 *
	 * @return BaseMediaDecodeTime value
	 */
	uint64_t getBaseMDT();

	/**
	 * @fn constructTfdtBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed TfdtBox object
	 */
	static TfdtBox* constructTfdtBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @class EmsgBox
 * @brief Class for ISO BMFF EMSG Box
 */
class EmsgBox : public FullBox
{
private:
	uint32_t timeScale;
	uint32_t eventDuration;
	uint32_t id;
	uint32_t presentationTimeDelta; // This is added in emsg box v1
	uint64_t presentationTime;	// This is included in emsg box v0
	char* schemeIdUri;
	uint8_t* value;
	// Message data
	uint8_t* messageData;
	uint32_t messageLen;

public:
	/**
	 * @fn EmsgBox
	 *
	 * @param[in] sz - box size
	 * @param[in] tScale - TimeScale value
	 * @param[in] evtDur - eventDuration value
	 * @param[in] _id - id value
	 */
	EmsgBox(uint32_t sz, uint32_t tScale, uint32_t evtDur, uint32_t _id);

	/**
	 * @fn EmsgBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] tScale - TimeScale value
	 * @param[in] evtDur - eventDuration value
	 * @param[in] _id - id value
	 * @param[in] presTime - presentationTime value
	 * @param[in] presTimeDelta - presentationTimeDelta value
	 */
	EmsgBox(FullBox &fbox, uint32_t tScale, uint32_t evtDur, uint32_t _id, uint64_t presTime, uint32_t presTimeDelta);

	/**
	 * @fn ~EmsgBox
	 */
	~EmsgBox();

	/**
	 * @brief EmsgBox copy constructor
	 */
	EmsgBox(const EmsgBox&) = delete;

	/**
	 * @brief EmsgBox =operator overloading
	 */
	EmsgBox& operator=(const EmsgBox&) = delete;

	/**
	 * @fn setTimeScale
	 *
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setTimeScale(uint32_t tScale);

	/**
	 * @fn getTimeScale
	 *
	 * @return TimeScale value
	 */
	uint32_t getTimeScale();

	/**
	 * @fn setEventDuration
	 *
	 * @param[in] evtDur - eventDuration value
	 * @return void
	 */
	void setEventDuration(uint32_t evtDur);

	/**
	 * @fn getEventDuration
	 *
	 * @return eventDuration value
	 */
	uint32_t getEventDuration();

	/**
	 * @fn setId
	 *
	 * @param[in] _id - id
	 * @return void
	 */
	void setId(uint32_t _id);

	/**
	 * @fn getId
	 *
	 * @return id value
	 */
	uint32_t getId();

	/**
	 * @fn setPresentationTimeDelta
	 *
	 * @param[in] presTimeDelta - presTimeDelta
	 * @return void
	 */
	void setPresentationTimeDelta(uint32_t presTimeDelta);

	/**
	 * @fn getPresentationTimeDelta
	 *
	 * @return presentationTimeDelta value
	 */
	uint32_t getPresentationTimeDelta();

	/**
	 * @fn setPresentationTime
	 *
	 * @param[in] presTime - presTime
	 * @return void
	 */
	void setPresentationTime(uint64_t presTime);

	/**
	 * @fn getPresentationTime
	 *
	 * @return presentationTime value
	 */
	uint64_t getPresentationTime();

	/**
	 * @fn setSchemeIdUri
	 *
	 * @param[in] schemeIdUri - schemeIdUri pointer
	 * @return void
	 */
	void setSchemeIdUri(char* schemeIdURI);

	/**
	 * @fn getSchemeIdUri
	 *
	 * @return schemeIdUri value
	 */
	char* getSchemeIdUri() const;

	/**
	 * @fn setValue
	 *
	 * @param[in] value - value pointer
	 * @return void
	 */
	void setValue(uint8_t* schemeIdValue);

	/**
	 * @fn getValue
	 *
	 * @return schemeIdUri value
	 */
	uint8_t* getValue();

	/**
	 * @fn setMessage
	 *
	 * @param[in] message - Message pointer
	 * @param[in] len - Message length
	 * @return void
	 */
	void setMessage(uint8_t* message, uint32_t len);

	/**
	 * @fn getMessage
	 *
	 * @return messageData
	 */
	uint8_t* getMessage();

	/**
	 * @fn getMessageLen
	 *
	 * @return messageLen
	 */
	uint32_t getMessageLen();

	/**
	 * @fn constructEmsgBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed EmsgBox object
	 */
	static EmsgBox* constructEmsgBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @class TrunBox
 * @brief Class for ISO BMFF TRUN Box
 */
class TrunBox : public FullBox
{
private:
	uint64_t duration;    //Sample Duration value
	uint32_t sample_count;
	uint8_t *sample_count_loc;
	uint8_t *first_sample_duration_loc;
	uint32_t mFirstSampleSize;
	uint32_t mFlags;

public:
	struct Entry {
	Entry() : sample_duration(0), sample_size(0), sample_flags(0), sample_composition_time_offset(0) {}
	uint32_t sample_duration;
	uint32_t sample_size;
	uint32_t sample_flags;
	uint32_t sample_composition_time_offset;
	};

	/**
 	 * @fn TrunBox
 	 *
	 * @param[in] sz - box size
	 * @param[in] sampleDuration - sample duration value
	 * @param[in] sampleCount - sample count value
	 * @param[in] sampleCountLoc - sample count location
	 * @param[in] sampleDurationLoc - sample duration location
	 * @param[in] firstSampleSize - size of the first sample
	 * @param[in] flags - flags set on this box
	 */
	TrunBox(uint32_t sz, uint64_t sampleDuration, uint32_t sampleCount, uint8_t *sampleCountLoc, uint8_t *sampleDurationLoc, uint32_t firstSampleSize, uint32_t flags);

	/**
	 * @fn TrunBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] sampleDuration - sample duration value
	 * @param[in] sampleCount - sample count value
	 * @param[in] sampleCountLoc - sample count location
	 * @param[in] sampleDurationLoc - sample duration location
	 * @param[in] firstSampleSize - size of the first sample
	 * @param[in] flags - flags set on this box
	 */
	TrunBox(FullBox &fbox, uint64_t sampleDuration, uint32_t sampleCount, uint8_t *sampleCountLoc, uint8_t *sampleDurationLoc, uint32_t firstSampleSize, uint32_t flags);

	/**
	 * @fn setFirstSampleDuration
	 *
	 * @brief The sample duration of the first sample is updated with the value provided.
	 *        The sample duration of the remaining samples is not modified.
	 *
	 * @param[in] sampleDuration - Sample Duration value
	 * @return void
	 */
	void setFirstSampleDuration(uint64_t sampleDuration);

	/**
	 * @fn getSampleDuration
	 *
	 * @return sampleDuration value
	 */
	uint64_t getSampleDuration();

	/**
	 * @fn getSampleCount
	 *
	 * @return SampleCount value
	 */
	uint32_t getSampleCount();

	/**
	 * @fn constructTrunBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed TrunBox object
	 */
	static TrunBox* constructTrunBox(uint32_t sz, uint8_t *ptr);

	/**
	 * @fn truncate
	 */
	void truncate(void) override;

	/**
	 * @fn getFirstSampleSize
	 *
	 * @return The size of the first sample
	 */
	uint32_t getFirstSampleSize(void);

	/**
	 * @fn sampleDurationPresent
	 *
	 * @return true if SAMPLE_DURATION_PRESENT is enabled, false otherwise
	 */
	bool sampleDurationPresent(void);
};

/**
 * @class TfhdBox
 * @brief Class for ISO BMFF TFHD Box
 */
class TfhdBox : public FullBox
{
private:
	uint64_t mDefaultSampleDuration;
	uint8_t *default_sample_duration_location;
	uint32_t mDefaultSampleSize;
	uint32_t mFlags;

public:
	/**
	 * @fn TfhdBox
	 *
	 * @param[in] sz - box size
	 * @param[in] sample_duration - Sample Duration value
	 * @param[in] first_sample_duration_loc - Default sample duration location
	 * @param[in] default_sample_size - Default sample size
	 * @param[in] flags - flags set on this box
	 */
	TfhdBox(uint32_t sz, uint64_t sample_duration, uint8_t* first_sample_duration_loc, uint32_t default_sample_size, uint32_t flags);

	/**
	 * @fn TfhdBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] sample_duration - Sample Duration value
	 * @param[in] first_sample_duration_loc - Default sample duration location
	 * @param[in] default_sample_size - Default sample size
	 * @param[in] flags - flags set on this box
	 */
	TfhdBox(FullBox &fbox, uint64_t default_duration, uint8_t * default_duration_location, uint32_t default_sample_size, uint32_t flags);

	/**
	 * @fn defaultSampleDurationPresent
	 *
	 * @return True if default sample duration is present in the box; false otherwise
	 */
	bool defaultSampleDurationPresent(void);

	/**
	 * @fn getDefaultSampleDuration
	 *     Use defaultSampleDurationPresent() beforehand to check for presence.
	 *
	 * @return Default sample duration, 0 if not present
	 *         NOTE: It's possible that 0 may also be a valid value.
	 */
	uint64_t getDefaultSampleDuration();

	/**
	 * @fn setDefaultSampleDuration
	 *     Use defaultSampleDurationPresent() beforehand to check for presence.
	 *
	 * @param[in] sample_duration - Default sample duration value to set
	 */
	void setDefaultSampleDuration(uint64_t sample_duration);

	/**
	 * @fn getDefaultSampleSize
	 *
	 * @return The default sample size, 0 if not present
	 */
	uint32_t getDefaultSampleSize(void);

	/**
	 * @fn constructTfhdBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed TfhdBox object
	 */
	static TfhdBox* constructTfhdBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @class PrftBox
 * @brief Class for ISO BMFF TFHD Box
 */
class PrftBox : public FullBox
{
private:
	uint32_t track_id;
	uint64_t ntp_ts;
	uint64_t media_time;

public:
	/**
	 * @fn PrftBox
	 *
	 * @param[in] sz - box size
	 * @param[in] trackId - media time
	 * @param[in] ntpTs - media time
	 * @param[in] mediaTime - media time
	 */
	PrftBox(uint32_t sz, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime);

	/**
	 * @fn PrftBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] trackId - media time
	 * @param[in] ntpTs - media time
	 * @param[in] mediaTime - media time
	 */
	PrftBox(FullBox &fbox, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime);

	/**
	 * @fn setTrackId
	 *
	 * @param[in] trackId - Track Id value
	 * @return void
	 */
	void setTrackId(uint32_t trackId);

	/**
	 * @fn getTrackId
	 *
	 * @return track_id value
	 */
	uint32_t getTrackId();

	/**
	 * @fn setNtpTs
	 *
	 * @param[in] ntpTs - ntp timestamp value
	 * @return void
	 */
	void setNtpTs(uint64_t ntpTs);

	/**
	 * @fn getNtpTs
	 *
	 * @return ntp_ts value
	 */
	uint64_t getNtpTs();

	/**
	 * @fn setMediaTime
	 *
	 * @param[in] mediaTime - media time value
	 * @return void
	 */
	void setMediaTime(uint64_t mediaTime);

	/**
	 * @fn getMediaTime
	 *
	 * @return media_time value
	 */
	uint64_t getMediaTime();

	/**
	 * @fn constructPrftBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed PrftBox object
	 */
	static PrftBox* constructPrftBox(uint32_t sz, uint8_t *ptr);
};

class SencBox: public FullBox
{
private:
	uint8_t *sampleCountLoc;
	uint32_t numSamples;

public:
	/**
	 * @fn SencBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] sampleCountLoc - sample count location
	 * @param[in] numSamples - number of samples
	 */
	SencBox(FullBox &fbox, uint8_t *sampleCountLoc, uint32_t numSamples);

	/**
	 * @fn constructSencBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed SencBox object
	 */
	static SencBox* constructSencBox(uint32_t sz, uint8_t *ptr);

	/**
	 * @fn truncate
	 *
	 * @param[in] firstSampleSize - Size of the first sample
	 */
	void truncate(uint32_t firstSampleSize) override;
};

class SaizBox : public FullBox
{
private:
	uint8_t *sampleCountLoc;
	uint32_t numSamples;
	uint32_t firstSampleInfoSize;

public:
	/**
	 * @fn SaizBox
	 *
	 * @param[in] fbox - box object
	 * @param[in] sampleCountLoc - location of the sample count
	 * @param[in] numSamples - number of samples
	 * @param[in] sample_info_size - Size for the first auxiliary sample information entry
	 */
	SaizBox(FullBox &fbox, uint8_t *sampleCountLoc, uint32_t numSamples, uint32_t sample_info_size);

	/**
	 * @fn constructSaizBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed SaizBox object
	 */
	static SaizBox* constructSaizBox(uint32_t sz, uint8_t *ptr);

	/*
	* @fn getFirstSampleInfoSize
	*
	* @return The first sample size signalled in the saiz
	*/
	uint32_t getFirstSampleInfoSize(void);

	void truncate(void) override;
};

/**
 * @class SidxBox
 * @brief Class for ISO BMFF SIDX Box
 */
class SidxBox : public FullBox
{
private:
	uint32_t timeScale;
	uint64_t duration;
public:

	/**
	 * @fn SidxBox
	 * @param[in] fbox - box object
	 * @param[in] tScale - TimeScale value
	 * @param[in] sidxDuration - duration from sidx box
	 */
	SidxBox(FullBox &fbox, uint32_t tScale, uint64_t sidxDuration);

	/**
	 * @fn SidxBox
	 *
	 * @param[in] sz - box size
	 * @param[in] tScale - TimeScale value
	 * @param[in] sidxDuration - duration from sidx box
	 */
	SidxBox(uint32_t sz, uint32_t tScale, uint64_t sidxDuration);

	/* @fn constructSidxBox
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed SidxBox object
	 */
	static SidxBox* constructSidxBox(uint32_t sz, uint8_t *ptr);

	/**
	 * @fn getTimeScale
	 *
	 * @return TimeScale value
	 */
	uint32_t getTimeScale();

	/**
	 * @fn setTimeScale
	 *
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setTimeScale(uint32_t tScale);

	/**
	 * @fn getSampleDuration
	 *
	 * @return sampleDuration value
	 */
	uint64_t getSampleDuration();
};

#endif /* __ISOBMFFBOX_H__ */
