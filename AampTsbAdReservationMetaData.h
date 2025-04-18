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

#ifndef AAMP_TSB_AD_RESERVATION_METADATA_H
#define AAMP_TSB_AD_RESERVATION_METADATA_H

#include "AampTsbAdMetaData.h"
#include <string>

class PrivateInstanceAAMP;

/**
 * @class AampTsbAdReservationMetaData
 * @brief Metadata for ad reservation events in AAMP TSB.
 */
class AampTsbAdReservationMetaData : public AampTsbAdMetaData
{
public:
	/**
	 * @brief Constructor
	 * @param[in] eventType Type of the reservation event
	 * @param[in] adPosition Advert position in absolute time
	 * @param[in] adBreakId Identifier for the ad break
	 * @param[in] periodPosition Start position of the ad break
	 */
	AampTsbAdReservationMetaData(EventType eventType, const AampTime& adPosition,
								 std::string adBreakId, uint64_t periodPosition);

	/**
	 * @brief Copy constructor
	 * @param[in] other The object to copy from
	 */
	AampTsbAdReservationMetaData(const AampTsbAdReservationMetaData& other) = default;

	/**
	 * @brief Destructor
	 */
	~AampTsbAdReservationMetaData() override = default;

	/**
	 * @brief Dump metadata information
	 * @param[in] message Optional message to include in the dump
	 */
	virtual void Dump(const std::string &message = "") const override;

	/**
	 * @brief Send reservation-specific ad event
	 * @param[in] aamp Pointer to the AAMP instance for sending events
	 */
	void SendEvent(PrivateInstanceAAMP* aamp) const override;

private:
	std::string mAdBreakId;    /**< Reservation Id */
	uint64_t mPeriodPosition;  /**< Adbreak's start position */
};

#endif // AAMP_TSB_AD_RESERVATION_METADATA_H
