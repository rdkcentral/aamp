/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#ifndef AAMP_MOCK_AAMP_EVENT_MANAGER_H
#define AAMP_MOCK_AAMP_EVENT_MANAGER_H

#include <gmock/gmock.h>
#include "AampEventManager.h"

MATCHER_P(AnEventOfType, type, "") { return type == arg->getType(); }

MATCHER_P(SpeedChanged, rate, "")
{
    bool match = false;

    if (AAMP_EVENT_SPEED_CHANGED == arg->getType())
    {
        SpeedChangedEventPtr ev = std::dynamic_pointer_cast<SpeedChangedEvent>(arg);
        match = (rate == ev->getRate());
    }
    return match;
}

MATCHER_P(StateChanged, state, "")
{
    bool match = false;

    printf("arg->getType() = %d", arg->getType());
    if (AAMP_EVENT_STATE_CHANGED == arg->getType())
    {
        StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(arg);
        printf("state %d ev->getState() = %d", state, ev->getState());
        match = (state == ev->getState());
    }
    return match;
}

MATCHER_P4(AdResolved, resolveStatus, asId, errorCode, errorDescription, "")
{
    bool match = false;

    if (AAMP_EVENT_AD_RESOLVED == arg->getType())
    {
        AdResolvedEventPtr ev = std::dynamic_pointer_cast<AdResolvedEvent>(arg);
        match = (resolveStatus == ev->getResolveStatus() &&
                 asId == ev->getAdId() &&
                 errorCode == ev->getErrorCode() &&
                 errorDescription == ev->getErrorDescription());
    }
    return match;
}

class MockAampEventManager
{
public:

    MOCK_METHOD(void, SendEvent, (const AAMPEventPtr &eventData, AAMPEventMode eventMode));

    MOCK_METHOD(bool, IsEventListenerAvailable, (AAMPEventType eventType));
};

extern MockAampEventManager *g_mockAampEventManager;

#endif /* AAMP_MOCK_AAMP_EVENT_MANAGER_H */
