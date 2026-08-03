/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ElementaryProcessor.h>
#include <string>
#include <stdint.h>
#include <iostream>

#include "MockPrivateInstanceAAMP.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

PrivateInstanceAAMP *mPrivateInstanceAAMP{};

const int tsPacketLength = 188;

class  ElementaryProcessorTest: public ::testing::Test {
protected:
    void SetUp() override 
    {
        mPrivateInstanceAAMP = new PrivateInstanceAAMP();
        mElementaryProcessor = new TestElementaryProcessor(mPrivateInstanceAAMP);
    
    }
    
    void TearDown() override 
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete mElementaryProcessor;
        mElementaryProcessor = nullptr;
    }

    class TestElementaryProcessor : public ElementaryProcessor
    {
    public:
        TestElementaryProcessor(PrivateInstanceAAMP *mPrivateInstanceAAMP)
            : ElementaryProcessor(mPrivateInstanceAAMP)
        {
        }

        // bool CallsetTuneTimePTS(char *segment, size_t& size, double position, double duration, bool discontinuous, bool &ptsError)
        // {
        //     char segmentData[] = {'a','b','c','d'};
        //     segment = segmentData;
        //     // segment = new char[1000];
	    //     size = sizeof(segment);
        //     position = 1.2;
        //     duration = 1.3;
        //     discontinuous = true;
        //     ptsError = true;
        //     bool TuneTimePTSResult = setTuneTimePTS(segment, size, position, duration, discontinuous, ptsError);       
        //     return TuneTimePTSResult;
        // }

    };

    TestElementaryProcessor* mElementaryProcessor;
};

// TEST_F(ElementaryProcessorTest, CallsetTuneTimePTSTest)
// {
//     char segmentData[] = {'a','b','c','d'};
//     char *segment = segmentData;
//     // char segment = new char[1000];
// 	size_t size = sizeof(segment);
//     double position = 1.2;
//     double duration = 1.3;
//     bool discontinuous = true;
//     bool ptsError = true;
//     //mPrivateInstanceAAMP->SendStreamCopy((AampMediaType)eMEDIATYPE_SUBTITLE, segment, size, position, position, duration);
//     bool TuneTimePTSResult = mElementaryProcessor->CallsetTuneTimePTS(segment, size, position, duration, discontinuous, ptsError);       
// }

// TEST_F(ElementaryProcessorTest, sendSegmentTest)
// {
    // char segmentData[] = {'a','b','c','d'};
    // char *segment = segmentData;
    // // char *segment = new char[100];
	// size_t size = sizeof(segment);
    // double position = 1.2;
    // double duration = 1.3;
    // bool discontinuous = true;
    // MediaProcessor::process_fcn_t processor;
    // bool ptsError = true;
    //mPrivateInstanceAAMP->SendStreamCopy((AampMediaType)eMEDIATYPE_SUBTITLE, segment, size, position, position, duration);
    //bool TuneTimePTSResult = mElementaryProcessor->sendSegment(segment, size, position, duration, discontinuous, processor, ptsError);    
// }

// TEST_F(ElementaryProcessorTest, sendSegmentTest)
// {
//     char *segment = new char[100];
// 	AampGrowableBuffer buffer("tsProcessor PAT/PMT test");
// 	double position = 0;
// 	double duration = 2.43;
// 	bool discontinuous = false;
// 	bool isInit = false;
//     MediaProcessor::process_fcn_t processor;
// 	bool ptsError = false;
// 	buffer.AppendBytes(segment,tsPacketLength*2);
// 	mElementaryProcessor->sendSegment(&buffer, position, duration, discontinuous,isInit,
// 		[this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
// 		{
// 			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
// 		},
// 		ptsError
// 	);
// 	buffer.Free();
// }

TEST_F(ElementaryProcessorTest, abortInjectionWaitTest)
{
    mElementaryProcessor->abortInjectionWait();
}

TEST_F(ElementaryProcessorTest, abortTest)
{
    mElementaryProcessor->abort();
}

TEST_F(ElementaryProcessorTest, resetTest)
{
    mElementaryProcessor->reset();
}

TEST_F(ElementaryProcessorTest, setRateTest1)
{
    double rate = 1.2;
    PlayMode mode = PlayMode::PlayMode_normal;
    mElementaryProcessor->setRate(rate, mode);
}

TEST_F(ElementaryProcessorTest, setRateTest2)
{
    //Checking the Maximum value of rate
    double rate = DBL_MAX;
    PlayMode mode = PlayMode::PlayMode_normal;
    mElementaryProcessor->setRate(rate, mode);
}

TEST_F(ElementaryProcessorTest, setRateTest3)
{
    //Checking the Minimum value of rate
    double rate = DBL_MIN;
    PlayMode mode = PlayMode::PlayMode_normal;
    mElementaryProcessor->setRate(rate, mode);
}

TEST_F(ElementaryProcessorTest, setRateTest4)
{
    //Checking the zero value of rate
    double rate = 0.0;
    PlayMode mode = PlayMode::PlayMode_normal;
    mElementaryProcessor->setRate(rate, mode);
}

TEST_F(ElementaryProcessorTest, setRateTest5)
{
    //Checking the zero value of rate
    double rate = 0.0;
    PlayMode mode[] = {
        PlayMode_normal,
	    PlayMode_retimestamp_IPB,
	    PlayMode_retimestamp_IandP,
	    PlayMode_retimestamp_Ionly,
	    PlayMode_reverse_GOP
    };
    for(PlayMode playerMode : mode)
    {
        mElementaryProcessor->setRate(rate, playerMode);
    }
}

// ---------------------------------------------------------------------------
// Fixture that wires up MockPrivateInstanceAAMP so injection API calls can be
// observed.  Used to verify the new false-return contract of sendSegment().
// ---------------------------------------------------------------------------
class ElementaryProcessorSendSegmentTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mPrivateInstanceAAMP2 = new PrivateInstanceAAMP(gpGlobalConfig);
		g_mockPrivateInstanceAAMP = std::make_shared<MockPrivateInstanceAAMP>();
		// ElementaryProcessor constructor calls GetMediaFormatTypeEnum()
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetMediaFormatTypeEnum())
			.WillRepeatedly(Return(eMEDIAFORMAT_HLS_MP4));
		mElementaryProcessor = new ElementaryProcessor(mPrivateInstanceAAMP2);
	}

	void TearDown() override
	{
		delete mElementaryProcessor;
		mElementaryProcessor = nullptr;
		delete mPrivateInstanceAAMP2;
		mPrivateInstanceAAMP2 = nullptr;
		g_mockPrivateInstanceAAMP.reset();
	}

	ElementaryProcessor    *mElementaryProcessor{};
	PrivateInstanceAAMP    *mPrivateInstanceAAMP2{};
	MediaProcessor::process_fcn_t mProcessorFn{};
};

// sendSegment() must return false when the sink rejects the buffer
// (SendStreamCopy returns false on the HLS path).
TEST_F(ElementaryProcessorSendSegmentTest, sendSegmentReturnsFalse_WhenSinkRejectsBuffer)
{
	std::vector<uint8_t> buffer{0xAA, 0xBB};
	bool ptsError = false;

	// Unblock the PTS wait so sendSegment() reaches sendStream() synchronously.
	mElementaryProcessor->abortInjectionWait();

	// Sink signals failure.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamCopy(_, _, _, _, _))
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _, _)).Times(0);

	bool result = mElementaryProcessor->sendSegment(std::move(buffer), 0.0, 2.0, 0.0, false, false, mProcessorFn, ptsError);

	EXPECT_FALSE(result);
}
