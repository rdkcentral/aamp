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
#ifndef AAMP_MOCK_TSB_DATAMANAGER_H
#define AAMP_MOCK_TSB_DATAMANAGER_H

#include <gmock/gmock.h>
#include "AampTsbDataManager.h"

using namespace TSB;
class MockTSBDataManager
{
public:
	MOCK_METHOD(std::shared_ptr<TsbFragmentData>, GetFragment, (double position, bool& eos));
	MOCK_METHOD(std::shared_ptr<TsbFragmentData>, RemoveFragment, (bool &initDeleted));
	MOCK_METHOD(TsbFragmentDataPtr, GetFirstFragment, ());
	MOCK_METHOD(TsbFragmentDataPtr, GetLastFragment, ());
	MOCK_METHOD(TsbFragmentDataPtr, GetNearestFragment, (double position));
};
extern MockTSBDataManager *g_mockTSBDataManager;
#endif /* AAMP_MOCK_TSB_DATAMANAGER_H */


