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
AampLogManager *mLogObj{nullptr};

const int tsPacketLength = 188;

class sendSegmentTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};

    class TestTSProcessor : public TSProcessor
    {
    public:
        // Making the test class a friend of the sendSegmentTests class
        friend class sendSegmentTests;
		//Constructor for Testclass
        TestTSProcessor(AampLogManager *mLogObj, class PrivateInstanceAAMP *mPrivateInstanceAAMP, StreamOperation streamOperation)
	    : TSProcessor(mLogObj, mPrivateInstanceAAMP, eStreamOp_DEMUX_AUDIO, nullptr)
        {
        }

        void CallsendQueuedSegment(long long basepts, double updatedStartPositon)
        {
            sendQueuedSegment(basepts,updatedStartPositon);
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

		void CallabortUnlocked()
		{
			abortUnlocked();
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

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		
		g_mockAampConfig = new MockAampConfig();

		mTSProcessor = new TestTSProcessor(mLogObj, mPrivateInstanceAAMP, eStreamOp_DEMUX_AUDIO);

		g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
	}
		
	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

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
    double updatedStartPositon = 1.1;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPositon);
}

TEST_F(sendSegmentTests, CallsendQueuedSegmentTest1)
{
	//Giving Max long long and Max double as test cases
    long long basepts = LLONG_MAX;
    double updatedStartPositon = DBL_MAX;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPositon);
}

TEST_F(sendSegmentTests, CallsendQueuedSegmentTest2)
{
	//Giving Min long long and Min double as test cases
    long long basepts = LLONG_MIN;
    double updatedStartPositon = DBL_MIN;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPositon);
}

TEST_F(sendSegmentTests, CallsendQueuedSegmentTest4)
{
	//Assigning 0 as the input for both parameters
    long long basepts = 0;
    double updatedStartPositon = 0;
    mTSProcessor->CallsendQueuedSegment(basepts, updatedStartPositon);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest)
{
    double position = 1.2;
	long long pts = 10;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest1)
{
	//Assigning 0 as the input for both parameters
    double position = 0;
	long long pts = 0;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest2)
{
	//Testing both the input parameters with Double Min and long long Max
    double position = DBL_MIN;
	long long pts = LLONG_MAX;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest3)
{
	//Testing both the input parameters with Double Max and long long Min
    double position = DBL_MAX;
	long long pts = LLONG_MIN;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest4)
{
	//Testing both the input parameters with Double Max and long long Max
    double position = DBL_MAX;
	long long pts = LLONG_MAX;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest5)
{
	//Testing both the input parameters with Double Min and long long Min
    double position = DBL_MIN;
	long long pts = LLONG_MIN;
    mTSProcessor->CallsetBasePTS(position, pts);
}

TEST_F(sendSegmentTests, CallsetBasePTSTest6)
{
	//Testing the double with long long as input
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
	//Checking the enum values by passing them to the setPlayMode function
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

TEST_F(sendSegmentTests, CallprocessPMTSectionTest)
{
    unsigned char* section;
	int sectionLength = 10;
    mTSProcessor->CallprocessPMTSection(section, sectionLength);
}

TEST_F(sendSegmentTests, CallprocessPMTSectionTest1)
{
	//Checking whether unsigned char pointer can save the initial position of a big array
    unsigned char* section = new unsigned char[1000];
	int sectionLength = 10;
    mTSProcessor->CallprocessPMTSection(section, sectionLength);
	delete[] section;
}

TEST_F(sendSegmentTests, CallprocessPMTSectionTest2)
{
	//Checking unsigned char pointer by passing both integers and Hexadecimal numbers
	unsigned char data[5] = {0xFF,6,7,0x10,0x01};
    unsigned char* section = data;
	int sectionLength = 10;
    mTSProcessor->CallprocessPMTSection(section, sectionLength);
}

TEST_F(sendSegmentTests, CallinsertPatPmtTest)
{
	unsigned char bufferdata[] = {'a', 'b', 'c'};
	unsigned char *buffer = bufferdata;
	bool trick = true;
	int bufferSize = 12;
	int insertResult = mTSProcessor->CallinsertPatPmt(buffer, trick, bufferSize);
}

TEST_F(sendSegmentTests, CallinsertPatPmtTest1)
{
	//Checking the buffer pointer by assinging it small data values of integers ASCII characters and hexadecimal numbers
	unsigned char bufferdata[] = {'a', 'A', 'c', 0x10, 0b111, 0};
	unsigned char *buffer = bufferdata;
	bool trick = false;
	//buffer size is set to max and bool value is changed to check
	int bufferSize = INT_MAX;
	int insertResult = mTSProcessor->CallinsertPatPmt(buffer, trick, bufferSize);
}

TEST_F(sendSegmentTests, CallinsertPatPmtTest2)
{
	//buffer size is set to min and bool value is changed to check
	unsigned char bufferdata[] = {'a', 'A', 'c', 0x10, 0b111, 0};
	unsigned char *buffer = bufferdata;
	bool trick = true;
	int bufferSize = INT_MIN;
	int insertResult = mTSProcessor->CallinsertPatPmt(buffer, trick, bufferSize);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest)
{
	unsigned char bufferData[] = {'a', 'b', 'c'};
	unsigned char *buffer = bufferData;
	bool scanningvalue = true;
	bool &keepScanning = scanningvalue;
	int length = 6;
	int base = 5;
	bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, keepScanning, length, base);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest1)
{
	//Checking unsinged char with an array of ASCII, hexadecimal, integers and binary data
	unsigned char bufferData[] = {'a', 'A', 'c', 0x10, 0b111, 0};
	unsigned char *buffer = bufferData;
	bool scanningvalue = false;
	bool &keepScanning = scanningvalue;
	//Checking the max limits of both length and base
	int length = INT_MIN;
	int base = INT_MAX;
	bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, keepScanning, length, base);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest2)
{
	//Checking the min limits of both length and base
	unsigned char bufferData[] = {'a', 'A', 'c', 0x10, 0b111, 0};
	unsigned char *buffer = bufferData;
	bool scanningvalue = true;
	bool &keepScanning = scanningvalue;
	int length = INT_MAX;
	int base = INT_MIN;
	bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, keepScanning, length, base);
}

TEST_F(sendSegmentTests, CallprocessStartCodeTest3)
{
	//Initialising length and base to 0 and checking the function
	unsigned char bufferData[] = {'a', 'A', 'c', 0x10, 0b111, 0};
	unsigned char *buffer = bufferData;
	bool scanningvalue = true;
	bool &keepScanning = scanningvalue;
	int length = 0;
	int base = 0;
	bool Process_Result = mTSProcessor->CallprocessStartCode(buffer, keepScanning, length, base);
}

TEST_F(sendSegmentTests, CallcheckIfInterlacedTest)
{
	unsigned char packetData[] = {'a', 'b', 'c'};
	unsigned char *packet = packetData;
	int length = 2;
	mTSProcessor->CallcheckIfInterlaced(packet, length);
}

TEST_F(sendSegmentTests, CallcheckIfInterlacedTest1)
{
	//Initialize length to 0 and assign various data members to packetdata array 
	unsigned char packetData[] = {'a', 'B', 'c', 0x10, 0b111, 0};
	unsigned char *packet = packetData;
	int length = 0;
	mTSProcessor->CallcheckIfInterlaced(packet, length);
}

TEST_F(sendSegmentTests, CallcheckIfInterlacedTest2)
{
	//Checking the max limits of length variable (Length can take negative values without returning errors)
	unsigned char packetData[] = {'a', 'b', 'c'};
	unsigned char *packet = packetData;
	int length = 100;
	mTSProcessor->CallcheckIfInterlaced(packet, length);
}

TEST_F(sendSegmentTests, CallreadTimeStampTest)
{
	unsigned char pData[] = {'a', 'b', 'c'};
	unsigned char *p = pData;
	long long valueData = 10;
	long long &value = valueData;
	bool ResultTimeStamp = mTSProcessor->CallreadTimeStamp(p, value);
}

TEST_F(sendSegmentTests, CallreadTimeStampTest1)
{
	//Testing the inputs and max limits of pData and valueData variables
	unsigned char pData[] = {'a', 'B', 'c', 0x10, 0b111, 0};
	unsigned char *p = pData;
	long long valueData = LLONG_MAX;
	long long &value = valueData;
	bool ResultTimeStamp = mTSProcessor->CallreadTimeStamp(p, value);
}

TEST_F(sendSegmentTests, CallreadTimeStampTest2)
{
	//Testing the inputs and min limits of pData and valueData variables
	unsigned char pData[] = {'a', 'B', 'c', 0x10, 0b111, 0};
	unsigned char *p = pData;
	long long valueData = LLONG_MIN;
	long long &value = valueData;
	bool ResultTimeStamp = mTSProcessor->CallreadTimeStamp(p, value);
}

TEST_F(sendSegmentTests, CallwriteTimeStampTest)
{
	unsigned char pData = '\0';
	unsigned char *p = &pData;
	int prefix = 0;
	long long TS = 0;
	mTSProcessor->CallwriteTimeStamp(p, prefix, TS);
}

TEST_F(sendSegmentTests, CallwriteTimeStampTest1)

{
	//Testing the upper limits of int and long long datatype variables and giving random values to pData
	unsigned char pData[] = {'a', 'B', 'c', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int prefix = INT_MAX;
	long long TS = LLONG_MAX;
	mTSProcessor->CallwriteTimeStamp(p, prefix, TS);
}

TEST_F(sendSegmentTests, CallwriteTimeStampTest2)
{
	//Testing the lower limits of int and long long datatype variables and giving random values to pData
	unsigned char pData[] = {'a', 'B', 'c', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int prefix = INT_MIN;
	long long TS = LLONG_MIN;
	mTSProcessor->CallwriteTimeStamp(p, prefix, TS);
}

TEST_F(sendSegmentTests, CallreadPCRTest)
{
	unsigned char pData[] = {'a', 'b', 'c'};
	unsigned char *p = pData;
    long long PCRResult = mTSProcessor->CallreadPCR(p);
}

TEST_F(sendSegmentTests, CallreadPCRTest1)
{
	//Give different types of values to the array to check
	unsigned char pData[] = {'a', 'B', 'c', 0x10, 0b111, 0};
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
	//Check max limits of long long and checking the boolean 
	unsigned char pData[] = {'a', 'b', 'c','d','e','f', 0x10, 0b111, 0};
	unsigned char *p = pData;
	long long PCR = LLONG_MAX;
	bool clearExtension = false;
	mTSProcessor->CallwritePCR(p, PCR, clearExtension);
}

TEST_F(sendSegmentTests, CallwritePCRTest2)
{
	//Check min limits of long long and checking the boolean
	unsigned char pData[] = {'a', 'b', 'c','d','e','f', 0x10, 0b111, 0};
	unsigned char *p = pData;
	long long PCR = LLONG_MIN;
	bool clearExtension = true;
	mTSProcessor->CallwritePCR(p, PCR, clearExtension);
}

TEST_F(sendSegmentTests, CallwritePCRTest3)
{
	//Check the long long with 0 as input
	unsigned char pData[] = {'a', 'b', 'c','d','e','f', 0x10, 0b111, 0};
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
	//Initialising zero for length to check the intput capabilities
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int length = 0;
	bool process_Result = mTSProcessor->CallprocessSeqParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessSeqParameterSetTest2)
{
	//Checking the max limits of length 
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int length = INT_MAX;
	bool process_Result = mTSProcessor->CallprocessSeqParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessSeqParameterSetTest3)
{
	//Checking the min limits of length
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int length = INT_MIN;
	bool process_Result = mTSProcessor->CallprocessSeqParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest)
{
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
	unsigned char *p = pData;
	int length = 3;
	mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest1)
{
	//Checking max limits of length
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int length = INT_MAX;
	mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest2)
{
	//Checking min limits of length
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int length = INT_MIN;
	mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessPictureParameterSetTest3)
{
	//Assigning 0 as input for length to check it's function
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int length = 0;
	mTSProcessor->CallprocessPictureParameterSet(p, length);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest)
{
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g'};
	unsigned char *p = pData;
	int maskData = 3;
	int& mask = maskData;
	int size = 2;
    mTSProcessor->CallprocessScalingList(p, mask, size);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest1)
{
	//checking the maximum limits of both maskData and size variables
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MAX;
	int& mask = maskData;
	int size = INT_MAX;
    mTSProcessor->CallprocessScalingList(p, mask, size);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest2)
{
	//Checking the minimum limits of both maskData and size variables
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MIN;
	int& mask = maskData;
	int size = INT_MIN;
    mTSProcessor->CallprocessScalingList(p, mask, size);
}

TEST_F(sendSegmentTests, CallprocessScalingListTest3)
{
	//Initialize 0 to check the variables
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = 0;
	int& mask = maskData;
	int size = 0;
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
	//Initialising 0 to maskData and bitcount to check their working
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = 0;
	int& mask = maskData;
	int bitCount = 0;
	unsigned int bitValue = mTSProcessor->CallgetBits(p, mask, bitCount);
}

TEST_F(sendSegmentTests, CallgetBitsTest2)
{
	//Testing the max limits of both maskData and bitCount variables
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MAX;
	int& mask = maskData;
	int bitCount = 1000;
	unsigned int bitValue = mTSProcessor->CallgetBits(p, mask, bitCount);
}

TEST_F(sendSegmentTests, CallgetBitsTest3)
{
	//Testing the min limits of both maskData and bitCount variables
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MIN;
	int& mask = maskData;
	int bitCount = INT_MIN;
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
	//Test all variables with 0 as the input
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = 0;
	int& mask = maskData;
	int bitCount = 0;
	unsigned int value = 0;
	mTSProcessor->CallputBits(p, mask, bitCount, value);
}

TEST_F(sendSegmentTests, CallputBitsTest2)
{
	//Testing the max limits of both maskData and bitCount variables
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MAX;
	int& mask = maskData;
	int bitCount = 99;
	unsigned int value = UINT_MAX;
	mTSProcessor->CallputBits(p, mask, bitCount, value);
}
TEST_F(sendSegmentTests, CallputBitsTest3)
{
	//Testing the min limits of both maskData and bitCount variables adn unsigned int value
    unsigned char pData[] = {'a', 'b', 'c','d','e','f','g', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MIN;
	int& mask = maskData;
	int bitCount = INT_MIN;
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
	//Testing the max limits of maskData
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','G', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MAX;
	int& mask = maskData;
	unsigned int UExpValue = mTSProcessor->CallgetUExpGolomb(p, mask);
}

TEST_F(sendSegmentTests, CallgetUExpGolombTest2)
{
	//Testing the minimum limit of maskData
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','G', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MIN;
	int& mask = maskData;
	unsigned int UExpValue = mTSProcessor->CallgetUExpGolomb(p, mask);
}

TEST_F(sendSegmentTests, CallgetUExpGolombTest3)
{
	//Testing the variable maskData by assigning 0 to it
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','G', 0x10, 0b111, 0};
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
	//Testing the minimum limit of maskData
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','G', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MIN;
	int& mask = maskData;
	int SExpValue = mTSProcessor->CallgetSExpGolomb(p,mask);
}

TEST_F(sendSegmentTests, CallgetSExpGolombTest2)
{
	//Testing the maximum limit of maskData
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','G', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = INT_MAX;
	int& mask = maskData;
	int SExpValue = mTSProcessor->CallgetSExpGolomb(p,mask);
}

TEST_F(sendSegmentTests, CallgetSExpGolombTest3)
{
	//Testing the variable maskData by assigning 0 to it
	unsigned char pData[] = {'a', 'b', 'c','d','e','f','G', 0x10, 0b111, 0};
	unsigned char *p = pData;
	int maskData = 0;
	int& mask = maskData;
	int SExpValue = mTSProcessor->CallgetSExpGolomb(p,mask);
}

TEST_F(sendSegmentTests, CallupdatePATPMTTest)
{
	mTSProcessor->CallupdatePATPMT();
}

TEST_F(sendSegmentTests, CallabortUnlockedTest)
{
	mTSProcessor->CallabortUnlocked();
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
	//Checking the max limit of the variable position
	double position = DBL_MAX;
 	mTSProcessor->CallsendDiscontinuity(position);
}

TEST_F(sendSegmentTests, CallsendDiscontinuityTest2)
{
	//Checking the min limit of the variable position
	double position = DBL_MIN;
 	mTSProcessor->CallsendDiscontinuity(position);
}

TEST_F(sendSegmentTests, CallsendDiscontinuityTest3)
{
	//Checking by assigning 0 to the position variable
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
	//Checking the min limit condition
	int segmentDurationMs = INT_MIN;	
	mTSProcessor->CallsetupThrottle(segmentDurationMs);
}

TEST_F(sendSegmentTests, CallsetupThrottleTest2)
{
	//Checking the max limit condition
	int segmentDurationMs = INT_MAX;	
	mTSProcessor->CallsetupThrottle(segmentDurationMs);
}

TEST_F(sendSegmentTests, CallsetupThrottleTest3)
{
	//Checking the variable with 0 as the input
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

TEST_F(sendSegmentTests, FilterAudioCodecWithAC3Enabled)
{
	bool ignoreProfile = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_MPEGTS);
	EXPECT_FALSE(ignoreProfile);
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

TEST_F(sendSegmentTests, FilterAudioCodecBasedOnConfig_ATMOSEnabled)
{
	bool result = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_ATMOS);
	ASSERT_FALSE(result);
}

TEST_F(sendSegmentTests, SetThrottleEnableTest)
{
	mTSProcessor->setThrottleEnable(true);
	mTSProcessor->setThrottleEnable(false);
}

TEST_F(sendSegmentTests, FilterAudioCodecBasedOnConfig_ATMOSEnabled11)
{
	bool result = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_AC3);
	ASSERT_FALSE(result);
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

TEST_F(sendSegmentTests, FilterAudioCodecBasedOnConfig_ATMOSEnabled12)
{
	bool result = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_EC3);
	ASSERT_FALSE(result);
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
	bool discontinuous = false;
	bool init = false;
	bool ptsError = true;
	bool result;
	result = mTSProcessor->sendSegment(&buf, position, duration, discontinuous,init, nullptr, ptsError);
	ASSERT_FALSE(result);
	buf.Free();
}

TEST_F(sendSegmentTests, SetApplyOffsetFlagFalse)
{
	mTSProcessor->setApplyOffsetFlag(false);
}

TEST_F(sendSegmentTests, esMP3test)
{
	/* Following segment definition contains PAT and PMT details wherein the audio is MP3 i.e. FORMAT_AUDIO_ES_MP3*/
	unsigned char segment[tsPacketLength*2] =
	{
		0x47,0x40,0x00,0x10,0x00,0x00,0xb0,0x0d,0x00,0x01,0xc1,0x00,0x00,0x00,0x01,0xef, \
		0xff,0x36,0x90,0xe2,0x3d,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x47,0x4f,0xff,0x10, \
		0x00,0x02,0xb0,0x3c,0x00,0x01,0xc1,0x00,0x00,0xe1,0x00,0xf0,0x11,0x25,0x0f,0xff, \
		0xff,0x49,0x44,0x33,0x20,0xff,0x49,0x44,0x33,0x20,0x00,0x1f,0x00,0x01,0x1b,0xe1, \
		0x00,0xf0,0x00,0x03,0xe1,0x01,0xf0,0x00,0x15,0xe1,0x02,0xf0,0x0f,0x26,0x0d,0xff, \
		0xff,0x49,0x44,0x33,0x20,0xff,0x49,0x44,0x33,0x20,0x00,0x0f,0x6f,0x2d,0xc7,0x0d, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
	};
	AampGrowableBuffer buffer("tsProcessor PAT/PMT test");
	double position = 0;
	double duration = 2.43;            /*Duration of the stream from which the segment data is extracted*/
	bool discontinuous = false;
	bool init = false;
	bool ptsError = false;
	buffer.AppendBytes(segment,tsPacketLength*2);

	/*A thread was waiting for base PTS, in order to get around it eAAMPConfig_AudioOnlyPlayback is configured true*/
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePublishingMuxedAudio)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamFormat(_,FORMAT_AUDIO_ES_MP3, _));

	mTSProcessor->sendSegment(&buffer, position, duration, discontinuous,init,
		[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
		{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_s, info.dts_s, info.duration);
		},
		ptsError
	);
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
