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
 * @file FakePrivateInstanceAAMPNotifiable.cpp
 * @brief Stub implementation of PrivateInstanceAAMPNotifiable for test targets
 *        that link FakeAampRialtoPlayer but do not exercise the Rialto path.
 *
 * Provides the vtable and constructor so that unique_ptr<> destruction inside
 * ~AampRialtoPlayer() compiles and links without pulling in priv_aamp.h.
 * All methods are no-ops; they are never called in tests that use the fake.
 */

#include "PrivateInstanceAAMPNotifiable.h"

PrivateInstanceAAMPNotifiable::PrivateInstanceAAMPNotifiable(
	PrivateInstanceAAMP * /*aamp*/) noexcept
{
}

void PrivateInstanceAAMPNotifiable::NotifyFirstFrameReceived(
	unsigned long /*ccDecoderHandle*/) {}

void PrivateInstanceAAMPNotifiable::NotifyFirstBufferProcessed(
	const std::string & /*videoRectangle*/) {}

void PrivateInstanceAAMPNotifiable::NotifyFirstVideoFrameDisplayed() {}

void PrivateInstanceAAMPNotifiable::LogFirstFrame() {}

void PrivateInstanceAAMPNotifiable::LogTuneComplete() {}

void PrivateInstanceAAMPNotifiable::NotifyEOSReached() {}

void PrivateInstanceAAMPNotifiable::MonitorProgress(
	bool /*sync*/, bool /*beginningOfStream*/) {}

double PrivateInstanceAAMPNotifiable::GetProgressReportIntervalSeconds()
{
	return 0.0;
}

void PrivateInstanceAAMPNotifiable::NotifySpeedChanged(
	float /*rate*/, bool /*changeState*/) {}

AAMPPlayerState PrivateInstanceAAMPNotifiable::GetState()
{
	return eSTATE_IDLE;
}

void PrivateInstanceAAMPNotifiable::NotifyBufferUnderflow(
	AampMediaType /*type*/) {}
