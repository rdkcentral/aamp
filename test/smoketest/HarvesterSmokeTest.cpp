/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
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
#include "gtest/gtest.h"
#include <unistd.h>
#include <string>
#include <fstream>

using namespace std;


bool fileExists(const std::string& filePath) {
    std::ifstream file(filePath);
    return file.good();
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

    return result;

}

int countFilesInFolder(const char* folderPath) {
    char command[256];
    sprintf(command, "find '%s' -maxdepth 10 -type f | wc -l", folderPath);

    FILE* pipe = popen(command, "r");
    if (pipe == NULL) {
        perror("popen failed");
        return -1;
    }

    char buffer[16];
    if (fgets(buffer, sizeof(buffer), pipe) == NULL) {
        perror("fgets failed");
        pclose(pipe);
        return -1;
    }

    pclose(pipe);

    // Remove trailing newline character from the buffer
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    return atoi(buffer);
}

// acquire number of files in the folder
int test_get_file_count(std::string folder) {
	
    int fileCount = countFilesInFolder(folder.c_str());

    printf("Number of files in the folder: %d\n", fileCount);

    return fileCount;
}

// to run the aamp-cli process with harvest cmd
void run_harvester(std::string& cmd){

	FILE *FileOpen;

  	char line[500]; 	 
	FileOpen = popen(cmd.c_str(), "w");                                                                            
                                                                     
  	sleep(10);
  	pclose(FileOpen);

}

//type that holds the necessary parameter to run harvester test
typedef struct {
	std::string harvestUrl;
	std::string harvestPath;
	std::string manifest_folder;
	int fileCount;		
}TestParam;

bool test_harvester(TestParam & param){	

	#ifdef AAMPCLI_PATH
	std::string my_str = AAMPCLI_PATH;
	std::string aampcli_path = my_str.substr(1, my_str.size()-2);
	std::cout << "aampcli path is: " << aampcli_path << std::endl;
	#endif

	#ifdef AAMP_ROOT
	std::string root_str = AAMP_ROOT;
	std::string aamp_root = root_str.substr(1, root_str.size()-2);
	std::cout << "aamp root is: " << aamp_root << std::endl;
	#endif

	// test failed if aamp-cli is not available
    if (!fileExists(aampcli_path)) {
         std::cout << "aamp-cli does not exist, harvester smoketest failed" << std::endl;
		return false;
    }

	std::string manifest_abs_path = param.harvestPath + param.manifest_folder;
	std::string set_ld = "LD_LIBRARY_PATH=" + aamp_root + "Linux/lib ";

	//first remove the harvested files folder if it already exist 
	remove_folder(param.harvestPath);

	std::string cmd = set_ld + aampcli_path + " harvest harvestMode=Master harvestCountLimit=3 harvestUrl=" + param.harvestUrl + " harvestPath=" + param.harvestPath;

	run_harvester(cmd);

	int fileCount = test_get_file_count(manifest_abs_path);

	//remove harvest folder
	remove_folder(param.harvestPath);
	
	if (fileCount == param.fileCount) {
		return true;
	}else {
		return false;
	}

}

TEST(harvesterSmokeTest, test_dash)
{
	TestParam param_dash = {
	// std::string harvestUrl
	"https://cpetestutility.stb.r53.xcal.tv/multilang/main.mpd",
	// std::string harvestPath
	"./dash/",
	// std::string manifest_folder
	"cpetestutility.stb.r53.xcal.tv/multilang/dash/",
	//expected file count
	30
	};

	bool testResult = test_harvester(param_dash);
	EXPECT_EQ(testResult,true);
}

TEST(harvesterSmokeTest, test_hls)
{
	TestParam param_hls = {
	// std::string harvestUrl
	"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8",
	// std::string harvestPath
	"./hls/",
	// std::string manifest_folder
	"cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/",
	//expected file count
	34
	};

	bool testResult = test_harvester(param_hls);
	EXPECT_EQ(testResult,true);
}