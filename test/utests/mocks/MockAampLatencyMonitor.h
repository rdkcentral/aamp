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
 * @file MockAampLatencyMonitor.h
 * @brief Google Mock stand-in for AampLatencyMonitor used by
 *        FakeAampLatencyMonitor to make selected methods controllable
 *        from unit tests without depending on the real implementation.
 */

#ifndef MOCK_AAMP_LATENCY_MONITOR_H
#define MOCK_AAMP_LATENCY_MONITOR_H

#include <gmock/gmock.h>

/**
 * @class MockAampLatencyMonitor
 * @brief Standalone mock — does NOT inherit from AampLatencyMonitor.
 *
 * FakeAampLatencyMonitor holds a global pointer to this class and
 * delegates to it from stub method bodies, following the same pattern
 * used by MockPrivateInstanceAAMP.
 */
class MockAampLatencyMonitor
{
public:
	MOCK_METHOD(double, GetAccumulatedLatencyIncrementMs, (), (const));
};

extern MockAampLatencyMonitor *g_mockAampLatencyMonitor;

#endif /* MOCK_AAMP_LATENCY_MONITOR_H */
