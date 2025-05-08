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

#ifndef AAMP_TSB_AD_PLACEMENT_METADATA_H
#define AAMP_TSB_AD_PLACEMENT_METADATA_H

#include "AampTsbAdMetaData.h"
#include <string>

class PrivateInstanceAAMP;

/**
 * @class AampTsbAdPlacementMetaData
 * @brief Metadata for ad placement events in AAMP TSB.
 */
class AampTsbAdPlacementMetaData : public AampTsbAdMetaData
{
public:
	/**
	 * @brief Constructor
	 * @param[in] eventType Type of the placement event
	 * @param[in] adPosition Advert position in absolute time
	 * @param[in] duration Duration of the placement in seconds
	 * @param[in] adId Identifier for the ad
	 * @param[in] relativePosition Position relative to reservation start
	 * @param[in] offset Ad start offset
	 */
	AampTsbAdPlacementMetaData(EventType eventType, const AampTime& adPosition, uint32_t duration,
							   std::string adId, uint32_t relativePosition, uint32_t offset);

	/**
	 * @brief Copy constructor
	 * @param[in] other The object to copy from
	 */
	AampTsbAdPlacementMetaData(const AampTsbAdPlacementMetaData& other) = default;

	/**
	 * @brief Destructor
	 */
	~AampTsbAdPlacementMetaData() override = default;

	/**
	 * @brief Dump metadata information
	 * @param[in] message Optional message to include in the dump
	 */
	virtual void Dump(const std::string &message = "") const override;

	/**
	 * @brief Send placement-specific ad event
	 * @param[in] aamp Pointer to the AAMP instance for sending events
	 */
	void SendEvent(PrivateInstanceAAMP* aamp) const override;

private:
	std::string mAdId;        		/**< Ad Id */
	uint32_t mRelativePosition;    	/**< Ad Position relative to Reservation Start */
	uint32_t mOffset;         		/**< Ad start offset */
	uint32_t mDuration;         	/**< Ad duration */
};

#endif // AAMP_TSB_AD_PLACEMENT_METADATA_H
