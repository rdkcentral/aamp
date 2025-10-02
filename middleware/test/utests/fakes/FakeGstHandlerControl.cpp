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
#include "MockGstHandlerControl.h"
#include "GstHandlerControl.h"

MockGstHandlerControl *g_mockGstHandlerControl = nullptr;

GstHandlerControl::ScopeHelper&  GstHandlerControl::ScopeHelper::operator=(GstHandlerControl::ScopeHelper&& other)
{
	return *this;
}

void GstHandlerControl::handlerEnd()
{
}

bool GstHandlerControl::isEnabled() const
{
    bool retvalue = false;
    if (g_mockGstHandlerControl != nullptr)
    {
        retvalue = g_mockGstHandlerControl->isEnabled();
    }
    return retvalue;
}

GstHandlerControl::ScopeHelper GstHandlerControl::getScopeHelper()
{
	return GstHandlerControl::ScopeHelper(this);
}

bool GstHandlerControl::waitForDone(int MaximumDelayMilliseconds, std::string name)
{
	return true;
}
