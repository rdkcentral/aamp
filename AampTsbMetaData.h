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

#ifndef AAMP_TSB_METADATA_H
#define AAMP_TSB_METADATA_H

#include <string>
#include <cstdint>
#include "AampTime.h"

/**
 * @class AampTsbMetaData
 * @brief Base class for TSB metadata.
 */
class AampTsbMetaData
{
public:

	/**
	 * @enum Type
	 * @brief Type definition for TSB metadata type identifiers.
	 */
	enum class Type
	{
		AD_METADATA_TYPE,	/**< Ad Metadata */
	};

	/**
	 * @brief Constructor
	 * @param[in] absPosition Metadata position in absolute time.
	 */
	AampTsbMetaData(const AampTime& absPosition);

	/**
	 * @brief Virtual destructor
	 */
	virtual ~AampTsbMetaData() = default;

	/**
	 * @brief Get the type of the metadata.
	 * @return Metadata type.
	 */
	virtual Type GetType() const = 0;

	/**
	 * @brief Get the metadata position.
	 * @return Metadata position.
	 */
	virtual AampTime GetPosition() const;

	/**
	 * @brief Set the metadata position.
	 * @param[in] position New metadata position.
	 */
	virtual void SetPosition(const AampTime& absPosition);

	/**
	 * @brief Dump metadata information.
	 * @param[in] message Optional message to include in the dump.
	 */
	virtual void Dump(const std::string &message = "") const = 0;

	/**
	 * @brief Get the metadata orderAdded value.
	 * @return Metadata orderAdded, used for secondary sort when positions are equal.
	 */
	virtual uint32_t GetOrderAdded() const;

	/**
	 * @brief Set the metadata orderAdded value.
	 * @param[in] orderAdded New metadata orderAdded.
	 */
	virtual void SetOrderAdded(uint32_t orderAdded);

protected:
	AampTime mPosition;       /**< Metadata position as absolute time, primary sorting */
	uint32_t mOrderAdded;     /**< Indicates order metadata was added, secondary sorting */
};

#endif // AAMP_TSB_METADATA_H
