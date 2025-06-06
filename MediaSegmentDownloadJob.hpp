/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file MediaSegmentDownloadJob.hpp
 * @brief Dash Segment Download Job Wrapper structure for AAMP
 */

#ifndef MEDIA_SEGMENT_DOWNLOAD_JOB_HPP
#define MEDIA_SEGMENT_DOWNLOAD_JOB_HPP

#include <string>
#include <map>
#include <iostream>
#include <functional>
#include <memory>
#include "AampDownloadInfo.hpp"

namespace aamp
{
    class MediaSegmentDownloadJob : public AampTrackWorkerJob
    {
    private:
        DownloadInfoPtr mDownloadInfo;
        std::function<void()> mJobFunction;

    public:
        MediaSegmentDownloadJob(DownloadInfoPtr downloadInfo, std::function<void()> jobFunction)
            : mDownloadInfo(std::move(downloadInfo)), mJobFunction(std::move(jobFunction)) {}

        // This is where the job gets executed
        void Execute() override
        {
            if (mJobFunction)
            {
                mJobFunction(); // Call the provided job function
            }
        }

        // Accessor for DownloadInfo
        DownloadInfoPtr GetDownloadInfo() const
        {
            return mDownloadInfo;
        }
    };
}

#endif /* MEDIA_SEGMENT_DOWNLOAD_JOB_HPP */

