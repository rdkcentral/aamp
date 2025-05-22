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

#ifndef AD_TSB_METADATA_H
#define AD_TSB_METADATA_H

#include "AampTsbMetaData.h"

class PrivateInstanceAAMP;

/**
 * @class AampTsbAdMetaData
 * @brief Common base class for all ad-related metadata.
 */
class AampTsbAdMetaData : public AampTsbMetaData
{
public:
	/**
	 * @enum AdType
	 * @brief Type of ad metadata.
	 */
	enum class AdType
	{
		RESERVATION,   /**< Ad Reservation Metadata */
		PLACEMENT      /**< Ad Placement Metadata */
	};

	/**
	 * @enum EventType
	 * @brief Type of events.
	 */
	enum class EventType
	{
		START,    /**< Ad Start */
		END,      /**< Ad End */
		ERROR     /**< Ad Error */
	};

	/**
	 * @brief Constructor
	 * @param[in] adType Type of the ad metadata.
	 * @param[in] eventType Type of the event.
	 * @param[in] adPosition Position of advert in absolute time.
	 */
	AampTsbAdMetaData(AdType adType, EventType eventType, const AampTime& adPosition);

	/**
	 * @brief Get the type of the metadata.
	 * @return Metadata type.
	 */
	virtual AampTsbMetaData::Type GetType() const override
	{
		return AampTsbMetaData::Type::AD_METADATA_TYPE;
	}

	/**
	 * @brief Get the ad metadata type.
	 * @return Ad metadata type.
	 */
	virtual AdType GetAdType() const;

	/**
	 * @brief Get the ad event type.
	 * @return The event type.
	 */
	virtual EventType GetEventType() const;

	/**
	 * @brief Send the appropriate ad event based on metadata type
	 * @param[in] aamp Pointer to the AAMP instance for sending events
	 */
	virtual void SendEvent(PrivateInstanceAAMP* aamp) const = 0;

protected:
	AdType mAdType;           /**< Type of ad metadata */
	EventType mEventType;     /**< Ad Event Type */
};

#endif // AD_TSB_METADATA_H
