/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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
#include "MockAampConfig.h"
#include "MockAampScheduler.h"
#include "MockPrivateInstanceAAMP.h"
#include "main_aamp.h"
#include "MockStreamAbstractionAAMP.h"
#include "AampcliHarvester.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>

#include <AampConfig.cpp>
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::AtLeast;
using namespace std;

static int create_folder(std::string folderName){

    std::string command = "mkdir -p " ;
    command += folderName;

    int result = system(command.c_str());

    if (result == 0) {
        std::cout << "successfully created folder:" << folderName << std::endl;
    } else {
        std::cout << "Failed to create folder:" << folderName << std::endl;
    }

    return 0;

}

static int remove_folder(std::string folderName){

    // Removing the contents of the folder recursively using the rm command
    std::string command = "rm -rf " ;
    command += folderName;

    int result = system(command.c_str());

    if (result == 0) {
        std::cout << "successfully removed folder:" << folderName << std::endl;
    } else {
        std::cout << "Failed to remove folder:" << folderName << std::endl;
    }

    return 0;

}

class HarversterTests : public ::testing::Test
{
protected:
    Harvester *harvester = nullptr ; 
	PlayerInstanceAAMP *p_aamp = nullptr;

    void SetUp() override 
    {
		harvester = new Harvester();
		p_aamp = new PlayerInstanceAAMP();

		g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(nullptr, p_aamp->aamp);    
    	p_aamp->aamp->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

    }
    
    void TearDown() override 
    {
		delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP = nullptr;

        delete p_aamp;
        p_aamp = nullptr;

        delete harvester;
        harvester = nullptr;

    }
};

static void process_harvester_report(std::string &filename, vector<string> &master_cmd, vector<string> &slave_cmds)
{
    std::map<std::string, std::string> taggedURLList;
	if (filename != "")
	{
		std::ifstream report(filename);
		if (report.good())
		{
			std::string line;
			std::string tag;
			std::string url;
			while (std::getline(report, line))
			{
                if (line.find("Master cmd") != std::string::npos) {
                    master_cmd.push_back(line);
                    continue;
                }

                if (line.find("Slave cmd") != std::string::npos) {
                    slave_cmds.push_back(line);
                    continue;
                }

			}
			report.close();
		}
	}
}

// aamp.cfg harvestConfig=255+harvestDuration=20
TEST_F(HarversterTests, aampcfg_harvestConfig_harvestDuration)
{
	g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();

    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestConfig, 255);
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestDuration, 20);
    
    std::string json = "[{\n\t\t\"name\":\t\"5\",\n\t\t\"language\":\t\"ger\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"german\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"5\",\n\t\t\"language\":\t\"ger\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"commentary\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"6\",\n\t\t\"language\":\t\"eng\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"english\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"6\",\n\t\t\"language\":\t\"eng\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"commentary\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"7\",\n\t\t\"language\":\t\"spa\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"spanish\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"7\",\n\t\t\"language\":\t\"spa\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"commentary\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"0\",\n\t\t\"language\":\t\"fra\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"french\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"0\",\n\t\t\"language\":\t\"fra\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"commentary\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"8\",\n\t\t\"language\":\t\"pol\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"polish\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}, {\n\t\t\"name\":\t\"8\",\n\t\t\"language\":\t\"pol\",\n\t\t\"codec\":\t\"mp4a.40.2\",\n\t\t\"rendition\":\t\"commentary\",\n\t\t\"bandwidth\":\t288000,\n\t\t\"Type\":\t\"audio\",\n\t\t\"availability\":\ttrue\n\t}]";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetAvailableAudioTracks(true)).WillOnce(Return(json));
    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));

    std::string folderName = std::to_string(1);
    create_folder(folderName);
    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool configCorrect = false;

    for (auto& cmd : master_cmd) {
        if (cmd.find("harvestConfig=16") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }
    }
    EXPECT_EQ(configCorrect, true);

    EXPECT_GT(slave_cmds.size(), 0);
	// found slave cmd for iFrame       
	if (slave_cmds[0].find("harvestConfig=65") != std::string::npos) {
		configCorrect = true;
	}else {
		configCorrect = false;
	}
    EXPECT_EQ(configCorrect, true);

    for (auto& cmd : slave_cmds) {

		//check duration
		if (cmd.find("harvestDuration=20") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }

		// found slave cmd for video
        if (cmd.find("defaultBitrate") != std::string::npos && 
		    cmd.find("preferredAudioLanguage") == std::string::npos) {
			
			if (cmd.find("harvestConfig=161") != std::string::npos) {
            	configCorrect = true;
			}else {
				configCorrect = false;
            	break;
			}
			continue;
        }

		// found slave cmd for audeo		
        if (cmd.find("preferredAudioLanguage") != std::string::npos) {
			// this cmd is for audeo
			if (cmd.find("harvestConfig=2") != std::string::npos) {
            	configCorrect = true;
			}else {
				configCorrect = false;
            	break;
			}
			continue;
        }
    }

    EXPECT_EQ(configCorrect, true);

	delete g_mockPrivateInstanceAAMP;
	g_mockPrivateInstanceAAMP = nullptr;

    remove_folder(folderName);
}

// cmd parameters overrides aamp.cfg harvestConfig=255+harvestDuration=20
TEST_F(HarversterTests, cmd_param_override_aampcfg_harvestConfig_harvestDuration)
{
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestConfig, 255);
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestDuration, 20);

    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
    
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));

    std::string folderName = std::to_string(2);
    create_folder(folderName);

    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd harvestConfig=27 harvestDuration=15 noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool configCorrect = false;

    for (auto& cmd : master_cmd) {
        if (cmd.find("harvestConfig=16") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }
    }
    EXPECT_EQ(configCorrect, true);

    EXPECT_GT(slave_cmds.size(), 0);
	// should not find iFame slave cmd      
	if (slave_cmds[0].find("harvestConfig=65") == std::string::npos) {
		configCorrect = true;
	}else {
		configCorrect = false;
	}
    EXPECT_EQ(configCorrect, true);

    for (auto& cmd : slave_cmds) {

		//check duration should be set by cmd parameter instead of aamp.cfg
		if (cmd.find("harvestDuration=15") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }

		// found slave cmd for video
        if (cmd.find("defaultBitrate") != std::string::npos && 
		    cmd.find("preferredAudioLanguage") == std::string::npos) {
			
			if (cmd.find("harvestConfig=1") != std::string::npos) {
            	configCorrect = true;
			}else {
				configCorrect = false;
            	break;
			}
			continue;
        }

		// found slave cmd for audeo		
        if (cmd.find("preferredAudioLanguage") != std::string::npos) {
			// this cmd is for video
			if (cmd.find("harvestConfig=2") != std::string::npos) {
            	configCorrect = true;
			}else {
				configCorrect = false;
            	break;
			}
			continue;
        }
    }

    EXPECT_EQ(configCorrect, true);

    remove_folder(folderName);

}


// harvestCountLimit
TEST_F(HarversterTests, harvestCountLimit)
{
    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
    
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));

    std::string folderName = std::to_string(3);
    create_folder(folderName);

    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd harvestCountLimit=5 noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool configCorrect = false;
    for (auto& cmd : slave_cmds) {
        if (cmd.find("harvestCountLimit=5") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }
    }
    EXPECT_EQ(configCorrect, true);
    remove_folder(folderName);

}

// aamp.cfg harvestConfig=3
TEST_F(HarversterTests, harvestConfig)
{

    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestConfig, 3);

    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
    
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));

    std::string folderName = std::to_string(4);
    create_folder(folderName);

    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd harvestCountLimit=2 noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool harvestConfigCorrect = false;
    for (auto& cmd : slave_cmds) {
        if (cmd.find("harvestConfig=1") != std::string::npos) {
            harvestConfigCorrect = true;
        }else{
            harvestConfigCorrect = false;
            break;
        }
    }
    EXPECT_EQ(harvestConfigCorrect, true);
    remove_folder(folderName);

}

// aamp.cfg suppressDecode
TEST_F(HarversterTests, suppressDecode)
{

    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestConfig, 3);
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_SuppressDecode, true);

    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
    
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));

    std::string folderName = std::to_string(5);
    create_folder(folderName);

    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd harvestCountLimit=2 noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool masterConfigCorrect = false;
    for (auto& cmd : master_cmd) {
        if (cmd.find("suppressDecode=1") != std::string::npos) {
            masterConfigCorrect = true;
        }else{
            masterConfigCorrect = false;
            break;
        }
    }
    EXPECT_EQ(masterConfigCorrect, true);
    remove_folder(folderName);

}

// aamp.cfg useTCPServerSink
TEST_F(HarversterTests, useTCPServerSink)
{
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestConfig, 3);
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_useTCPServerSink, true);

    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
    
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));

    std::string folderName = std::to_string(6);
    create_folder(folderName);

    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://lin013-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SCINCOH_HD_SU_SKYUK_4019_0_6771210893185225163.mpd harvestDuration=10 noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool configCorrect = false;
    for (auto& cmd : slave_cmds) {
        if (cmd.find("useTCPServerSink=true") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }
    }
    EXPECT_EQ(configCorrect, true);
    remove_folder(folderName);

}

// aamp.cfg harvestDuration=60
TEST_F(HarversterTests, harvestDuration)
{
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestDuration, 60);

    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
    
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));

    std::string folderName = std::to_string(7);
    create_folder(folderName);

    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool configCorrect = false;
    for (auto& cmd : slave_cmds) {
        if (cmd.find("harvestDuration=60") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }
    }
    EXPECT_EQ(configCorrect, true);
    remove_folder(folderName);

}

// aamp.cfg harvestCountLimit=5
TEST_F(HarversterTests, aampcfg_harvestCountLimit)
{
    p_aamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestCountLimit, 5);

    std::vector<BitsPerSecond> rates = {80000, 120000, 480000};
    
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetVideoBitrates()).WillOnce(Return(rates));
    
    std::string folderName = std::to_string(8);
    create_folder(folderName);

    std::string cmd_str = "harvest harvestMode=Master harvestUrl=https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd noHarvest=true harvestPath=" + folderName;
    harvester->execute(cmd_str.c_str(), p_aamp);
  
    std::string report_fname = folderName + "/HarvestReport.txt";
    
    vector<string> master_cmd, slave_cmds;
    process_harvester_report(report_fname, master_cmd, slave_cmds);

    bool configCorrect = false;
    for (auto& cmd : slave_cmds) {
        if (cmd.find("harvestCountLimit=5") != std::string::npos) {
            configCorrect = true;
        }else{
            configCorrect = false;
            break;
        }
    }
    EXPECT_EQ(configCorrect, true);
    remove_folder(folderName);

}