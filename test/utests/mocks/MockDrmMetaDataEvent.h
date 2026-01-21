
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

#ifndef MOCK_DRMMETADATA_EVENT_H
#define MOCK_DRMMETADATA_EVENT_H

#include <gmock/gmock.h>
#include "AampEvent.h"

// Minimal Google Mock for DrmMetaDataEvent used by unit tests.
// Tests construct this mock and set expectations on setFailure()/getFailure().
class MockDrmMetaDataEvent : public DrmMetaDataEvent
{
public:
	MockDrmMetaDataEvent(AAMPTuneFailure failure,
						 const std::string &accessStatus,
						 int statusValue,
						 int responseCode,
						 bool secclientErr,
						 std::string sid)
		: DrmMetaDataEvent(failure, accessStatus, statusValue, responseCode, secclientErr, std::move(sid))
	{
	}

	MOCK_METHOD(void, setFailure, (AAMPTuneFailure), ());
	MOCK_METHOD(AAMPTuneFailure, getFailure, (), (const));

	// Additional commonly-used setters/getters added to avoid future compile issues
	MOCK_METHOD(void, setResponseCode, (int), ());
	MOCK_METHOD(int, getResponseCode, (), (const));
	MOCK_METHOD(void, setSecclientError, (bool), ());
	MOCK_METHOD(bool, getSecclientError, (), (const));
	MOCK_METHOD(void, setAccessStatus, (const std::string &), ());
	MOCK_METHOD(const std::string &, getAccessStatus, (), (const));
};

// Global pointer used by fakes to delegate behavior to the test's mock instance


#endif // MOCK_DRMMETADATA_EVENT_H
