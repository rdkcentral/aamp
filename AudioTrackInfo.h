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

#ifndef AUDIO_TRACK_INFO_H
#define AUDIO_TRACK_INFO_H

/**
 * @struct AudioTrackInfo
 * @brief Structure for audio track information
 *        Holds information about an audio track in playlist
 */
struct AudioTrackInfo
{
    std::string index;            /**< Index of track */
    std::string language;             /**< Language of track */
    std::string rendition;            /**< role for DASH, group-id for HLS */
    std::string name;            /**< Name of track info */
    std::string codec;            /**< Codec of Audio track */
    std::string characteristics;        /**< Characteristics field of audio track */
    std::string label;            /**< label of audio track info */
    int channels;                /**< number channels of track */
    long bandwidth;                /**< Bandwidth value of track **/
    int primaryKey;             /**< used for ATSC to store key , this should not be exposed to app */
    std::string contentType;         /**< used for ATSC to propagate content type */
    std::string mixType;             /**< used for ATSC to propagate mix type */
    std::string accessibilityType;          /**< value of Accessibility */
    bool isMuxed;                 /**< Flag to indicated muxed audio track ; this is used by AC4 tracks */
    Accessibility accessibilityItem;     /**< Field to store Accessibility Node */
    std::string mType;            /**< Type field of track, to be populated by player */
    bool isAvailable;
    bool isDefault;

    AudioTrackInfo() : index(), language(), rendition(), name(), codec(), characteristics(), channels(0),
    bandwidth(0),primaryKey(0), contentType(), mixType(), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(), isAvailable(true),
    isDefault(false)
    {
    }

    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, std::string cha, int ch):
        index(idx), language(lang), rendition(rend), name(trackName),
        codec(codecStr), characteristics(cha), channels(ch), bandwidth(-1), primaryKey(0) , contentType(), mixType(), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(),
        isAvailable(true),isDefault(false)
    {
    }
    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, std::string cha, int ch,bool Default):
        index(idx), language(lang), rendition(rend), name(trackName),
        codec(codecStr), characteristics(cha), channels(ch), bandwidth(-1), primaryKey(0) , contentType(), mixType(), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(),
        isAvailable(true),isDefault(Default)
    {
    }

    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, int pk, std::string conType, std::string mixType):
            index(idx), language(lang), rendition(rend), name(trackName),
            codec(codecStr), characteristics(), channels(0), bandwidth(-1), primaryKey(pk),
                        contentType(conType), mixType(mixType), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(),
            isAvailable(true),isDefault(false)
    {
    }

    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, std::string typ, bool available):
        index(idx), language(lang), rendition(rend), name(trackName),
        codec(codecStr), characteristics(), channels(0), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(typ), isMuxed(false), label(), mType(), accessibilityItem(),
        isAvailable(true),isDefault(false)
    {
    }

    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, int channel):
        index(idx), language(lang), rendition(rend), name(trackName),
        codec(codecStr), characteristics(), channels(channel), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(), isMuxed(false), label(), mType(), accessibilityItem(),
        isAvailable(true),isDefault(false)
    {
    }

    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, int channel, bool muxed, bool available):
        index(idx), language(lang), rendition(rend), name(trackName),
        codec(codecStr), characteristics(), channels(channel), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(), isMuxed(muxed), label(), mType(), accessibilityItem(),
        isAvailable(available),isDefault(false)
    {
    }

    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, std::string typ, bool muxed, std::string lab, std::string type, bool available):
        index(idx), language(lang), rendition(rend), name(trackName),
        codec(codecStr), characteristics(), channels(0), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(typ), isMuxed(muxed), label(lab), mType(type), accessibilityItem(),
        isAvailable(available),isDefault(false)
    {
    }

    AudioTrackInfo(std::string idx, std::string lang, std::string rend, std::string trackName, std::string codecStr, long bw, std::string typ, bool muxed, std::string lab, std::string type, Accessibility accessibility, bool available):
        index(idx), language(lang), rendition(rend), name(trackName),
        codec(codecStr), characteristics(), channels(0), bandwidth(bw),primaryKey(0), contentType(), mixType(), accessibilityType(typ), isMuxed(muxed), label(lab), mType(type), accessibilityItem(accessibility),
        isAvailable(available),isDefault(false)
    {
    }

    bool operator == (const AudioTrackInfo& track) const
    {
        return ((language == track.language) &&
            (rendition == track.rendition) &&
            (contentType == track.contentType) &&
            (codec == track.codec) &&
            (channels == track.channels) &&
            (bandwidth == track.bandwidth) &&
            (isMuxed == track.isMuxed) &&
            (label == track.label) &&
            (mType == track.mType) &&
            (accessibilityItem == track.accessibilityItem) &&
            (isDefault == track.isDefault));
    }

    bool operator < (const AudioTrackInfo& track) const
    {
        return (bandwidth < track.bandwidth);
    }

    bool operator > (const AudioTrackInfo& track) const
    {
        return (bandwidth > track.bandwidth);
    }
};


#endif // AUDIO_TRACK_INFO_H
