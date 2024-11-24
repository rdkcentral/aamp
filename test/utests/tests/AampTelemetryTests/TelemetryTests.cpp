/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include <cjson/cJSON.h>
#include "AampTelemetry2.hpp"
#define AAMP_TUNE_MANIFEST_REQ_FAILED 10

// using namespace testing;

class AampTelemetryTest : public ::testing::Test {
protected:

	void SetUp() override {
	}

	void TearDown() override {
	}

public:
	AAMPTelemetry2 telemetry;
};


TEST_F(AampTelemetryTest, Send_1)
{
	std::string markername = "VideoStartTime";
	std::string data = "IP_AAMP_TUNETIME\",\n\t\"ver\":\t5,\n\t\"bld\":\t\"6.3\",\n\t\"tbu\":\t1710328417733,\n\t\"mms\":\t3,\n\t\"mmt\":\t1172,\n\t\"mme\":\t0,\n\t\"vps\":\t0,\n\t\"vpt\":\t0,\n\t\"vpe\":\t0,\n\t\"aps\":\t0,\n\t\"apt\":\t0,\n\t\"ape\":\t0,\n\t\"vis\":\t1180,\n\t\"vit\":\t721,\n\t\"vie\":\t0,\n\t\"ais\":\t1903,\n\t\"ait\":\t715,\n\t\"aie\":\t0,\n\t\"vfs\":\t2860,\n\t\"vft\":\t228,\n\t\"vfe\":\t0,\n\t\"vfb\":\t2067007,\n\t\"afs\":\t2860,\n\t\"aft\":\t218,\n\t\"afe\":\t0,\n\t\"afb\":\t0,\n\t\"las\":\t0,\n\t\"lat\":\t0,\n\t\"dfe\":\t0,\n\t\"lpr\":\t0,\n\t\"lnw\":\t0,\n\t\"lps\":\t0,\n\t\"vdd\":\t0,\n\t\"add\":\t0,\n\t\"gps\":\t2923,\n\t\"gff\":\t0,\n\t\"cnt\":\t2,\n\t\"stt\":\t20,\n\t\"ftt\":\ttrue,\n\t\"pbm\":\t0,\n\t\"tpb\":\t0,\n\t\"dus\":\t734,\n\t\"ifw\":\t1,\n\t\"tat\":\t1,\n\t\"tst\":\t-1,\n\t\"frs\":\t\"\",\n\t\"app\":\t\"\",\n\t\"tsb\":\t0,\n\t\"tot\":\t11639698\n";
   	EXPECT_EQ(true,telemetry.send(markername,data.c_str()));
}

TEST_F(AampTelemetryTest, Send_2)
{
	std::string markername = "VideoStartFailure";
	std::map<std::string,int>intData;
	intData["err"] = AAMP_TUNE_MANIFEST_REQ_FAILED;
	intData["cat"] = 10;
	intData["cls"] = 0;
	intData["smc"] = 0;
	intData["sbc"] = 0;
	std::map<std::string,std::string>stringData;
	stringData["abc"] = "def";
	std::map<std::string,float>floatData;
	floatData["xyz"] = 2.2;
	EXPECT_EQ(true,telemetry.send(markername,intData,stringData,floatData));
}

TEST_F(AampTelemetryTest, Send_3)
{
	std::string markername = "VideoPlaybackFailure";
	std::map<std::string,int>intData;
	intData["err"] = AAMP_TUNE_MANIFEST_REQ_FAILED;
	intData["cat"] = 10;
	intData["cls"] = 0;
	intData["smc"] = 0;
	intData["sbc"] = 0;
	std::map<std::string,std::string>stringData;
	stringData["abc"] = "def";
	std::map<std::string,float>floatData;
	floatData["xyz"] = 2.2;
	EXPECT_EQ(true,telemetry.send(markername,intData,stringData,floatData));
}

TEST_F(AampTelemetryTest, Send_4)
{
	std::string markername = "VideoBufferingStart";
	std::map<std::string,int>intData;
	intData["buffer"]=1;
	std::map<std::string,std::string>stringData;
	stringData["abc"] = "def";
	std::map<std::string,float>floatData;
	floatData["xyz"]=2.2;
	EXPECT_EQ(true,telemetry.send(markername,intData,stringData,floatData));
}

TEST_F(AampTelemetryTest, Send_5)
{
	std::string markername = "VideoBufferingEnd";
	std::map<std::string,int>intData;
	intData["buffer"]=1;
	std::map<std::string,std::string>stringData;
	stringData["abc"] = "def";
	std::map<std::string,float>floatData;
	floatData["xyz"]=2.2;
	EXPECT_EQ(true,telemetry.send(markername,intData,stringData,floatData));
}

TEST_F(AampTelemetryTest, Send_6)
{
	std::string markername = "VideoBitrateChange";
	std::map<std::string, int> bitrateData;
	bitrateData["bit"] =1650064;
	bitrateData["wdh"] =640;
	bitrateData["hth"] =266;
	bitrateData["pcap"] =0;
	bitrateData["tw"] =0;
	bitrateData["th"] =0;
	bitrateData["sct"] =2;
	bitrateData["asw"] =0;
	bitrateData["ash"] =0;
	std::map<std::string, std::string> bitrateDesc;
	bitrateDesc["desc"] ="Reset to default bitrate due to tune";
	std::map<std::string, float> bitrateFloat;
	bitrateFloat["frt"] =25;
	bitrateFloat["pos"] = 102;
	EXPECT_EQ(true,telemetry.send(markername,bitrateData,bitrateDesc,bitrateFloat));
}
