/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#ifndef PLAYER_MOCK_OPEN_CDM_SESSION_ADAPTER_H
#define PLAYER_MOCK_OPEN_CDM_SESSION_ADAPTER_H

#include <gmock/gmock.h>

extern std::vector<uint8_t> g_mockKeyId;

/**
 * @class MockOpenCdmSessionAdapter
 * @brief Mock implementation of OpenCDM session adapter for DRM testing
 * 
 * This mock class provides a test double for the OpenCDM session adapter,
 * allowing controlled testing of DRM-related functionality without requiring
 * actual DRM system interaction. It supports key management operations
 * and output protection verification for comprehensive unit testing.
 */
class MockOpenCdmSessionAdapter
{
    public:

        MOCK_METHOD(bool, verifyOutputProtection, ());

        /**
         * @brief Mock method to retrieve usable DRM keys
         * @return const std::vector<std::vector<uint8_t>>& Reference to a vector containing usable key data
         * 
         * This mock method simulates the retrieval of usable DRM keys from the CDM session.
         * Each inner vector represents a key identifier or key data that can be used for
         * content decryption. In tests, this method can be configured to return specific
         * key sets to verify proper handling of available keys by the system under test.
         * 
         * @note This is a const method as it should not modify the session state
         * @see setKeyId() for configuring available keys in test scenarios
         */
        MOCK_METHOD(const std::vector<std::vector<uint8_t>>&, getUsableKeys, (), (const));

        /**
         * @brief Mock method to set or configure a key identifier
         * @param keyId The key identifier to set, provided as a vector of bytes
         * 
         * This mock method simulates the configuration of a specific key identifier
         * in the CDM session. The key identifier is typically used to associate
         * content keys with specific media content or tracks. In test scenarios,
         * this method allows setting up specific key IDs to verify proper key
         * handling and selection logic.
         * 
         * @param keyId Vector containing the binary representation of the key identifier
         * @note This method may affect the behavior of getUsableKeys() in mock scenarios
         */
        MOCK_METHOD(void, setKeyId, (const std::vector<uint8_t>&));
};

extern std::unique_ptr<MockOpenCdmSessionAdapter> g_mockOpenCdmSessionAdapter;

#endif /* PLAYER_MOCK_OPEN_CDM_SESSION_ADAPTER_H */
