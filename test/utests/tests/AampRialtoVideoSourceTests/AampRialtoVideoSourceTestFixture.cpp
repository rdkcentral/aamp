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
 * @file AampRialtoVideoSourceTestFixture.cpp
 * @brief Out-of-line fixture set-up, compiled once instead of in every test file.
 */

#include "AampRialtoVideoSourceTestFixture.h"

void AampRialtoVideoSourceTest::SetUp()
{
	m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
	m_pipelinePtr  = m_mockPipeline.get();

	ON_CALL(*m_pipelinePtr, attachSource(_))
		.WillByDefault(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));

	ON_CALL(*m_pipelinePtr, setSourcePosition(_, _, _, _, _))
		.WillByDefault(Return(true));
}

void AampRialtoVideoSourceWithDemuxTest::SetUp()
{
	AampRialtoVideoSourceTest::SetUp();
	g_mockMp4Demux = std::make_shared<NiceMock<MockMp4Demux>>();
	// Prime the demuxer lazily via processInitFragment so that
	// subsequent processDataFragment tests have an initialised demuxer.
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	auto buf = std::make_shared<std::vector<uint8_t>>(
		std::vector<uint8_t>{0x00});
	m_source.processInitFragment(std::move(buf));
}

void AampRialtoVideoSourceWithDemuxTest::TearDown()
{
	g_mockMp4Demux.reset();
	AampRialtoVideoSourceTest::TearDown();
}
