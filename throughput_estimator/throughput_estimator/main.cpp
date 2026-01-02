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
#include "NetworkBandwidthEstimator.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

/**
 * @brief libcurl progress callback (XFERINFOFUNCTION)
 * This is called at frequent intervals allowing application to monitor progress and abort stalled transfers
 * Note: this may be called even after all data has been downloaded while final logic is executed
 *
 * @param dltotal total bytes to download
 * @param dlnow downloaded bytes so far
 * @param ultotal total bytes to upload
 * @param ulnow uploaded bytes so far
 */
static int xferinfo(void *clientp,
					curl_off_t dltotal, curl_off_t dlnow,
					curl_off_t ultotal, curl_off_t ulnow)
{
	DownloadContext *context = reinterpret_cast<DownloadContext*>(clientp);
	return context->xferinfo( dltotal, dlnow );
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{ // stub to avoid spewing download contents to console log
	return size*nmemb;
}

int main(int argc, const char* argv[])
{
	const char *path = std::getenv("outpath");
	if( path == nullptr )
	{
		std::cerr << "please set environment variable 'outpath' to a valid directory\n";
		return EXIT_FAILURE;
	}
	
	std::string pathEWMA = std::string(path) + "/ewma.csv";
	DownloadContext downloadContext(pathEWMA.c_str());
	
	std::string pathABR = std::string(path) + "/abr.csv";
	std::ofstream f_ABR(pathABR, std::ios::binary);
	if( !f_ABR.is_open() )
	{
		std::cerr << "unable to open " << pathABR << "\n";
		return EXIT_FAILURE;
	}
	
	const double segment_duration_seconds = 2.0; // playback duration of media segment
	const double representation_BytesPerSecond = 5000000/8;
	const double segment_size_bytes = representation_BytesPerSecond * segment_duration_seconds;
	
	CURL *curl = curl_easy_init();
	if( !curl )
	{
		std::cerr << "curl_easy_init failed\n";
		return EXIT_FAILURE;
	}
	
	NetworkBandwidthEstimator networkBandwidthEstimator;
	
	// write csv headers
	f_ABR << "TTFB(s),Throughput(Bps),Predicted Download Time(s),Actual Download Time(s)\n";
	
	constexpr int iteration_count = 30;
	for( int i=0; i<iteration_count; i++ )
	{
		// here we download media segment(s) repeatedly on good network to collect baseline performance data
		const char * url = "https://aamp-test-content.s3.us-east-1.amazonaws.com/VideoTestStream/dash/1080p_001.m4s";
		
		f_ABR << networkBandwidthEstimator.GetTimeToFirstByteSeconds() << ","
			  << networkBandwidthEstimator.GetThroughputBytesPerSecond() << ","
			  << networkBandwidthEstimator.GetPredictedDownloadTimeSeconds(segment_size_bytes);
		
		downloadContext.Reset();
		
		curl_easy_setopt(curl, CURLOPT_URL, url);
		
		// enable CURLOPT_XFERINFOFUNCTION, modern alternative to CURLOPT_PROGRESSFUNCTION
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &downloadContext);
		
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // allow compressed transfers
		//curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024L * 64); // larger buffer may produce smoother progress
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "throughput-estimator/1.0");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		const CURLcode res = curl_easy_perform(curl);
		if (res == CURLE_OK ) // || ctx.bailed)
		{ // note: if we bailed early, partial bytes are still fine for statistics
			CurlInfo curlInfo;
			
			curl_off_t size_download;
			curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &size_download);
			curlInfo.m_size_download_bytes = size_download;
			
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curlInfo.m_total_time_seconds);
			
			curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &curlInfo.m_time_to_first_byte_seconds);
			
			networkBandwidthEstimator.UpdateDownloadMetrics(curlInfo);
			f_ABR << "," << curlInfo.m_total_time_seconds << "\n";
		}
		else
		{
			std::cerr << "curl_easy_perform error: " << curl_easy_strerror(res) << "\n";
		}
	} // next iteration
	
	curl_easy_cleanup(curl);
	return EXIT_SUCCESS;
}
