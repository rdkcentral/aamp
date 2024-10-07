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

/**************************************
* @file ManifestSticher.cpp
* @brief Manifest Stiching Tool
**************************************/

#include <iostream>
#include <string>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>
#include <memory>
#include "MPDModel.h"
#include "MPDSegmenter.h"
#include <boost/filesystem.hpp>

#define MANIFEST_STICHER_OUTPUT_FILENAME "/mergedManifest.mpd"
// Compatibility in both ubuntu and MacOS
namespace fs = boost::filesystem;


void showHelp()
{
    std::cout << "Run binary with folder path having harvested manifests(.mpd files)" << std::endl << std::endl;
    std::cout << "Example, if files are dumped to /home/user/dump run following command." << std::endl;
    std::cout << "./ManifestSticher /home/user/dump" << std::endl << std::endl;
    std::cout << "Output filepath will be logged after execution" << std::endl;
}

bool compareFileNames(std::string& file1, std::string& file2)
{
    if (file1.size() == file2.size()) {
        return file1 < file2;
    }
    else {
        return file1.size() < file2.size();
    }
}

int main(int argc, char* argv[])
{
    std::string path;
    if(argc == 2)
    {
        path = argv[1];
    }
    else
    {
        std::cout << "Couldn't read path from argument," << std::endl;
        std::cout << "Please provide a path now:";
        std::cin >> path;
    }
    if(!path.empty())
    {
        if(path == "--help" || path == "-h" || path == "-?")
        {
            showHelp();
        }
        else
        {
            if(fs::exists(path.c_str()))
            {
                std::vector<std::string> files;
                // Take files from given path
                for (const auto & entry : fs::directory_iterator(path))
                {
                    if(fs::is_regular_file(entry.path()) && entry.path().string().find(".mpd") != std::string::npos)
                    {
                        files.push_back(entry.path().string());
                    }
                }
                // Sort files in the ascending order
                std::sort(files.begin(), files.end(), compareFileNames);
                std::shared_ptr<DashMPDDocument> mainDocument = nullptr;
                for(auto &file : files)
                {
                    std::ifstream input(file.c_str());
                    std::string output;
                    input.seekg(0, std::ios::end);
                    output.reserve(input.tellg());
                    input.seekg(0, std::ios::beg);

                    output.assign((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
                    std::shared_ptr<DashMPDDocument> subDocument = std::make_shared<DashMPDDocument>(output);
                    if(nullptr == mainDocument)
                    {
                        std::cout << "Parsed: " << file << std::endl;
                        mainDocument = subDocument;
                    }
                    else
                    {
                        if(subDocument && subDocument->isValid())
                        {
                            // Stich documents using DashMPDDOcument
                            std::cout << "Merging: " << file << std::endl;
                            mainDocument->mergeDocuments(subDocument);
                        }
                        else
                        {
                            std::cout << "Invalid Document: " << file << std::endl;
                        }
                    }
                }
                if(mainDocument && mainDocument->isValid())
                {
                    // Save stiched output to a file
                    std::string outputPath = path + MANIFEST_STICHER_OUTPUT_FILENAME;
                    std::ofstream out(outputPath);
                    out << mainDocument->toString();
                    out.close();
                    std::cout << "Written to: " << outputPath << std::endl;
                }
                else
                {
                    std::cout << "Parse Error!!" << std::endl;
                }
            }
            else
            {
                std::cout << "Invalid Path!!" << std::endl;
                showHelp();
            }
        }
    }
    return 0;
}
