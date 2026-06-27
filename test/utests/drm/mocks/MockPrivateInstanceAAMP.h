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

#ifndef AAMP_MOCK_AAMP_PRIV_AAMP_H
#define AAMP_MOCK_AAMP_PRIV_AAMP_H

#include <gmock/gmock.h>
#include <memory>
#include "priv_aamp.h"

class MockPrivateInstanceAAMP
{
public:
	MOCK_METHOD(void, Individualization, (const std::string &payload));
	MOCK_METHOD(bool, isDecryptClearSamplesRequired, ());
	MOCK_METHOD(void, SendErrorEvent, (AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus, const std::string &responseData));
	MOCK_METHOD(void, SendDrmErrorEvent, (DrmMetaDataEventPtr event, bool isRetryEnabled));
	MOCK_METHOD(void, SendDRMMetaData, (DrmMetaDataEventPtr e));
};

extern std::shared_ptr<MockPrivateInstanceAAMP> g_mockPrivateInstanceAAMP;

#endif /* AAMP_MOCK_AAMP_PRIV_AAMP_H */
