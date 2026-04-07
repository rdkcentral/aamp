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
 * @file FakeAampSourceWorker.cpp
 * @brief No-logic linker stubs for SourceWorker used in L1 tests.
 *
 * These stubs satisfy the linker when AampRialtoPlayer.h is included (it
 * holds std::unique_ptr<SourceWorker> members) without pulling in the real
 * worker thread logic.
 */

#include "AampSourceWorker.h"

SourceWorker::SourceWorker(InjectFn /*injectFn*/) {}
SourceWorker::~SourceWorker() {}

void SourceWorker::postNeedData(
	int32_t /*sourceId*/, uint32_t /*requestId*/, size_t /*frameCount*/) {}

void SourceWorker::cancelNeedData() {}

void SourceWorker::enqueueSamples(std::vector<QueuedSample> /*samples*/) {}

void SourceWorker::setEos() {}

void SourceWorker::flush() {}

void SourceWorker::stop() {}

void SourceWorker::run() {}
