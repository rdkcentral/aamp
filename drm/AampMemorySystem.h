/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#ifndef AAMPMEMORYSYSTEM_H
#define AAMPMEMORYSYSTEM_H

/**
 * @file AampMemorySystem.h
 * @brief Memory handler for Aamp DRM process
 */

#include <stdint.h>
#include <vector>
#include <unistd.h>
#include "AampConfig.h"

/**
 * @class AAMPMemorySystem
 * @brief Handles the operations for AAMP memory managemnts
 */
class AAMPMemorySystem {
public:
	/**
	 * @brief Encode a block of data to send over the divide
	 * @param dataIn pointer to the data to encode
	 * @param dataInSz the size to encode
	 * @param out dataOut the data to send
	 * @return true if data is encoded
	 */
	virtual bool encode(const uint8_t *dataIn, uint32_t dataInSz, std::vector<uint8_t>& dataOut) = 0;
	/**
	 * @brief Decode from getting back
	 * @param dataIn pointer to the data to decode
	 * @param size the size to decode
	 * @param out dataOut the data to recover
	 * @param int dataOutSz the size of the space for data to recover
	 */
	virtual bool decode(const uint8_t* dataIn, uint32_t dataInSz, uint8_t *dataOut, uint32_t dataOutSz) = 0;

	/**
	 * @brief Call this if there's an failure external to the MS and it needs to tidy up unexpectedly
	 */
	virtual void terminateEarly() {}

	AAMPMemorySystem(){}
	AAMPMemorySystem(const AAMPMemorySystem&) = delete;
	AAMPMemorySystem& operator=(const AAMPMemorySystem&) = delete;
	virtual ~AAMPMemorySystem() {}
};

/**
 * @class AampMemoryHandleCloser
 * @brief  This just closes a file on descope
 */
class AampMemoryHandleCloser {
public:
	AampMemoryHandleCloser(int handle) : handle_(handle) {};
	~AampMemoryHandleCloser() { if (handle_ > 0) { close(handle_); } }
private:
	int handle_;
};


#endif /* AAMPMEMORYSYSTEM_H */

