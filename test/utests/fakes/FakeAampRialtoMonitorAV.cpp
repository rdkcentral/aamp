/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

/**
 * @file FakeAampRialtoMonitorAV.cpp
 * @brief No-op stubs for AampRialtoMonitorAV used in L1 tests that pull in
 *        AampRialtoPlayer.h (via FakeAampRialtoPlayer.cpp) but do not
 *        exercise the direct-Rialto AV monitoring path.
 */

#include "AampRialtoMonitorAV.h"

AampRialtoMonitorAV::AampRialtoMonitorAV(
	std::shared_ptr<firebolt::rialto::IMediaPipeline> /*pipeline*/,
	IStreamSinkNotifiable * /*notifiable*/,
	std::function<int32_t()> /*videoSourceIdGetter*/,
	std::function<int()> /*rateGetter*/,
	std::function<bool()> /*isPlayingGetter*/,
	Config /*config*/)
{
}

AampRialtoMonitorAV::~AampRialtoMonitorAV()
{
}

void AampRialtoMonitorAV::start()
{
}

void AampRialtoMonitorAV::stop()
{
}
