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

#ifndef TEXT_TRACK_INFO_H
#define TEXT_TRACK_INFO_H

/**
 * @brief Structure for text track information
 *        Holds information about a text track in playlist
 */
struct TextTrackInfo
{
    std::string index;
    std::string language;
    bool isCC;
    std::string rendition; //role for DASH, group-id for HLS
    std::string name;
    std::string instreamId;
    std::string characteristics;
    std::string codec;
    std::string label; //Field for label
    int primaryKey; // used for ATSC to store key , this should not be exposed to app.
    std::string accessibilityType; //value of Accessibility
    Accessibility accessibilityItem; /**< Field to store Accessibility Node */
    std::string mType;
    bool isAvailable;

    //setter functions to reduce the number of parameters in constructor
    void setLanguage(const std::string& lang) { language = lang; }
    void setName(const std::string& trackName) { name = trackName; }
    void setInstreamId(const std::string& id) { instreamId = id; }
    void setCharacteristics(const std::string& cha) { characteristics = cha; }
    void setCodec(const std::string& codecStr) { codec = codecStr; }
    void setLabel(const std::string& lab) { label = lab; }
    void setPrimaryKey(int pk) { primaryKey = pk; }
    void setAccessibilityType(const std::string& accType) { accessibilityType = accType; }
    void setAccessibilityItem(const Accessibility& acc) { accessibilityItem = acc; }
    void setType(const std::string& type) { mType = type; }
    void setAvailable(bool available) { isAvailable = available; }

    TextTrackInfo() : index(), language(), isCC(false), rendition(), name(), instreamId(), characteristics(), codec(), primaryKey(0), accessibilityType(), label(), mType(), accessibilityItem(),
              isAvailable(true)
    {
    }

    //2 parameter constructor
    TextTrackInfo(bool cc, std::string rend):
        isCC(cc), rendition(rend), index(), language(), name(), instreamId(), characteristics(), codec(), primaryKey(0), accessibilityType(), label(), mType(), accessibilityItem(), isAvailable(true)
    {
    }

    //8 parameter constructor
    TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string id, std::string cha, int pk):
        index(idx), language(lang), isCC(cc), rendition(rend),
        name(trackName), instreamId(id), characteristics(cha),
        codec(), primaryKey(pk), accessibilityType(), label(), mType(), accessibilityItem(), isAvailable(true)
    {
    }

    //12 parameter full constructor
    TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string codecStr, std::string cha, std::string accType, std::string lab, std::string type, Accessibility acc, bool available):
        index(idx), language(lang), isCC(cc), rendition(rend),
        name(trackName), instreamId(), characteristics(cha),
        codec(codecStr), primaryKey(0), accessibilityType(accType), label(lab), mType(type), accessibilityItem(acc), isAvailable(available)
    {
    }

    void set (std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string codecStr, std::string cha,
            std::string accType, std::string lab, std::string type, Accessibility acc)
    {
        index = idx;
        language = lang;
        isCC = cc;
        rendition = rend;
        name = trackName;
        characteristics = cha;
        codec = codecStr;
        accessibilityType = accType;
        label = lab;
        accessibilityItem = acc;
        mType = type;
    }

    bool operator == (const TextTrackInfo& track) const
    {
        return ((language == track.language) &&
            (isCC == track.isCC) &&
            (rendition == track.rendition) &&
            (name == track.name) &&
            (characteristics == track.characteristics) &&
            (codec == track.codec) &&
            (accessibilityType == track.accessibilityType) &&
            (label == track.label) &&
            (accessibilityItem == track.accessibilityItem) &&
            (mType == track.mType));
    }

    bool operator < (const TextTrackInfo& track) const
    {
        return (index < track.index);
    }

    bool operator > (const TextTrackInfo& track) const
    {
        return (index > track.index);
    }

};

#endif // TEXT_TRACK_INFO_H
