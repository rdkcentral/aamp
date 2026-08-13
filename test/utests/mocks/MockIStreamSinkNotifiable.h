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

#ifndef MOCK_ISTREAM_SINK_NOTIFIABLE_H
#define MOCK_ISTREAM_SINK_NOTIFIABLE_H

#include <gmock/gmock.h>
#include "IStreamSinkNotifiable.h"

class MockIStreamSinkNotifiable : public IStreamSinkNotifiable
{
public:
	MOCK_METHOD(void, NotifyFirstFrameReceived,
		(unsigned long ccDecoderHandle), (override));

	MOCK_METHOD(void, NotifyFirstBufferProcessed,
		(const std::string &videoRectangle), (override));

	MOCK_METHOD(void, NotifyFirstVideoFrameDisplayed, (), (override));

	MOCK_METHOD(void, LogFirstFrame, (), (override));

	MOCK_METHOD(void, LogTuneComplete, (), (override));

	MOCK_METHOD(void, NotifyEOSReached, (), (override));

	MOCK_METHOD(void, MonitorProgress,
		(bool sync, bool beginningOfStream), (override));

	MOCK_METHOD(double, GetProgressReportIntervalSeconds,
		(), (override));

	MOCK_METHOD(void, NotifySpeedChanged,
		(float rate, bool changeState), (override));

	MOCK_METHOD(AAMPPlayerState, GetState, (), (override));

	MOCK_METHOD(void, NotifyBufferUnderflow,
		(AampMediaType type), (override));

	MOCK_METHOD(void, CompleteDiscontinuityDataDeliverForPTSRestamp,
		(AampMediaType type), (override));

	MOCK_METHOD(void, NotifyPipelinePausedToUnderflowMonitor, (), (override));

	MOCK_METHOD(void, SendMonitorAvEvent,
		(const std::string &status,
		 int64_t videoPositionMs,
		 int64_t audioPositionMs,
		 uint64_t timeInStateMs,
		 uint64_t droppedFrames), (override));
};

#endif // MOCK_ISTREAM_SINK_NOTIFIABLE_H
