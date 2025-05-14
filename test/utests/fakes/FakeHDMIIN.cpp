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

#include "videoin_shim.h"
#include "hdmiin_shim.h"
#include "compositein_shim.h"

StreamAbstractionAAMP_VIDEOIN::StreamAbstractionAAMP_VIDEOIN( const std::string name, PlayerThunderAccessPlugin callSign, class PrivateInstanceAAMP *aamp,double seek_pos, float rate, const std::string type)
                               : StreamAbstractionAAMP(aamp), thunderAccessObj(callSign)
{
}

StreamAbstractionAAMP_VIDEOIN::~StreamAbstractionAAMP_VIDEOIN()
{
}

AAMPStatusType StreamAbstractionAAMP_VIDEOIN::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_VIDEOIN::Start() {  }

void StreamAbstractionAAMP_VIDEOIN::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_VIDEOIN::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_VIDEOIN::GetFirstPTS() { return 0; }

bool StreamAbstractionAAMP_VIDEOIN::IsInitialCachingSupported() { return false; }

long StreamAbstractionAAMP_VIDEOIN::GetMaxBitrate()
{
    return 0;
}

void StreamAbstractionAAMP_VIDEOIN::SetVideoRectangle(int x, int y, int w, int h)
{
}
AAMPStatusType StreamAbstractionAAMP_VIDEOIN::InitHelper(TuneType tuneType)
{
    return eAAMPSTATUS_OK;
}

void StreamAbstractionAAMP_VIDEOIN::StartHelper(int parameter)
{
}

void StreamAbstractionAAMP_VIDEOIN::StopHelper()
{
}

StreamAbstractionAAMP_HDMIIN::StreamAbstractionAAMP_HDMIIN(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                             : StreamAbstractionAAMP_VIDEOIN("HDMIIN", PlayerThunderAccessPlugin::HDMIINPUT,aamp,seek_pos,rate,"HDMI")
{
}

StreamAbstractionAAMP_HDMIIN::~StreamAbstractionAAMP_HDMIIN()
{
}

AAMPStatusType StreamAbstractionAAMP_HDMIIN::Init(TuneType tuneType)
{
        return eAAMPSTATUS_OK;
}

void StreamAbstractionAAMP_HDMIIN::Start(void)
{
}

void StreamAbstractionAAMP_HDMIIN::Stop(bool clearChannelData)
{
}

StreamAbstractionAAMP_HDMIIN* StreamAbstractionAAMP_HDMIIN::GetInstance(class PrivateInstanceAAMP *aamp,double seekpos, float rate)
{
    return nullptr;
}

void StreamAbstractionAAMP_HDMIIN::ResetInstance()
{
}

 StreamAbstractionAAMP_COMPOSITEIN::StreamAbstractionAAMP_COMPOSITEIN(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                              : StreamAbstractionAAMP_VIDEOIN("COMPOSITEIN", PlayerThunderAccessPlugin::COMPOSITEINPUT, aamp,seek_pos,rate,"COMPOSITE")
 {
 }

 StreamAbstractionAAMP_COMPOSITEIN::~StreamAbstractionAAMP_COMPOSITEIN()
 {
 }

 AAMPStatusType StreamAbstractionAAMP_COMPOSITEIN::Init(TuneType tuneType)
 {
     return eAAMPSTATUS_OK;
 }

 void StreamAbstractionAAMP_COMPOSITEIN::Start(void)
 {
 }

 void StreamAbstractionAAMP_COMPOSITEIN::Stop(bool clearChannelData)
 {
 }

StreamAbstractionAAMP_COMPOSITEIN* StreamAbstractionAAMP_COMPOSITEIN::GetInstance(class PrivateInstanceAAMP *aamp,double seekpos, float rate)
{
    return nullptr;
}

void StreamAbstractionAAMP_COMPOSITEIN::ResetInstance()
{
}

