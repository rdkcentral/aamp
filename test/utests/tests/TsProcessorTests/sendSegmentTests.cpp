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

#include "AampMediaType.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "tsprocessor.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
using ::testing::_;
using ::testing::Return;
AampConfig *gpGlobalConfig{nullptr};

const int tsPacketLength = 188;

class sendSegmentTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};

    class TestTSProcessor : public TSProcessor
    {
    public:
        friend class sendSegmentTests;
        TestTSProcessor(class PrivateInstanceAAMP *mPrivateInstanceAAMP, StreamOperation streamOperation)
        : TSProcessor(mPrivateInstanceAAMP, eStreamOp_DEMUX_AUDIO, nullptr)
        {
        }

        void CallsendQueuedSegment(long long basepts, double updatedStartPosition)
        {
            sendQueuedSegment(basepts,updatedStartPosition);
        }

        void CallsetBasePTS(double position, long long pts)
        {
            setBasePTS(position, pts);
        }

        void CallsetPlayMode( PlayMode mode )
        {
            setPlayMode(mode);
        }

        void CallprocessPMTSection( unsigned char* section, int sectionLength )
        {
            processPMTSection(section, sectionLength);
        }

        int CallinsertPatPmt( unsigned char *buffer, bool trick, int bufferSize )
        {
            int insertResult = insertPatPmt(buffer, trick, bufferSize);
            return insertResult;
        }

        void CallinsertPCR( unsigned char *packet, int pid )
        {
            insertPCR(packet, pid);
        }

        bool CallprocessStartCode( unsigned char *buffer, bool& keepScanning, int length, int base )
        {
            bool process_Result = processStartCode(buffer, keepScanning, length, base);
            return process_Result;
        }

        void CallcheckIfInterlaced( unsigned char *packet, int length )
        {
            checkIfInterlaced(packet, length);
        }

        bool CallreadTimeStamp( unsigned char *p, long long &value )
        {
            bool TimeStampResult = readTimeStamp(p, value);
            return TimeStampResult;
        }

        void CallwriteTimeStamp( unsigned char *p, int prefix, long long TS )
        {
            writeTimeStamp(p, prefix, TS);
        }

        long long CallreadPCR( unsigned char *p )
        {
            long long readPCRValue = readPCR(p);
            return readPCRValue;
        }

        void CallwritePCR( unsigned char *p, long long PCR, bool clearExtension )
        {
            writePCR(p, PCR, clearExtension);
        }

        bool CallprocessSeqParameterSet( unsigned char *p, int length )
        {
            bool process_Result = processSeqParameterSet(p, length);
            return process_Result;
        }

        void CallprocessPictureParameterSet( unsigned char *p, int length )
        {
            processPictureParameterSet(p, length);
        }

        void CallprocessScalingList( unsigned char *& p, int& mask, int size )
        {
            processScalingList(p, mask, size);
        }

        unsigned int CallgetBits( unsigned char *& p, int& mask, int bitCount )
        {
            unsigned int bitValue = getBits(p, mask, bitCount);
            return bitValue;
        }

        void CallputBits( unsigned char *& p, int& mask, int bitCount, unsigned int value )
        {
            putBits(p, mask, bitCount, value);
        }

        unsigned int CallgetUExpGolomb( unsigned char *& p, int& mask )
        {
            unsigned int UExpValue = getUExpGolomb(p, mask);
            return UExpValue;
        }

        int CallgetSExpGolomb( unsigned char *& p, int& mask )
        {
            int SExpValue = getSExpGolomb(p,mask);
            return SExpValue;
        }

        void CallupdatePATPMT()
        {
            updatePATPMT();
        }

        bool CallprocessBuffer(unsigned char *buffer, int size, bool &insPatPmt, bool discontinuity_pending)
        {
            bool BufferResult = processBuffer(buffer, size, insPatPmt, discontinuity_pending);
            return BufferResult;
        }

        long long CallgetCurrentTime()
        {
            long long currentTime = getCurrentTime();
            return currentTime;
        }

        bool Callthrottle()
        {
            bool throttleValue = throttle();
            return throttleValue;
        }

        void CallsendDiscontinuity(double position)
        {
            sendDiscontinuity(position);
        }

        void CallsetupThrottle(int segmentDurationMs)
        {
            setupThrottle(segmentDurationMs);
        }

        bool CalldemuxAndSend(const void *ptr, size_t len, double fTimestamp, double fDuration, bool discontinuous, MediaProcessor::process_fcn_t processor, TrackToDemux trackToDemux = ePC_Track_Both)
        {
            bool demuxValue = demuxAndSend(ptr, len, fTimestamp, fDuration, discontinuous, processor, trackToDemux = ePC_Track_Both);
            return demuxValue;
        }

        bool Callmsleep(long long throttleDiff)
        {
            bool sleepValue = msleep(throttleDiff);
            return sleepValue;
        }

        double getApparentFrameRate() const {
            return m_apparentFrameRate;
        }
    };

    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }
        g_mockAampConfig = new MockAampConfig();

        mTSProcessor = new TestTSProcessor(mPrivateInstanceAAMP, eStreamOp_DEMUX_AUDIO);

        g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
    }

    void TearDown() override
    {
        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;

        delete mTSProcessor;
        mTSProcessor = nullptr;

        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;
    }
    TestTSProcessor *mTSProcessor;
};

TEST_F(sendSegmentTests, CallsendQueuedSegmentTest)
{
    long long basepts = 0;
    double updatedStartPosition = 1.1;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPosition);
}

TEST_F(sendSegmentTests, CallsendQueuedSegmentTest1)
{
    long long basepts = LLONG_MAX;
    double updatedStartPosition = DBL_MAX;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPosition);
}

TEST_F(sendSegmentTests, CallsendQueuedSegmentTest2)
{
    long long basepts = LLONG_MIN;
    double updatedStartPosition = DBL_MIN;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPosition);
}

TEST_F(sendSegmentTests, CallsendQueuedSegmentTest4)
{
    long long basepts = 0;
    double updatedStartPosition = 0;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPosition);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest)
{
    double position = 1.2;
    long long pts = 10;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest1)
{
    double position = 0;
    long long pts = 0;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest2)
{
    double position = DBL_MIN;
    long long pts = LLONG_MAX;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest3)
{
    double position = DBL_MAX;
    long long pts = LLONG_MIN;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest4)
{
    double position = DBL_MAX;
    long long pts = LLONG_MAX;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest5)
{
    double position = DBL_MIN;
    long long pts = LLONG_MIN;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest6)
{
    double position = LLONG_MAX;
    long long pts = LLONG_MIN;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetPlayModeTest)
{
    PlayMode mode;
    mTSProcessor->CallsetPlayMode(mode);
}

TEST_F(sendSegmentTests, CallsetPlayModeTest1)
{
    PlayMode mode[5]={
        PlayMode_normal,
        PlayMode_retimestamp_IPB,
        PlayMode_retimestamp_IandP,
        PlayMode_retimestamp_Ionly,
        PlayMode_reverse_GOP};
    for (int i=0; i<5; i++){
        mTSProcessor->CallsetPlayMode(mode[i]);
    }
}

TEST_F(sendSegmentTests, CallinsertPatPmtTest)
{
    unsigned char bufferdata[5] = {'a', 'b', 'c','d','e'};
    unsigned char *buffer = bufferdata;
    bool trick = true;
    int bufferSize = 5;
    int insertResult = mTSProcessor->CallinsertPatPmt(buffer, trick, bufferSize);
}

TEST_F(sendSegmentTests, CallinsertPatPmtTest1)
{
    unsigned char bufferdata[4] = {'a', 'A', 'c', 'd'};
    unsigned char *buffer = bufferdata;
    bool trick = false;
    int bufferSize = 4;
    int insertResult = mTSProcessor->CallinsertPatPmt(buffer, trick, bufferSize);
}

TEST_F(sendSegmentTests, CallinsertPatPmtTest2)
{
    unsigned char bufferdata[6] = {'a', 'A', 'c','d','e','f'};
    unsigned char *buffer = bufferdata;
    bool trick = true;
    int bufferSize = 6;
    int insertResult = mTSProcessor->CallinsertPatPmt(buffer, trick, bufferSize);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest)
{
    unsigned char bufferData[5] = {'a', 'b', 'c','e','f'};
    unsigned char *buffer = bufferData;
    bool scanningvalue = true;
    int length = 5;
    int base = 10;
    bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, scanningvalue, length, base);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest1)
{
    unsigned char bufferData[7] = {'a', 'A', 'c','s','f','v','t'};
    unsigned char *buffer = bufferData;
    bool scanningvalue = false;
    bool &keepScanning = scanningvalue;
    int length = 7;
    int base = 10;
    bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, keepScanning, length, base);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest2)
{
    unsigned char bufferData[5] = {'a', 'A', 'c','w','r'};
    unsigned char *buffer = bufferData;
    bool scanningvalue = true;
    bool &keepScanning = scanningvalue;
    int length = 5;
    int base = 10;
    bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, keepScanning, length, base);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest3)
{
    unsigned char bufferData[7] = {'a', 'A', 'c','w','r','f','b'};
    unsigned char *buffer = bufferData;
    bool scanningvalue = true;
    bool &keepScanning = scanningvalue;
    int length = 7;
    int base = 10;
    bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, keepScanning, length, base);
}

TEST_F(sendSegmentTests, CallcheckIfInterlacedTest)
{
    unsigned char packetData[4] = {'a', 'b', 'c' ,'e'};
    unsigned char *packet = packetData;
    int length = 4;
    mTSProcessor->CallcheckIfInterlaced(packet, length);
}

TEST_F(sendSegmentTests, CallcheckIfInterlacedTest1)
{
    unsigned char packetData[7] = {'a', 'B', 'c','q','e','r','t'};
    unsigned char *packet = packetData;
    int length = 7;
    mTSProcessor->CallcheckIfInterlaced(packet, length);
}

TEST_F(sendSegmentTests, CallcheckIfInterlacedTest2)
{
    unsigned char packetData[7] = {'a', 'b', 'c','y','t','c','r'};
    unsigned char *packet = packetData;
    int length = 7;
    mTSProcessor->CallcheckIfInterlaced(packet, length);
}

TEST_F(sendSegmentTests, CallreadTimeStampTest)
{
    unsigned char pData[5] = {'a', 'b', 'c','s','e'};
    unsigned char *p = pData;
    long long value = 5;
    bool ResultTimeStamp = mTSProcessor->CallreadTimeStamp(p, value);
}

TEST_F(sendSegmentTests, CallreadTimeStampTest1)
{
    unsigned char pData[7] = {'a', 'B', 'c','q','w','e','r'};
    unsigned char *p = pData;
    long long value = 6;
    bool ResultTimeStamp = mTSProcessor->CallreadTimeStamp(p, value);
}

TEST_F(sendSegmentTests, CallreadTimeStampTest2)
{
    unsigned char pData[5] = {'a', 'B', 'c','d','e'};
    unsigned char *p = pData;
    long long value = 10;
    bool ResultTimeStamp = mTSProcessor->CallreadTimeStamp(p, value);
}

TEST_F(sendSegmentTests, CallwriteTimeStampTest)
{
    unsigned char pData[64];
    memset(pData, 0, sizeof(pData));
    unsigned char* p = &pData[0];
    int prefix = 0;
    long long TS = 12;
    mTSProcessor->CallwriteTimeStamp(p, prefix, TS);
}

TEST_F(sendSegmentTests, CallwriteTimeStampTest1)

{
    unsigned char pData[64];
    memset(pData, 0, sizeof(pData));
    unsigned char* p = &pData[0];
    int prefix = 5;
    long long TS = 10;
    mTSProcessor->CallwriteTimeStamp(p, prefix, TS);
}

TEST_F(sendSegmentTests, CallwriteTimeStampTest2)
{
    unsigned char pData[64];
    memset(pData, 0, sizeof(pData));
    unsigned char* p = &pData[0];
    int prefix = 2;
    long long TS = 25;
    mTSProcessor->CallwriteTimeStamp(p, prefix, TS);
}

TEST_F(sendSegmentTests, CallreadPCRTest)
{
    unsigned char pData[5] = {'a', 'b', 'c','d','e'};
    unsigned char *p = pData;
    long long PCRResult = mTSProcessor->CallreadPCR(p);
}

TEST_F(sendSegmentTests, CallreadPCRTest1)
{
    unsigned char pData[7] = {'a', 'B', 'c','e','q','x','f'};
    unsigned char *p = pData;
    long long PCRResult = mTSProcessor->CallreadPCR(p);
}

TEST_F(sendSegmentTests, CallwritePCRTest)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f'};
    unsigned char *p = pData;
    long long PCR = 3;
    bool clearExtension = true;
    mTSProcessor->CallwritePCR(p, PCR, clearExtension);
}

TEST_F(sendSegmentTests, CallwritePCRTest1)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f'};
    unsigned char *p = pData;
    long long PCR = 12;
    bool clearExtension = false;
    mTSProcessor->CallwritePCR(p, PCR, clearExtension);
}

TEST_F(sendSegmentTests, CallwritePCRTest2)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f'};
    unsigned char *p = pData;
    long long PCR = 10;
    bool clearExtension = true;
    mTSProcessor->CallwritePCR(p, PCR, clearExtension);
}

TEST_F(sendSegmentTests, CallwritePCRTest3)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f'};
    unsigned char *p = pData;
    long long PCR = 0;
    bool clearExtension = true;
    mTSProcessor->CallwritePCR(p, PCR, clearExtension);
}

TEST_F(sendSegmentTests, CallprocessSeqParameterSetTest)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int length = 3;
    bool process_Result = mTSProcessor->CallprocessSeqParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessSeqParameterSetTest1)
{
    unsigned char pData[7] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int length = 7;
    bool process_Result = mTSProcessor->CallprocessSeqParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessSeqParameterSetTest2)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f'};
    unsigned char *p = pData;
    int length = 6;
    bool process_Result = mTSProcessor->CallprocessSeqParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessSeqParameterSetTest3)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int length = 7;
    bool process_Result = mTSProcessor->CallprocessSeqParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest)
{
    unsigned char pData[] = {'a','b'};
    unsigned char *p = pData;
    int length = 2;
    mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest1)
{
    unsigned char pData[] = {'a', 'b'};
    unsigned char *p = pData;
    int length = 2;
    mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest2)
{
    unsigned char pData[] = {'a', 'b', 'c'};
    unsigned char *p = pData;
    int length = 3;
    mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest3)
{
    //Assigning 0 as input for length to check it's function
    unsigned char pData[] = {'a', 'b', 'c','d'};
    unsigned char *p = pData;
    int length = 4;
    mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 12;
    int& mask = maskData;
    int size = 7;
    mTSProcessor->CallprocessScalingList(p, mask, size);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest1)
{
    unsigned char pData[] = {'a', 'b', 'c'};
    unsigned char *p = pData;
    int maskData = 8;
    int& mask = maskData;
    int size = 3;
   mTSProcessor->CallprocessScalingList(p, mask, size);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest2)
{
    unsigned char pData[] = {'a', 'b', 'c','d'};
    unsigned char *p = pData;
    int maskData = 25;
    int& mask = maskData;
    int size = 4;
    mTSProcessor->CallprocessScalingList(p, mask, size);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest3)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e'};
    unsigned char *p = pData;
    int maskData = 13;
    int& mask = maskData;
    int size = 5;
    mTSProcessor->CallprocessScalingList(p, mask, size);
}

TEST_F(sendSegmentTests, CallgetBitsTest)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 3;
    int& mask = maskData;
    int bitCount = 2;
    unsigned int bitValue = mTSProcessor->CallgetBits(p, mask, bitCount);
}

TEST_F(sendSegmentTests, CallgetBitsTest1)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e'};
    unsigned char *p = pData;
    int maskData = 11;
    int& mask = maskData;
    int bitCount = 12;
    unsigned int bitValue = mTSProcessor->CallgetBits(p, mask, bitCount);
}

TEST_F(sendSegmentTests, CallgetBitsTest2)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 25;
    int& mask = maskData;
    int bitCount = 10;
    unsigned int bitValue = mTSProcessor->CallgetBits(p, mask, bitCount);
}

TEST_F(sendSegmentTests, CallgetBitsTest3)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 99;
    int& mask = maskData;
    int bitCount = 7;
    unsigned int bitValue = mTSProcessor->CallgetBits(p, mask, bitCount);
}

TEST_F(sendSegmentTests, CallputBitsTest)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 3;
    int& mask = maskData;
    int bitCount = 2;
    unsigned int value = 1;
    mTSProcessor->CallputBits(p, mask, bitCount, value);
}

TEST_F(sendSegmentTests, CallputBitsTest1)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 0;
    int& mask = maskData;
    int bitCount = 0;
    unsigned int value = 0;
    mTSProcessor->CallputBits(p, mask, bitCount, value);
}

TEST_F(sendSegmentTests, CallputBitsTest3)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 99;
    int& mask = maskData;
    int bitCount = 24;
    unsigned int value = 0;
    mTSProcessor->CallputBits(p, mask, bitCount, value);
}

TEST_F(sendSegmentTests, CallgetUExpGolombTest)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 3;
    int& mask = maskData;
    unsigned int UExpValue = mTSProcessor->CallgetUExpGolomb(p, mask);
}

TEST_F(sendSegmentTests, CallgetUExpGolombTest1)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','G'};
    unsigned char *p = pData;
    int maskData = 125;
    int& mask = maskData;
    unsigned int UExpValue = mTSProcessor->CallgetUExpGolomb(p, mask);
}

TEST_F(sendSegmentTests, CallgetUExpGolombTest2)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','G'};
    unsigned char *p = pData;
    int maskData = 23;
    int& mask = maskData;
    unsigned int UExpValue = mTSProcessor->CallgetUExpGolomb(p, mask);
}

TEST_F(sendSegmentTests, CallgetUExpGolombTest3)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','G'};
    unsigned char *p = pData;
    int maskData = 0;
    int& mask = maskData;
    unsigned int UExpValue = mTSProcessor->CallgetUExpGolomb(p, mask);
}

TEST_F(sendSegmentTests, CallgetSExpGolombTest)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
    unsigned char *p = pData;
    int maskData = 3;
    int& mask = maskData;
    int SExpValue = mTSProcessor->CallgetSExpGolomb(p,mask);
}

TEST_F(sendSegmentTests, CallgetSExpGolombTest1)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','G'};
    unsigned char *p = pData;
    int maskData = INT_MIN;
    int& mask = maskData;
    int SExpValue = mTSProcessor->CallgetSExpGolomb(p,mask);
}

TEST_F(sendSegmentTests, CallgetSExpGolombTest2)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','G'};
    unsigned char *p = pData;
    int maskData = 2345;
    int& mask = maskData;
    int SExpValue = mTSProcessor->CallgetSExpGolomb(p,mask);
}

TEST_F(sendSegmentTests, CallgetSExpGolombTest3)
{
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','G'};
    unsigned char *p = pData;
    int maskData = 0;
    int& mask = maskData;
    int SExpValue = mTSProcessor->CallgetSExpGolomb(p,mask);
}

TEST_F(sendSegmentTests, CallupdatePATPMTTest)
{
    mTSProcessor->CallupdatePATPMT();
}

TEST_F(sendSegmentTests, CallgetCurrentTimeTest)
{
    long long currentTime = mTSProcessor->CallgetCurrentTime();
}

TEST_F(sendSegmentTests, CallthrottleTest)
{
    bool throttleValue = mTSProcessor->Callthrottle();
}

TEST_F(sendSegmentTests, CallsendDiscontinuityTest)
{
    double position = 2.1;
    mTSProcessor->CallsendDiscontinuity(position);
}

TEST_F(sendSegmentTests, CallsendDiscontinuityTest1)
{
    double position = DBL_MAX;
    mTSProcessor->CallsendDiscontinuity(position);
}

TEST_F(sendSegmentTests, CallsendDiscontinuityTest2)
{
    double position = DBL_MIN;
    mTSProcessor->CallsendDiscontinuity(position);
}

TEST_F(sendSegmentTests, CallsendDiscontinuityTest3)
{
    double position = 0;
    mTSProcessor->CallsendDiscontinuity(position);
}

TEST_F(sendSegmentTests, CallsetupThrottleTest)
{
    int segmentDurationMs = 2;
    mTSProcessor->CallsetupThrottle(segmentDurationMs);
}

TEST_F(sendSegmentTests, CallsetupThrottleTest1)
{
    int segmentDurationMs = INT_MIN;
    mTSProcessor->CallsetupThrottle(segmentDurationMs);
}

TEST_F(sendSegmentTests, CallsetupThrottleTest2)
{
    int segmentDurationMs = INT_MAX;
    mTSProcessor->CallsetupThrottle(segmentDurationMs);
}

TEST_F(sendSegmentTests, CallsetupThrottleTest3)
{
    int segmentDurationMs = 0;
    mTSProcessor->CallsetupThrottle(segmentDurationMs);
}

TEST_F(sendSegmentTests, CalldemuxAndSendTest)
{
    const void *ptr;
    size_t len = 2;
    double fTimestamp = 1.2;
    double fDuration = 1.1;
    bool discontinuous = true;
    MediaProcessor::process_fcn_t processor;
    TrackToDemux trackToDemux = ePC_Track_Both;
    bool demuxValue = mTSProcessor->CalldemuxAndSend(ptr, len, fTimestamp, fDuration, discontinuous, processor, trackToDemux = ePC_Track_Both);
}

TEST_F(sendSegmentTests, CallmsleepTest)
{
    long long throttleDiff = 3;
    bool sleepValue = mTSProcessor->Callmsleep(throttleDiff);
}

TEST_F(sendSegmentTests, SetAudio1)
{
    std::string id = "group123";
    mTSProcessor->SetAudioGroupId(id);
}

TEST_F(sendSegmentTests, SetAudio2)
{
    std::string id = "   CDCACVDC    ";
    mTSProcessor->SetAudioGroupId(id);
}

TEST_F(sendSegmentTests, FlushTest)
{
     mTSProcessor->flush();
}

TEST_F(sendSegmentTests, GetLanguageCodeTest)
{
    std::string lang = "fr";
    mTSProcessor->GetLanguageCode(lang);
    ASSERT_EQ(lang, "fr");
}

TEST_F(sendSegmentTests, SetThrottleEnableTest)
{
    mTSProcessor->setThrottleEnable(true);
    mTSProcessor->setThrottleEnable(false);
}


TEST_F(sendSegmentTests, setFrameRateForTMTests)
{
    double rate;
    mTSProcessor->setFrameRateForTM(20);
    rate = mTSProcessor->getApparentFrameRate();
    EXPECT_EQ(rate,20);

    mTSProcessor->setFrameRateForTM(-12);
    rate = mTSProcessor->getApparentFrameRate();
    EXPECT_NE(rate,-12);

}

TEST_F(sendSegmentTests, ResetTest)
{
    mTSProcessor->reset();
}

TEST_F(sendSegmentTests, SelectAudioIndexToPlay_NoAudioComponents)
{
    int selectedTrack = mTSProcessor->SelectAudioIndexToPlay();
    ASSERT_EQ(selectedTrack, -1);
}

TEST_F(sendSegmentTests, ChangeMuxedAudioTrackTest)
{
    mTSProcessor->ChangeMuxedAudioTrack(UCHAR_MAX);
}

TEST_F(sendSegmentTests, SetApplyOffsetFlagTrue)
{
    mTSProcessor->setApplyOffsetFlag(true);
}

TEST_F(sendSegmentTests, SendSegmentTest)
{
    size_t size = 100;
    char segment[100];
    AampGrowableBuffer buf("ts-processor-buffer-send-test");
    buf.AppendBytes(segment,size);
    double position = 0.0;
    double duration = 10.0;
	double offset = 0.0;

	bool discontinuous = false;
	bool init = false;
    bool ptsError = true;
    bool result;
    result = mTSProcessor->sendSegment(&buf, position, duration, offset, discontinuous,init, nullptr, ptsError);
    ASSERT_FALSE(result);
    buf.Free();
}

TEST_F(sendSegmentTests, SetApplyOffsetFlagFalse)
{
    mTSProcessor->setApplyOffsetFlag(false);
}

TEST_F(sendSegmentTests, esMP3test)
{
    unsigned char segment[tsPacketLength * 2] = {};
    AampGrowableBuffer buffer("tsProcessor PAT/PMT test");
    double position = 0;
    double duration = 2.43;
	double offset = 0.0;
	bool discontinuous = false;
	bool init = false;
    bool ptsError = false;

    buffer.AppendBytes(segment, sizeof(segment));
    mTSProcessor->sendSegment(&buffer, position, duration, offset, discontinuous, init,
        [this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf) {
            mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_s, info.dts_s, info.duration);
        },
        ptsError);

    buffer.Free();
}

TEST_F(sendSegmentTests, SetRateTest)
{
    double m_playRateNext;
    double rate = 1.5;
    PlayMode mode = PlayMode_normal;
    mTSProcessor->setRate(rate, mode);
}

TEST_F(sendSegmentTests, AbortTest)
{
    mTSProcessor->setThrottleEnable(false);
    mTSProcessor->setRate(2.22, PlayMode_reverse_GOP);
    mTSProcessor->abort();
}
