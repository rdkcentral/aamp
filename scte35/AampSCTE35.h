/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#ifndef AAMP_SCTE35_H
#define AAMP_SCTE35_H

#include <exception>
#include <cstdint>
#include <cstdbool>
#include <string>
#include <vector>
#include <memory>
#include <cJSON.h>
#include "AampUtils.h"
#include "AampLogManager.h"
#include "AampConfig.h"

/* Forward class references */
class SCTE35Section;
class SCTE35DescriptorLoop;
class SCTE35Decoder;
class SCTE35DecoderDescriptorLoop;

/**
 * @brief SCTE-35 section data exception
 */
class SCTE35DataException : public std::exception
{
public:
	/**
	 * @brief SCTE-35 section data exception constructor
	 * @param[in] message Exception message
	 */
	SCTE35DataException(const std::string &message) : mMessage(message) {}

	/**
	 * @brief SCTE-35 section data exception explanation
	 * @return Explanation string
	 */
	const char *what() const noexcept override
	{
		return mMessage.c_str();
	}

private:
	std::string mMessage;				/**< @brief Exception explanation message */
};

/**
 * @brief Interface class for processing SCTE-35 section data
 */
class SCTE35Section
{
public:
	/**
	 * @brief Virtual destructor
	 */
	virtual ~SCTE35Section() {}

	/**
	 * @brief Single bit flag
	 *
	 * @param[in] key Data element name
	 * @return Data element value
	 * @throw SCTE35DataException on attempting to access past the end of the
	 *        section data
	 */
	virtual bool Bool(const char *key) = 0;

	/**
	 * @brief Aligned eight bit integer
	 *
	 * @param[in] key Data element name
	 * @return Data element value
	 * @throw SCTE35DataException on attempting a non-aligned access or an
	 *        access past the end of the section data
	 */
	virtual uint8_t Byte(const char *key) = 0;

	/**
	 * @brief Byte aligned sixteen bit integer
	 *
	 * @param[in] key Data element name
	 * @return Data element value
	 * @throw SCTE35DataException on attempting a non-byte-aligned access or
	 *        an access past the end of the section data
	 */
	virtual uint16_t Short(const char *key) = 0;

	/**
	 * @brief Byte aligned thirty two bit integer
	 *
	 * @param[in] key Data element name
	 * @return Data element value
	 * @throw SCTE35DataException on attempting a non-byte-aligned access or
	 *        an access past the end of the section data
	 */
	virtual uint32_t Integer(const char *key) = 0;

	/**
	 * @brief Unaligned multiple bits
	 *
	 * Note that if the data element is represented by a JSON value, then some
	 * data bits may be lost.
	 *
	 * @param[in] key Data element name
	 * @param[in] bits Number of bits
	 * @return Data element value
	 * @throw SCTE35DataException on attempting an access past the end of the
	 *        section data
	 */
	virtual uint64_t Bits(const char *key, int bits) = 0;

	/**
	 * @brief Unaligned multiple reserved bits
	 *
	 * @param[in] bits Number of bits
	 * @throw SCTE35DataException if any reserved bit is zero or on attempting
	 *        an access past the end of the section data
	 */
	virtual void ReservedBits(int bits) = 0;

	/**
	 * @brief Skip bytes
	 *
	 * @param[in] bytes Number of bytes to skip
	 * @throw SCTE35DataException on attempting a non-aligned access or an
	 *        access past the end of the section data
	 */
	virtual void SkipBytes(int bytes) = 0;

	/**
	 * @brief Byte aligned string
	 *
	 * @param[in] key Data element name
	 * @param[in] bytes String length in bytes
	 * @return Data element value
	 * @throw SCTE35DataException on attempting a non-aligned access or an
	 *        access past the end of the section data
	 */
	virtual std::string String(const char *key, int bytes) = 0;

	/**
	 * @brief Byte aligned thirty two bit CRC32 value
	 *
	 * An MPEG CRC32 value is calculated on the whole of the section data.
	 *
	 * @return Data element value
	 * @throw SCTE35DataException on reading an incorrect CRC32 value or
	 *        attempting a non-byte-aligned access or an access past the end of
	 *        the section data
	 */
	virtual uint32_t CRC32() = 0;

	/**
	 * @brief End of section or subsection data
	 *
	 * @throw SCTE35DataException if not byte-aligned access or if not at
	 *        the end of section or subsection data
	 */
	virtual void End() = 0;

	/**
	 * @brief Start of data element subsection
	 *
	 * End the subsection by calling the returned instance's End method.
	 *
	 * @param[in] key Data subsection name
	 * @param[in] bytes Data subsection length in bytes
	 * @return SCTE35Section instance
	 * @throw SCTE35DataException on the subsection data is not byte aligned
	 *        or if it extends beyond the parent section data
	 */
	virtual SCTE35Section *Subsection(const char *key, int bytes) = 0;

	/**
	 * @brief Start of descriptor loop
	 *
	 * End the descriptor loop by calling the returned instance's End method.
	 *
	 * @param[in] key Descriptor loop name, typically "descriptors"
	 * @param[in] bytes Descriptor loop data length in bytes
	 * @return SCTE35DescriptorLoop instance
	 * @throw SCTE35DataException on the descriptor loop data is not byte
	 *        aligned or if it extends beyond the parent section data
	 */
	virtual SCTE35DescriptorLoop *DescriptorLoop(const char *key, int bytes) = 0;

	/**
	 * @brief Check if the current bit offset is at the end of the section data
	 *
	 * @retval true if the current bit offset is at the end of the section data
	 * @retval false if the current bit offset is not at the end of the section data
	 */
	virtual bool isEnd() = 0;
};

/**
 * @brief Interface class for processing SCTE-35 descriptor loop section data
 */
class SCTE35DescriptorLoop
{
public:
	/**
	 * @brief Virtual destructor
	 */
	virtual ~SCTE35DescriptorLoop() {}

	/**
	 * @brief See if there is another descriptor in the SCTE-35 section data
	 *        descriptor loop
	 *
	 * @retval true if there is another descriptor
	 * @retval false if there are no more descriptors
	 */
	virtual bool hasAnotherDescriptor() = 0;

	/**
	 * @brief Start of descriptor in the SCTE-35 section data descriptor loop
	 *
	 * End the descriptor by calling the returned instance's End method.
	 *
	 * @return SCTE35Section instance
	 * @throw SCTE35DataException if the descriptor starts beyond the parent
	 *        section data
	 */
	virtual SCTE35Section *Descriptor() = 0;

	/**
	 * @brief End of SCTE-35 section data descriptor loop
	 *
	 * @throw SCTE35DataException if the end of the section data has not
	 *        been reached
	 */
	virtual void End() = 0;
};

/**
 * SCTE-35 section data decoder class
 */
class SCTE35Decoder : public SCTE35Section
{
public:
	/**
	 * Public constructor
	 *
	 * @param[in] string Base64 encoded SCTE-35 signal string
	 */
	SCTE35Decoder(std::string string);

	/**
	 * Constructor used to implement descriptor loops
	 *
	 * @param[in] parent Parent decoder instance
	 * @param[in] descriptorLoop Descriptor loop instance
	 * @param[in] loopMaxOffset End of section data measured in bits
	 */
	SCTE35Decoder(SCTE35Decoder *parent, SCTE35DecoderDescriptorLoop *descriptorLoop, size_t loopMaxOffset);

	/**
	 * Constructor used to implement data subsections
	 *
	 * @param[in] parent Parent decoder instance
	 * @param[in] len Subsection data length in bytes
	 */
	SCTE35Decoder(const char *key, SCTE35Decoder *parent, int len);

	/**
	 * @brief destructor
	 */
	~SCTE35Decoder();

	bool Bool(const char *key) override;
	uint8_t Byte(const char *key) override;
	uint16_t Short(const char *key) override;
	uint32_t Integer(const char *key) override;
	uint64_t Bits(const char *key, int bits) override;
	void ReservedBits(int bits) override;
	void SkipBytes(int bytes) override;
	std::string String(const char *key, int bytes) override;
	uint32_t CRC32() override;
	void End() override;
	SCTE35Section *Subsection(const char *key, int bytes) override;
	SCTE35DescriptorLoop *DescriptorLoop(const char *key, int bytes) override;
	bool isEnd() override;

	/**
	 * @brief Get a JSON representation of the section data
	 *
	 * Ownership of the JSON instance is transferred to the caller. Subsequent
	 * calls to this method will return NULL.
	 *
	 * @return cJSON instance
	 */
	cJSON *getJson();

	/**
	 * @brief Add a JSON array of descriptors
	 *
	 * Used to implement descriptor loops.
	 *
	 * @param[in] key Descriptor loop name, typically "descriptors"
	 * @param[in] objects JSON array object
	 * @param[in] expectedOffset Expected section data offset in bits
	 * @throw SCTE35DataException if expectedOffset does not match the
	 *        current section data bit offset
 	 */
	void addDescriptors(const char *key, cJSON *objects, size_t expectedOffset);

	/**
	 * @brief Check the current bit offset
	 *
	 * @param[in] offset Current bit offset
	 * @retval true if the current bit offset is valid
	 * @retval false if the current bit offset past the end of the section data
	 */
	bool checkOffset(size_t offset);

	//copy constructor
	SCTE35Decoder(const SCTE35Decoder&)=delete;
	//copy assignment operator
	SCTE35Decoder& operator=(const SCTE35Decoder&)=delete;

private:
	size_t mOffset;						/**< @brief Section data bit offset */
	size_t mMaxOffset;					/**< @brief Section data maximum bit offset */
	uint8_t *mData;						/**< @brief Section data */
	SCTE35Decoder *mParent;				/**< @brief Parent decoder or NULL*/
	cJSON *mJsonObj;					/**< @brief JSON representation of the section data*/
	SCTE35DecoderDescriptorLoop *mLoop;	/**< @brief Current descriptor loop instance */
	std::string mKey;					/**< @brief Subsection object name */
};

/**
 * SCTE-35 section data decoder descriptor loop class
 */
class SCTE35DecoderDescriptorLoop : public SCTE35DescriptorLoop
{
public:
	/**
	 * @brief Constructor
	 *
	 * @param[in] key Descriptor loop name, typically "descriptors"
	 * @param[in] decoder Decoder instance
	 * @param[in] maxOffset Maximum section data offset measured in bits
	 */
	SCTE35DecoderDescriptorLoop(const std::string &key, SCTE35Decoder *decoder, size_t maxOffset);

	/**
	 * @brief Destructor
	 */
	~SCTE35DecoderDescriptorLoop();

	bool hasAnotherDescriptor() override;
	SCTE35Section *Descriptor() override;
	void End() override;

	/**
	 * @brief Add a JSON representation of a descriptor
	 *
	 * Note that the ownership of object is passed to the callee.
	 *
	 * @param[in] object Descriptor JSON object
	 */
	void add(cJSON *object);

	//copy constructor
	SCTE35DecoderDescriptorLoop(const SCTE35DecoderDescriptorLoop&)=delete;
	//copy assignment operator
	SCTE35DecoderDescriptorLoop& operator=(const SCTE35DecoderDescriptorLoop&)=delete;

private:
	std::string mKey;					/**< @brief Data element name */
	SCTE35Decoder *mDecoder;			/**< @brief Decoder instance */
	size_t mMaxOffset;					/**< @brief Maximum section data offset measured in bits */
	cJSON *mObjects;					/**< @brief JSON representation of the descriptors */
};

/**
 * @brief SCTE-35 splice info signal class
 */
class SCTE35SpliceInfo
{
public:
	/**
	 * @brief SCTE-35 segmentation type id values
	 *
	 * An SCTE-35 signal can contain multiple segmentation types.
	 */
	enum class SEGMENTATION_TYPE
	{
		NOT_INDICATED = 0x00,
		BREAK_START = 0x22,
		BREAK_END = 0x23,
		PROVIDER_ADVERTISEMENT_START = 0x30,
		PROVIDER_ADVERTISEMENT_END = 0x31,
		PROVIDER_PLACEMENT_OPPORTUNITY_START = 0x34,
		PROVIDER_PLACEMENT_OPPORTUNITY_END = 0x35,
		DISTRIBUTOR_PLACEMENT_OPPORTUNITY_START = 0x36,
		DISTRIBUTOR_PLACEMENT_OPPORTUNITY_END = 0x37,
		PROVIDER_AD_BLOCK_START = 0x44,
		PROVIDER_AD_BLOCK_END = 0x45
	};

	/**
	 * @brief SCTE-35 splice info signal summary
	 *
	 * Note that the signal time value will wrap every (2^33)/90000
	 * (PTS_WRAP_TIME) seconds.
	 */
	struct Summary
	{
		SEGMENTATION_TYPE type;		/**< @brief Signal type */
		double time;				/**< @brief Time in seconds (modulus PTS_WRAP_TIME) */
		double duration;			/**< @brief Duration in seconds */
		uint32_t event_id;			/**< Event id */
	};

	/** @brief PTS ticks per second */
	static constexpr double TIMESCALE = 90000.0;

	/** @brief PTS time modulus value in seconds */
	static constexpr double PTS_WRAP_TIME = (double)0x200000000/TIMESCALE;

	/**
	 * @brief Constructor
	 *
	 * @param[in] string Base64 encoded SCTE-35 signal
	 */
	SCTE35SpliceInfo(const std::string &string);

	/**
	 * @brief Destructor
	 */
	~SCTE35SpliceInfo();

	/**
	 * @brief Get JSON string
	 *
	 * @param[in] formatted Optionally set to true to get a formatted string
	 * @return JSON string representation of the SCTE-35 signal
	 */
	std::string getJsonString(bool formatted = false);

	/**
	 * @brief Get a summary of the splice info signal
	 *
	 * Multiple splice descriptors may be signalled. An empty summary vector is
	 * returned on error.
	 *
	 * Note that the signal time value will wrap every (2^33)/90000
	 * (PTS_WRAP_TIME) seconds.
	 *
	 * @param[out] summary A summary of the signal
	 */
	void getSummary(std::vector<Summary> &summary);

	//copy constructor
	SCTE35SpliceInfo(const SCTE35SpliceInfo&)=delete;
	//copy assignment operator
	SCTE35SpliceInfo& operator=(const SCTE35SpliceInfo&)=delete;

private:
	cJSON *mJsonObj;					/**< JSON representation of the signal */
};

#endif /* AAMP_SCTE35_H */
