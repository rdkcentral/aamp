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
//
// MPDModel.cpp

/**
 * @file MPDModel.cpp
 * @brief
 */

#include "MPDModel.h"
#include "../utils/Url.h"
#include "../utils/Path.h"
#include "../utils/Utils.h"
#include "../xml/DomElement.h"
//#include "../sha1.h"
#include <locale>
#include <cctype>
#include <string>
#include <functional>
#include <mutex>

#define BUFSIZE 1 << 15
#define FOG_EXTRA_NS "fog"
#define FOG_EXTRA_NS_URI "fog:dash:extra"

#define SET_ATTRIBUTE(attr, value, formatted, unset) \
    if ((value) == (unset)) elem.removeAttribute((attr)); \
    else elem.setAttribute((attr), (formatted));

#define SET_ATTRIBUTE_DOUBLE(attr, value, formatted) SET_ATTRIBUTE((attr), (value), (formatted), MPD_UNSET_DOUBLE)
#define SET_ATTRIBUTE_LONG(attr, value, formatted) SET_ATTRIBUTE((attr), (value), (formatted), MPD_UNSET_LONG)
#define SET_ATTRIBUTE_STRING(attr, value, formatted) SET_ATTRIBUTE((attr), (value), (formatted), MPD_UNSET_STRING)

using namespace std;

/**
 * Helper to retrieve an attribute in the current element or in it's parent if not set
 */
/**
 * @brief   Helper to retrieve an attribute in the current element or in it's parent if not set
 * @param   attr String Attribute
 * @param   _this DashMPDRepresentation
 * @param   _parent DashMPDAdaptationSet
 * @retval  Element
 */
template<class T, class P>
string elem_get_attribute_hierarchy(string attr, T _this, P _parent) {
    auto v = _this->elem.attribute(attr, MPD_UNSET_STRING);
    if (v == MPD_UNSET_STRING) {
        auto parent = _parent.lock();
        if(parent) {
            return parent->elem.attribute(attr, MPD_UNSET_STRING);
        }
        else {
            return string();
        }

    } else {
        return v;
    }
};

/**
 * Helper to set the attribute in a hierarchy. Sets in the current element and remove the attribute
 * if set in the parent
 */
/**
 * @brief   Helper to set the attribute in a hierarchy. Sets in the current element and remove the attribute if set in the parent
 * @param   attr String Attribute
 * @param   value String Attribute
 * @param   _this DashMPDRepresentation
 * @param   _parent DashMPDAdaptationSet
 */
template<class T, class P>
void elem_set_attribute_hierarchy(string attr, string value, T _this, P _parent) {
    _this->elem.setAttribute(attr, value);
    auto parent = _parent.lock();
    if(parent) {
        parent->elem.removeAttribute(attr);
    }
};

/**
 * Template function to create a equality predicate based by dereferencing pointers
 */
/**
 * @brief   Template function to create a equality predicate based by dereferencing pointers
 * @param   T DashMPDAdaptationSet
 * @retval  True or False
 */
template<class T>
std::function<bool(const T &)> content_equals(T &self) {
    return [&](const T &other) -> bool { return (*self) == (*other); };
}

/**
 * Returns a local relative filename path given an absolute target URL. This strips query and fragments.
 */
/**
 * @brief   Gets a local relative filename path given an absolute target URL. This strips query and fragments.
 * @param   targetUrl_ Target URL
 * @retval  local relative filename path
 */
string DashMPDRoot::getLocalRelativePathFromURL(string &targetUrl_) {
    string target(targetUrl_);
    Url targetUrl(target);
    Url mpdBaseUrl(this->getBaseUrlValue());

    if (mpdBaseUrl.isParentOf(targetUrl)) {
        // if child of mpd base url, store as simple relative path
        Url localPathUrl = Url(targetUrl.relativeTo(mpdBaseUrl));
        localPathUrl.set_query().clear(); //Clears query args
        return localPathUrl.format(Url::StripTrailingSlash);
    } else {
        // Create a subdirectory for this domain and store path relative to domain root
        string port = targetUrl.port().empty() ? "80" : targetUrl.port();
        Path subdir(string(targetUrl.host()).append("_").append(port));
        Path p = subdir / Path(targetUrl.path());
        return p.toString();
    }
}

/**
 * Find the Base URL for a element.
 */
/**
 * @brief   Finds the Base URL for a element
 * @param   element Element
 * @param   current Parent Base URL
 * @param   isFile Flag to indicate File
 * @retval  Base URL
 */
string findBaseUrl(DomElement &element, const string &current, bool isFile) {
    // Only process the first BaseURL
    DomElement eUrl = element.firstChildElement("BaseURL");

    string _current(current);

    if (eUrl.isNull() && !_current.empty()) {
        if (isFile) return current;

        if (current.back() == '/') {
            return current;
        } else {
            auto i = current.find_last_of('/');
            return current.substr(0, i + 1);
        }
    } else {
        string slash = isFile ? "" : "/";

        if (_current.empty())
            _current = "./";

        Url newbase(eUrl.text());
        if (newbase.isRelative()) {
            auto out = Url(_current).resolve(newbase).format(Url::StripTrailingSlash).append(
                    slash);
            return out;
        } else {
            return newbase.format(Url::StripTrailingSlash).append(slash);
        }
    }
}

/**
 * @brief   Get Dash MPD Segment Template
 * @param   fromChildren Flag to get from children
 * @param   cached Flag to get from cached
 * @retval  Dash MPD Segment Template
 */
std::shared_ptr<DashMPDSegmentTemplate> DashMPDRepresentation::getSegmentTemplate(bool fromChildren, bool cached) {
    if (cached && segmentTemplate) return segmentTemplate;
    segmentTemplate = getFirstChild<DashMPDSegmentTemplate>("SegmentTemplate");
    if (!fromChildren && segmentTemplate->isNull()) {
        auto parent = this->parent.lock();
        if(parent) {
            segmentTemplate = parent->getSegmentTemplate();
        }
    }
    return segmentTemplate;
}


/**
 * @brief   Get Base URL from Parent
 * @retval  Base URL
 */
std::string DashMPDRepresentation::getBaseUrl() {
    string baseurl;
    auto parent = this->parent.lock();
    if(parent) {
        baseurl = findBaseUrl(elem, parent->getBaseUrl());
    }
    return baseurl;
}

/**
 * @brief   Get Bandwidth from "bandwidth" element attribute
 * @retval  bandwidth
 */
long long int DashMPDRepresentation::getBandwidth() {
    auto v = elem.attribute("bandwidth", "0");
    return stoll(v);
}

/**
 * @brief   Get Width from "width" element attribute
 * @retval  width
 */
int DashMPDRepresentation::getWidth() {
    auto v = elem.attribute("width", "0");
    return stoi(v);
}

/**
 * @brief   Get Height from "height" element attribute
 * @retval  Height
 */
int DashMPDRepresentation::getHeight() {
    auto v = elem.attribute("height", "0");
    return stoi(v);
}

/**
 * @brief   Get id from "id" element attribute
 * @retval  id
 */
string DashMPDRepresentation::getId() const {
    return elem.attribute("id", MPD_UNSET_STRING);
}

/**
 * @brief   Get Supplemental Properties
 * @retval  supplementalProperties
 */
std::vector<std::shared_ptr<DashMPDSupplementalProperty>> DashMPDRepresentation::getSupplementalProperties() {
    if (supplementalProperties.size() > 0) return supplementalProperties;
    getChildren(supplementalProperties, "SupplementalProperty");
    return supplementalProperties;
}

/**
 * @brief   Adds Supplemental properties
 * @param   supplementalProperties shared pointer
 * @retval  period
 */
shared_ptr<DashMPDSupplementalProperty> DashMPDRepresentation::addSupplementalProperty(const shared_ptr<DashMPDSupplementalProperty> &supProp)
{
    getSupplementalProperties();
    auto eSupProp = supProp->elem.cloneTo(elem, false);
    auto _supProp = addChild<DashMPDSupplementalProperty>(eSupProp);
    supplementalProperties.push_back(_supProp);

    // sync index
    for (int i = 0; i < supplementalProperties.size(); i++) {
        supplementalProperties[i]->index = i;
    }

    return _supProp;
}

/**
 * @brief   Get id from "id" element attribute
 * @retval  id
 */
string DashMPDPeriod::getId() const {
    return elem.attribute("id");
}

/**
 * @brief   Get id from "id" element attribute
 * @retval  id
 */
string DashMPDAdaptationSet::getId() const {
    return elem.attribute("id", MPD_UNSET_STRING);
}

/**
 * @brief   Validates "EssentialProperty" element attribute
 * @retval  True for "EssentialProperty" or False
 */
bool DashMPDAdaptationSet::isIframeTrack() {
	bool ret = false;
	auto elm = elem.firstChildElement("EssentialProperty");
	while(!elm.isNull())
	{
		auto schemeUri = elm.attribute("schemeIdUri", "");
		if (schemeUri == "http://dashif.org/guidelines/trickmode")
		{
			ret=true;
			break;
		}
		else
		{
			elm = elm.nextSiblingElement("EssentialProperty");
		}
	}
	return ret;
}

bool DashMPDAdaptationSet::isTextTrack() {
        bool ret = false;
	std::string mimeType = getMimeType();
	if ((mimeType == "application/ttml+xml") ||
		(mimeType == "text/vtt") ||
		(mimeType == "application/mp4"))
	{
		ret = true;
	}
        return ret;
}



/**
 * @brief   Get Init URL
 * @retval  URL
 */
std::string DashMPDAdaptationSet::getInitUrl()
{
    string initUrl;
    auto segmentTemplate = this->getSegmentTemplate();
    if (segmentTemplate)
    {
        initUrl = segmentTemplate->getInitializationAttr();
    }
    return initUrl;
}

/**
 * @brief   Validates current Segment Template with Dash MPD Adaptation Set
 * @param   adaptationSet Dash MPD Adaptation Set
 * @retval  True on Dash MPD Adaptation Sets else false
 */
bool DashMPDAdaptationSet::isIdenticalAdaptionSet(const std::shared_ptr<DashMPDAdaptationSet>& adaptationSet)
{
    bool ret = false;
    auto segmentTemplate1 = this->getSegmentTemplate();
    auto segmentTemplate2 = adaptationSet->getSegmentTemplate();
    if (segmentTemplate1 && segmentTemplate2)
    {
        if (segmentTemplate1->getInitializationAttr() == segmentTemplate2->getInitializationAttr()
             && this->getBaseUrl() == adaptationSet->getBaseUrl()
             && segmentTemplate1->getMedia() == segmentTemplate2->getMedia())
        {
             AAMPLOG_TRACE("Found identical Adaption Sets");
             ret = true;
        }
    }
    return ret;
}

/**
 * @brief   Get MimeType from "mimeType" element attribute
 * @retval  MimeType
 */
string DashMPDRepresentation::getMimeType() {
    return elem_get_attribute_hierarchy("mimeType", this, parent);
}

/**
 * @brief   Gets representation Key
 * @retval  Representation Key
 */
string DashMPDRepresentation::getRepresentationKey() const {
    string repId = this->getId();
    string representation = repId.empty() ? to_string(this->index) : repId;
    auto parent = this->parent.lock();
    if(parent) {
        representationKey = parent->getAdaptationSetKey() + "-" + representation;
    }
    return representationKey;
}

/**
 * @brief   Gets Dash MPD Segment List  from "SegmentList" element attribute
 * @retval  Dash MPD Segment List
 */
vector<shared_ptr<DashMPDSegmentList>> DashMPDRepresentation::getSegmentLists() {
    if (!segmentLists.empty()) return segmentLists;
    getChildren(segmentLists, "SegmentList");
    if (segmentLists.empty()) {
        auto parent = this->parent.lock();
        if(parent) {
            segmentLists = parent->getSegmentLists();
        }
    }
    return segmentLists;
}

/**
 * @brief   Get Single Segment Base from "SegmentBase" element attribute
 * @retval  Dash MPD Segment Base
 */
std::shared_ptr<DashMPDSegmentBase> DashMPDRepresentation::getSingleSegmentBase() {
    return getFirstChild<DashMPDSegmentBase>("SegmentBase");
}

/**
 * @brief   Appends Segment List from "SegmentList"
 * @retval  Dash MPD Segment List
 */
std::shared_ptr<DashMPDSegmentList> DashMPDRepresentation::appendSegmentList() {
    if (segmentLists.empty())
        getSegmentLists();
    auto segList = addChild<DashMPDSegmentList>("SegmentList");
    segmentLists.push_back(segList);
    return segList;
}


/**
* @brief   Appends Segment List from "SegmentList"
* @retval  Dash MPD Segment List
*/
std::shared_ptr<DashMPDSegmentTemplate> DashMPDRepresentation::appendSegmentTemplate() {
    if (segmentTemplate) {
        return segmentTemplate;
    }
    segmentTemplate = addChild<DashMPDSegmentTemplate>("SegmentTemplate");
    return segmentTemplate;
}


/**
 * @brief   Gets StartWithSAP
 * @retval  1 on set else 0
 */
int DashMPDRepresentation::getStartWithSAP() {
    string v = elem_get_attribute_hierarchy("startWithSAP", this, parent);
    if (v == MPD_UNSET_STRING) return 0;
    else {
        auto i = stoi(v);
        return i;
    }
}

/**
 * @brief   Removes SegmentLists
 */
void DashMPDRepresentation::removeSegmentLists() {
    for (auto &seg : getSegmentLists()) {
        removeChild(seg);
    }
}

/**
 * @brief   Gets Codecs
 * @retval  Codecs value
 */
std::string DashMPDRepresentation::getCodecs() {
    return elem_get_attribute_hierarchy("codecs", this, parent);
}

/**
 * @brief   Sets Codecs
 * @param   value codecs value
 */
void DashMPDRepresentation::setCodecs(const string &value) {
    elem_set_attribute_hierarchy("codecs", value, this, parent);
}

/**
 * @brief   Merges Dash MPD Representation with "SegmentTemplate"
 * @param   representation Dash MPD Representation
 */
void DashMPDRepresentation::mergeRepresentation(const shared_ptr<DashMPDRepresentation> &representation) {
    // Only merge SegmentTemplate
    auto currentSegmentTemplate = getSegmentTemplate(false, true);
    if (currentSegmentTemplate->isNull()) {
        return;
    }
    auto otherSegmentTemplate = representation->getSegmentTemplate(false, true);
    if (otherSegmentTemplate->isNull()) {
        return;
    }
    currentSegmentTemplate->mergeSegmentTemplate(otherSegmentTemplate);
}

/**
 * @brief   set BaseURL
 * @param   Base URL
 * @retval  Dash MPD Base URL
 */
shared_ptr<DashMPDBaseURL> DashMPDRepresentation::setBaseURLValue(std::string value) {
    if (value == MPD_UNSET_STRING) {
        removeChild("BaseURL");
        return getFirstChild<DashMPDBaseURL>("BaseURL");
    } else {
        auto baseurl = getFirstChild<DashMPDBaseURL>("BaseURL");
        if (baseurl->isNull()) baseurl = addChild<DashMPDBaseURL>("BaseURL");
        baseurl->setText(value);
        return baseurl;
    }
}

/**
 * @brief   Get PeriodKey
 * @retval  Id
 */
std::string DashMPDPeriod::getPeriodKey() const {
    std::string id = getId();
    if (id.empty()) id = to_string(index);
    return id;
}

/**
 * @brief   Get SegmentLists from "SegmentList"
 * @retval  segmentLists
 */
std::vector<std::shared_ptr<DashMPDSegmentList>> DashMPDPeriod::getSegmentLists() {
    if (_parsedSegmentLists) return segmentLists;
    getChildren(segmentLists, "SegmentList");
    _parsedSegmentLists = true;
    return segmentLists;
}


/**
 * @brief   Gets Base URL
 * @retval  Base URL
 */
std::string DashMPDRoot::getBaseUrlValue() {
    Url location = getLocation();
    location = location.parent();
    return findBaseUrl(elem, location.str());
}

/**
 * @brief   Gets Availability Start Time
 * @retval  AvailabilityStartTime in seconds
 */
double DashMPDRoot::getAvailabilityStartTime() {
    return isoDateTimeToEpochSeconds(elem.attribute("availabilityStartTime", ""), MPD_UNSET_DOUBLE);
}

/**
 * @brief   Get MinimumUpdatePeriod from "minimumUpdatePeriod"
 * @retval  MinimumUpdatePeriod in seconds
 */
double DashMPDRoot::getMinimumUpdatePeriod() {
    return isoDurationToSeconds(elem.attribute("minimumUpdatePeriod", ""), MPD_UNSET_DOUBLE);
}

/**
 * @brief   Get MaxSegmentDuration from "maxSegmentDuration"
 * @retval  MaxSegmentDuration in seconds
 */
double DashMPDRoot::getMaxSegmentDuration() {
    return isoDurationToSeconds(elem.attribute("maxSegmentDuration", ""), MPD_UNSET_DOUBLE);
}

/**
 * @brief validate "type"  element attribute  is "dynamic" or "static"
 * @retval  True for "dynamic"
 */
bool DashMPDRoot::isDynamic() {
    return elem.attribute("type", "static") == "dynamic";
}

/**
 * @brief   Set "type" element attribute to "dynamic" or "static"
 * @param   Flag to indicate Dynamic or Static
 */
void DashMPDRoot::setDynamic(bool dynamic) {
    elem.setAttribute("type", dynamic ? "dynamic" : "static");
}

/**
 * @brief   Get SuggestedPresentationDelay from "suggestedPresentationDelay" element attribute
 * @retval  suggestedPresentationDelay in seconds
 */
double DashMPDRoot::getSuggestedPresentationDelay() {
    auto s = elem.attribute("suggestedPresentationDelay", "");
    return isoDurationToSeconds(s, 0);
}

/**
* @brief   Get scte35 event binary
* @retval  scte35 event binary
*/
std::string DashMPDEvent::getEventData() {
    if (eventData.empty()) {
        auto eventSignal = elem.firstChildElement("Signal");
        if (!eventSignal.isNull()) {
            auto eventDataTag = eventSignal.firstChildElement("Binary");
            if (!eventDataTag.isNull()) {
                eventData = eventDataTag.text();
            }
        }
    }
    return eventData;
}

/**
* @brief   Get event Presentation Time
* @retval  Presentation Time
*/
long long DashMPDEvent::getPresentationTime() {
    if (presentationTime < 0) {
        presentationTime = stoll(elem.attribute("presentationTime", "0"));
    }
    return presentationTime;
}


/**
* @brief   Get event duration
* @retval  duration
*/
long long DashMPDEvent::getDuration() {
    if (duration < 0) {
        duration = stoll(elem.attribute("duration", "0"));
    }
    return duration;
}

/**
* @brief   Get event stream schemeIdUri
* @retval  schemeIdUri
*/
std::string DashMPDEventStream::getSchemeIdUri() {
    if (schemeIdUri.empty()) {
        schemeIdUri = elem.attribute("schemeIdUri", "");
    }
    return schemeIdUri;
}

/**
* @brief   Get event stream timescale
* @retval  timescale
*/
long long DashMPDEventStream::getTimeScale() {
    if (timeScale <= 0) {
        timeScale = stoll(elem.attribute("timescale", "1"));
    }
    return timeScale;
}

/**
* @brief   Parse and get events from event stream
* @retval  vector of events
*/
std::vector<shared_ptr<DashMPDEvent>>& DashMPDEventStream::getEvents() {
    if (events.size() > 0) {
        return events;
    }
    getChildren(events, "Event");
    return events;
}

/**
 * @brief   Get ContentType from "contentType" element attribute
 * @retval  ContentType
 */
string DashMPDAdaptationSet::getContentType() {
    return elem.attribute("contentType", MPD_UNSET_STRING);
}

/**
 * @brief   Get MimeType from "mimeType" element attribute
 * @retval  MimeType
 */
string DashMPDAdaptationSet::getMimeType() {
    return elem.attribute("mimeType", MPD_UNSET_STRING);
}

/**
 * @brief   Get Language from "lang" element attribute
 * @retval  lang
 */
string DashMPDAdaptationSet::getLanguage() {
    return elem.attribute("lang", MPD_UNSET_STRING);
}

/**
 * @brief   Get Label element attribute
 * @retval  label
 */
string DashMPDAdaptationSet::getLabel() {
    return elem.attribute("label", MPD_UNSET_STRING);
}

/**
 * @brief   Get Dash MPD Representations
 * @retval  Dash MPD Representations
 */
std::vector<std::shared_ptr<DashMPDRepresentation>> DashMPDAdaptationSet::getRepresentations() {
    if (representations.size() > 0) return representations;
    getChildren(representations, "Representation");
    updateRepresentationsKeyToIndexMap();
    return representations;
}

/**
 * @brief   Get Dash MPD Roles
 * @retval  Dash MPD Roles
 */
std::vector<std::shared_ptr<DashMPDRole>> DashMPDAdaptationSet::getRoles() {
    if (roles.size() > 0) return roles;
    getChildren(roles, "Role");
    return roles;
}

/**
 * @brief   Get Dash MPD Accessibility
 * @retval  Dash MPD Accessibility
 */
std::vector<std::shared_ptr<DashMPDAccessibility>> DashMPDAdaptationSet::getAccessibility() {
    if (accessibility.size() > 0) return accessibility;
    getChildren(accessibility, "Accessibility");
    return accessibility;
}

/**
 * @brief   Get Dash MPD Label
 * @retval  Dash MPD Label
 */
std::vector<std::shared_ptr<DashMPDLabel>> DashMPDAdaptationSet::getLabels() {
    if (labels.size() > 0) return labels;
    getChildren(labels, "Label");
    return labels;
}

/**
 * @brief   Get Supplemental Properties
 * @retval  supplementalProperties
 */
std::vector<std::shared_ptr<DashMPDSupplementalProperty>> DashMPDAdaptationSet::getSupplementalProperties() {
    if (supplementalProperties.size() > 0) return supplementalProperties;
    getChildren(supplementalProperties, "SupplementalProperty");
    return supplementalProperties;
}

/**
 * @brief   Update RepresentationsKey to IndexMap
 */
void DashMPDAdaptationSet::updateRepresentationsKeyToIndexMap() {
    int i = 0;
    for (auto & representation: representations) {
        representationsKeyToIndexMap[representation->getRepresentationKey()] = i;
        i++;
    }
}

/**
 * @brief   Get Duration
 * @retval  Duration
 */
double DashMPDPeriod::getDuration() {
    if (_duration >= 0)
        return _duration;

    _duration = isoDurationToSeconds(elem.attribute("duration", ""), 0);
    if (_duration == 0) {
        auto eNextPeriod = elem.nextSiblingElement("Period");
        if (eNextPeriod.isNull()) {
            double mediaDuration;
            auto parent = this->parent.lock();
            if (parent && parent->isDynamic()) {
                mediaDuration = MPD_UNSET_DOUBLE;
            } else {
                auto eMPD = DomElement(elem.parentNode());
                mediaDuration = isoDurationToSeconds(eMPD.attribute("mediaPresentationDuration", ""), 1e10);
            }
            if(parent) {
                _duration = mediaDuration - (getStart() - parent->getPeriods()[0]->getStart());
            }
        } else {
            auto nextStart = isoDurationToSeconds(eNextPeriod.attribute("start"), 0);
            _duration = nextStart - getStart();
        }
    }
    return _duration;
}

/**
* @brief   Get event stream from period
* @retval  EventStream
*/
std::shared_ptr<DashMPDEventStream> DashMPDPeriod::getEventStream() {
    return getFirstChild<DashMPDEventStream>("EventStream");
}

/**
* @brief   Set event stream into period
* @retval  none
*/
void DashMPDPeriod::setEventStream(std::shared_ptr<DashMPDEventStream> &eventStream) {
    auto newEventStream = eventStream->elem.cloneTo(elem, true);
    addChild<DashMPDEventStream>(newEventStream);
}

/**
 * @brief   Get Start
 * @retval  period Start
 */
double DashMPDPeriod::getStart() {
    if (_periodStart != UNDEFINED_DOUBLE)
        return _periodStart;

    _periodStart = isoDurationToSeconds(elem.attribute("start", ""), MPD_UNSET_DOUBLE);
    if (_periodStart != MPD_UNSET_DOUBLE) return _periodStart;

    // First period
    if (index == 0) return 0;

    auto parent = this->parent.lock();
    if(parent) {
        const std::lock_guard<std::mutex> lock(parent->mBlockPeriodsUpdateMutex);
        try {
            auto periods = parent->getPeriods();
            // Single period
            if (periods.size() == 1)
            {
                return 0;
            }

            auto previousPeriod = periods.at(index - 1);
            _periodStart = previousPeriod->getStart() + previousPeriod->getDuration();
        }
        catch(const std::out_of_range& e) {
           AAMPLOG_ERR("OUT OF RANGE Error. DashMPDPeriod vector index used; index = %d. Returning %f.", index, _periodStart );
        }
    }
    return _periodStart;
}

/**
 * @brief   Get BaseUrl
 * @retval  Base URL
 */
std::string DashMPDPeriod::getBaseUrl() {
    std::string baseUrl;
    auto parent = this->parent.lock();
    if(parent) {
        baseUrl = findBaseUrl(elem, parent->getBaseUrlValue());
    }
    return baseUrl;
}

/**
 * @brief   Get Dash MPD SegmentTemplate
 * @retval  Dash MPD SegmentTemplate
 */
std::shared_ptr<DashMPDSegmentTemplate> DashMPDPeriod::getSegmentTemplate() {
    return getFirstChild<DashMPDSegmentTemplate>("SegmentTemplate");
}


/**
 * @brief   Validates Role value is primary or main or not
 * @retval  True if primary or main, else false
 */
bool DashMPDAdaptationSet::isRolePrimary() {
    auto e = elem.firstChildElement("Role");
    if (e.isNull()) return false;

    auto value = e.attribute("value", "");
    transform(value.begin(), value.end(), value.begin(), ::tolower);

    return (value == "primary " || value == "main");
}

/**
 * @brief   Get Dash MPD SegmentLists
 * @retval  Dash MPD SegmentList
 */
std::vector<std::shared_ptr<DashMPDSegmentList>> DashMPDAdaptationSet::getSegmentLists() {
    if (_parsedSegmentLists) return segmentLists;
    getChildren(segmentLists, "SegmentList");
    if (segmentLists.empty()) {
        auto parent = this->parent.lock();
        if(parent) {
            segmentLists = parent->getSegmentLists();
        }
    }
    _parsedSegmentLists = true;
    return segmentLists;
}


/**
 * @brief   Get Dash MPD AdaptationSet
 * @retval  Dash MPD AdaptationSet
 */
std::vector<std::shared_ptr<DashMPDAdaptationSet>> DashMPDPeriod::getAdaptationSets() {
    if (adaptationSets.size() > 0) return adaptationSets;
    getChildren(adaptationSets, "AdaptationSet");

    updateAdaptationSetsKeyToIndexMap();

    return adaptationSets;
}

/**
 * @brief   update AdaptationSetsKey to IndexMap
 */
void DashMPDPeriod::updateAdaptationSetsKeyToIndexMap() {
    int i = 0;
    for (auto& adaptationSet: adaptationSets) {
        const std::string &adaptationSetKey = adaptationSet->getAdaptationSetKey();
        adaptationSetsKeyToIndexMap[adaptationSetKey] = i;
        i++;
    }
}

/**
 * @brief   Is Active
 * @param   time Time
 * @retval  True id time is between start and end time, else false
 */
bool DashMPDPeriod::isActive(double time) {
    return (getStart() < time && time < getEnd());
}

/**
 * @brief   Get End time
 * @retval  duration
 */
double DashMPDPeriod::getEnd() {
    return getStart() + getDuration();
}

/**
 * @brief   Set or unset start
 * @param   start Start value
 */
void DashMPDPeriod::setStart(double start) {
    SET_ATTRIBUTE_DOUBLE("start", start, epochSecondsToIsoDuration(start));
    _periodStart = start;
}

/**
 * @brief   Set or unset Duration
 * @param   duration Duration value
 */
void DashMPDPeriod::setDuration(double duration) {
    SET_ATTRIBUTE_DOUBLE("duration", duration, epochSecondsToIsoDuration(duration));
}

/**
 * @brief   Set or unset Id
 * @param   id id value
 */
void DashMPDPeriod::setId(const string &id) {
    SET_ATTRIBUTE_STRING("id", id, id);
}

/**
 * @brief   Remove Dash MPD AdaptationSet
 * @param   Dash MPD AdaptationSet
 */
void DashMPDPeriod::removeAdaptationSet(shared_ptr<DashMPDAdaptationSet> &adaptationSet) {
    adaptationSets.erase(std::find_if(adaptationSets.begin(), adaptationSets.end(), content_equals(adaptationSet)));
    removeChild(adaptationSet);
}

/**
* @brief   Print period id, segment start number and end number
*          For debugging purpose
*/
void DashMPDPeriod::dumpPeriodBoundaries()
{
    if (getAdaptationSets().size() > 0) {
        auto adaptationSet = getAdaptationSets().at(0);
        auto segTemplate = adaptationSet->getSegmentTemplate();
        auto timeLine = segTemplate->getSegmentTimeline();
        auto startNum = stoll(segTemplate->elem.attribute("startNumber", "0"));

        vector<TimelineItem> timelineItems;
        extractTimeline(*timeLine, timelineItems);
        auto timeLineSize = timelineItems.size() - 1;
        auto endNumber = startNum + timeLineSize;
        AAMPLOG_ERR("FragmentsInfo : period ID  %s; adaptation %s; startNumber %lld, endNumber %lld  startTime %lld endTime %lld",
            getId().c_str(), adaptationSet->getAdaptationSetKey().c_str(), startNum, endNumber, timelineItems.at(0).startTime, timelineItems.at(timeLineSize).startTime);
    }
    else
    {
        AAMPLOG_ERR("FragmentsInfo : No fragments available in period ID  %s", getId().c_str());
    }
}


/**
 * @brief Update period start time based on PTS offset
 */
void DashMPDPeriod::updateStartTimeToPTSOffset()
{
    if(getAdaptationSets().size() == 0)
    {
	    AAMPLOG_INFO("emptyscteperiod");
	    return;
    }
    auto segTemplate = getAdaptationSets().at(0)->getSegmentTemplate();
    auto fistFragmentTime = getAdaptationSets().at(0)->getSegmentStartTime();
    if (!segTemplate)
    {
        auto representations = getAdaptationSets().at(0)->getRepresentations();
        if (representations.size() > 0)
        {
            segTemplate = representations.at(0)->getSegmentTemplate();
        }
    }
    if (segTemplate && !segTemplate->getSegmentTimeline()->isNull())
    {
        auto presentationTimeOffset = segTemplate->getPresentationTimeOffset();
        if((fistFragmentTime - presentationTimeOffset) > 0)
        {
            // Adjusting start time based on removed segments at the time of download
            // This ensures proper period start time value.
            AAMPLOG_TRACE("presentationoffset:%lld, firstFragmentTime:%lld", presentationTimeOffset, fistFragmentTime);
            double delta = (double)(fistFragmentTime - presentationTimeOffset) / (double)segTemplate->getTimeScale();
            auto periodStart = this->getStart();
            if(periodStart != (periodStart+delta))
            {
                this->setStart(periodStart+delta);
                AAMPLOG_TRACE("Adjusting period startTime (period:%s) :%lf => %lf(+%lf)", this->getId().c_str(), periodStart, this->getStart(), delta);
            }
        }
    }
    else
    {
        AAMPLOG_TRACE("No segment template");
    }
}

void DashMPDPeriod::updateStartTime()
{
    // First period
    if (index == 0)
    {
        this->setStart(0);
    }
    else
    {
        auto parent = this->parent.lock();
        if(parent)
        {
            auto periods = parent->getPeriods();
            // Single period
            if (periods.size() == 1)
            {
                this->setStart(0);
            }
            else
            {
                const std::lock_guard<std::mutex> lock(parent->mBlockPeriodsUpdateMutex);
                try
                {
                    auto previousPeriod = periods.at(index - 1);
                    auto prevPeriodDuration = previousPeriod->getStart();
                    if(getAdaptationSets().size() > 0 )
                    {
                        auto segTemplate = getAdaptationSets().at(0)->getSegmentTemplate();
                        if (!segTemplate)
                        {
                            auto representations = getAdaptationSets().at(0)->getRepresentations();
                            if (representations.size() > 0)
                            {
                                segTemplate = representations.at(0)->getSegmentTemplate();
                            }
                        }
                        if (segTemplate)
                        {
                            prevPeriodDuration += (double) segTemplate->getDuration()/ (double) segTemplate->getTimeScale();
                        }
                    }
                    if(prevPeriodDuration > 0)
                    {
                        this->setStart(prevPeriodDuration);
                    }
                }
                catch(const std::out_of_range& e) {
                    AAMPLOG_ERR("OUT OF RANGE Error. DashMPDPeriod vector index used; index = %d.", index );
                }
            }
        }
    }
}

/**
 * @brief   Removes Segment Details
 */
void DashMPDPeriod::removeSegmentDetails()
{
    removeChild("BaseURL");
    for (auto adaptationSet : getAdaptationSets())
    {
        auto segTemplate = adaptationSet->getSegmentTemplate();
        if (segTemplate)
        {
            auto timescale = segTemplate->getTimeScale();
            segTemplate = adaptationSet->newSegmentTemplate();
            segTemplate->setTimeScale(timescale);
        }

        auto domElm = adaptationSet->elem.addChildElement("AvailableBitrates");
        int iter = 0;
        for (auto representation : adaptationSet->getRepresentations())
        {
            auto repr = domElm.addChildElement("Representation");
            repr.setAttribute("bandwidth", to_string(representation->getBandwidth()));
            repr.setAttribute("codecs", representation->getCodecs());
            auto height = representation->getHeight();
            if (height)
            {
                repr.setAttribute("width", to_string(representation->getWidth()));
                repr.setAttribute("height", to_string(height));
            }
            auto samplingRate = representation->elem.attribute("audioSamplingRate", MPD_UNSET_STRING);
            if (MPD_UNSET_STRING != samplingRate)
            {
                repr.setAttribute("audioSamplingRate", samplingRate);
            }
            for(auto supProp : representation->getSupplementalProperties())
            {
                auto supplProp = repr.addChildElement("SupplementalProperty");
                supplProp.setAttribute("schemeIdUri", supProp->getSchemeIdUri());
                supplProp.setAttribute("value", supProp->getValue());
            }
            if (iter)
            {
                adaptationSet->removeRepresentation(representation);
            }
            iter++;
        }
    }
}

/**
 * @brief   Remove SegmentLists
 */
void DashMPDPeriod::removeSegmentLists() {
    for (auto &seg : getSegmentLists()) {
        removeChild(seg);
    }
    _parsedSegmentLists = false;
    segmentLists.clear();
}

#define VSS_EARLY_COMMON_PERIOD_ID_PREFIX "vss-ck"
#define VSS_EARLY_NEW_PERIOD_ID_PREFIX "vss-nk"

/**
 * @brief Check if the given period is early available period for
 *        virtual stream stitching/license rotation
 * @param None
 * @retval returns true if period is early available period
 */
bool DashMPDPeriod::isVssEarlyAvailablePeriod(){
    bool isVssEAP = false;
    std::string periodId = getId();

    if(periodId == VSS_EARLY_COMMON_PERIOD_ID_PREFIX || STARTS_WITH_IGNORE_CASE(periodId.c_str(), VSS_EARLY_NEW_PERIOD_ID_PREFIX)){
        isVssEAP = true;
    }

    return isVssEAP;
}

/**
 * @brief Check if the given period is a duplicate entry of another period by fog
 * @param None
 * @retval returns true if period is duplicate period, added by fog
 */
bool DashMPDPeriod::isDuplicatePeriod(){
    return STARTS_WITH_IGNORE_CASE(getId().c_str(), FOG_INTERRUPTED_PERIOD_SUFFIX);
}

/**
 * @brief   finds Dash MPD adaptation Set
 * @param   adaptationSet shared pointer to Dash MPD Adaptation Set
 * @retval  Dash MPD Adaptation Set
 */
shared_ptr<DashMPDAdaptationSet> DashMPDPeriod::findAdaptationSet(const shared_ptr<DashMPDAdaptationSet> & adaptationSet)
{
    shared_ptr<DashMPDAdaptationSet> ret;
    for (auto as : getAdaptationSets())
    {
        if (as->elem.attribute("id") == adaptationSet->elem.attribute("id"))
        {
            ret = as;
            break;
        }
    }
    return ret;
}

/**
 * @brief   Merge Dash MPD Period
 * @param   period Dash MPD Period
 */
void DashMPDPeriod::mergePeriod(const shared_ptr<DashMPDPeriod> &period) {
    // Merge adaptationSets
    bool hasNewAdaptationSet = false;
    getAdaptationSets();
    for (auto adaptationSet: period->getAdaptationSets()) {
        const std::string adaptationSetKey = adaptationSet->getAdaptationSetKey();
        // Check if exists the adaptionSet with the same key, if so
        // merge these adaptationSet
        if (adaptationSetsKeyToIndexMap.find(adaptationSetKey) != adaptationSetsKeyToIndexMap.end()) {
            // Merge adaptation set
            auto &toMergedAdaptationSet = adaptationSets[adaptationSetsKeyToIndexMap[adaptationSetKey]];
            toMergedAdaptationSet->mergeAdaptationSet(adaptationSet);
        } else {
            // Insert into the adaptationSets
            AAMPLOG_WARN("New adaptation is getting added to the period at the middle of refresh");
            auto eAdaptationSet = adaptationSet->elem.cloneTo(elem, true);
            auto clonedAdaptationSet = addChild<DashMPDAdaptationSet>(eAdaptationSet);
            adaptationSets.push_back(clonedAdaptationSet);
            hasNewAdaptationSet = true;
        }
    }

    // If there are no adaptationSet, update the key to index map.
    if (hasNewAdaptationSet) {
        updateAdaptationSetsKeyToIndexMap();
    }
    // Update new event stream into mpd period
    removeChild("EventStream");
    auto eventStream = period->getEventStream();
    if (!(eventStream->isNull()))
    {
        setEventStream(eventStream);
    }

    // TODO Merge segmentLists
    // Not sure how to do it, just ignore for now.
}

/**
* @brief   Get segment start number
* @retval  startNumber
*/
long long DashMPDSegmentTemplate::getStartNumber() {
    return startNumber;
}

/**
* @brief   Set segment start number
* @param  startNumber
*/
void DashMPDSegmentTemplate::setStartNumber(long long num) {
    setAttribute("startNumber", to_string(num));
    startNumber = num;
}

/**
* @brief   Get last segment number
* @retval  lastSegNumber
*/
long long DashMPDSegmentTemplate::getLastSegNumber() {
    return lastSegNumber;
}

/**
* @brief   Set last segment number
* @param  lastSegNumber
*/
void DashMPDSegmentTemplate::setLastSegNumber(long long num) {
    lastSegNumber = num;
}

/**
 * @brief   Get Media
 * @retval  Media
 */
string DashMPDSegmentTemplate::getMedia() {
    return elem.attribute("media");
}

/**
 * @brief   Get Periods
 * @retval  Periods
 */
std::vector<std::shared_ptr<DashMPDPeriod>> DashMPDRoot::getPeriods() {
    if (periods.size() > 0) return periods;
    getChildren(periods, "Period");
    return periods;
}


/**
 * @brief   Get Supplemental Properties
 * @retval  supplementalProperties
 */
std::vector<std::shared_ptr<DashMPDSupplementalProperty>> DashMPDRoot::getSupplementalProperties() {
    if (supplementalProperties.size() > 0) return supplementalProperties;
    getChildren(supplementalProperties, "SupplementalProperty");
    return supplementalProperties;
}


/**
 * @brief   ElemVector Constructor
 * @param   parent DomElement
 * @param   childName ChildName
 */
ElemVector::ElemVector(DomElement parent, string childName) {
    DomNode node = parent.firstChild();
    while (!node.isNull()) {
        if (node.isElement()) {
            auto elem = DomElement(node);
            if (elem.tagName() == childName) {
                push_back(elem);
            }
        }
        node = node.nextSiblingNode();
    }
}


/**
 * @brief   Set RelativeBaseUrlElement
 * @param   e Dash MPD
 * @param   parentUrl_ Parent URL
 */
void DashMPDRoot::setRelativeBaseUrlElement(DashMPDAny &e, string parentUrl_) {
    auto mpdElem = (DashMPDElement &) (e);

    auto eBaseUrl = mpdElem.elem.firstChildElement("BaseURL");

    if (!eBaseUrl.isNull()) {
        Url parentUrl(parentUrl_);
        Url absUrl;
        Url mpdUrl(this->getBaseUrlValue());

        // Make URL absolute
        Url elemUrl(eBaseUrl.text());
        if (!elemUrl.str().empty() && elemUrl.isRelative()) {
            absUrl = parentUrl.resolve(elemUrl);
        } else {
            absUrl = elemUrl;
        }

        Url newBaseUrl;
        if (mpdUrl == absUrl) {
            newBaseUrl = Url("./");
        } else if (mpdUrl.isParentOf(absUrl)) {
            // If child URL of MPD URL, store relative path to it.
            newBaseUrl = Url(absUrl.str().substr(mpdUrl.str().size()));
        } else {
            // Create a subdirectory for this domain and store path relative to domain root
            string port = absUrl.port().empty() ? absUrl.port() : "80";
            string subdir = (string(absUrl.host()).append("_").append(port));
            newBaseUrl = subdir.append(absUrl.path());
        }

        string nodeValue;

        if (newBaseUrl.str().empty()) {
            nodeValue = "./";
        } else {
            nodeValue = newBaseUrl.str();
        }

        eBaseUrl.setText(nodeValue);

        // Remove remaining base urls
        while (!(eBaseUrl = eBaseUrl.nextSiblingElement("BaseURL")).isNull()) { ;
            mpdElem.elem.removeChild(eBaseUrl);
        }
    }
}

/**
 * @brief   get TimeShiftBufferDepth
 * @retval  timeShiftBufferDepth
 */
double DashMPDRoot::getTimeShiftBufferDepth() {
    return isoDurationToSeconds(elem.attribute("timeShiftBufferDepth", ""), MPD_UNSET_DOUBLE);
}

/**
 * @brief   set AvailabilityEndTime
 * @param   epochSeconds
 */
void DashMPDRoot::setAvailabilityEndTime(double epochSeconds) {
    std::string isoDateTime;
    if (epochSecondsToIsoDateTime(epochSeconds, isoDateTime)) {
        SET_ATTRIBUTE_DOUBLE("availabilityEndTime", epochSeconds, isoDateTime);
    } else {
        AAMPLOG_ERR("Failed to convert %lf to iso datetime", epochSeconds);
    }
}

/**
 * @brief   Get Location
 * @retval  Location
 */
std::string DashMPDRoot::getLocation() {
    DomElement eLocation = elem.firstChildElement("Location");
    if (eLocation.isNull()) {
        return MPD_UNSET_STRING;
    } else {
        return eLocation.text();
    }
}

/**
 * @brief   Set or Unset Location
 * @param   Location
 */
void DashMPDRoot::setLocation(string location) {
    auto eLocation = elem.firstChildElement("Location");
    if (location == MPD_UNSET_STRING) {
        if (!eLocation.isNull()) elem.removeChild(eLocation);
    } else if (eLocation.isNull()) {
        auto eLocation = elem.addChildElement("Location", "");
        eLocation.setText(location);
    } else {
        eLocation.setText(location);
    }
}

/**
 * @briefa  Set fetchTime
 * @param   FetchTime
 */
void DashMPDRoot::setFetchTime(double fetchTime) {
    elem.addNamespace(FOG_EXTRA_NS, FOG_EXTRA_NS_URI);
    elem.setAttribute(FOG_EXTRA_NS, "fetchTime", to_string(fetchTime));
    _fetchTimeCache = fetchTime;
}

/**
 * @brief   Get fetchTime from element attribute
 * @retval  Fetch Time
 */
double DashMPDRoot::getFetchTime() {
    if (_fetchTimeCache == UNDEFINED_DOUBLE) {
        auto val = elem.attribute(FOG_EXTRA_NS, "fetchTime", "");
        if (val.empty()) {
            _fetchTimeCache = MPD_UNSET_DOUBLE;
        } else {
            _fetchTimeCache = stod(val);
        }
    }
    return _fetchTimeCache;
}

/**
 * @brief   Get MediaPresentationDuration
 * @retval  mediaPresentationDuration
 */
double DashMPDRoot::getMediaPresentationDuration() {
    return isoDurationToSeconds(elem.attribute("mediaPresentationDuration", ""), MPD_UNSET_DOUBLE);
}

/**
 * @brief   Get AvailabilityEndTime
 * @retval  availabilityEndTime
 */
double DashMPDRoot::getAvailabilityEndTime() {
    return isoDateTimeToEpochSeconds(elem.attribute("availabilityEndTime", ""), MPD_UNSET_DOUBLE);
}

/**
 * @brief   set or unset AvailabilityStartTime
 * @param   availStartTime availabilityStartTime  value
 */
void DashMPDRoot::setAvailabilityStartTime(double availStartTime) {
    std::string isoDateTime;
    if (epochSecondsToIsoDateTime(availStartTime, isoDateTime)) {
        SET_ATTRIBUTE_DOUBLE("availabilityStartTime", availStartTime, isoDateTime);
    } else {
        AAMPLOG_ERR("Failed to convert %lf to iso datetime", availStartTime);
    }
}

/**
 * @brief   Gets Publish Time
 * @retval  PublishTime in seconds
 */
double DashMPDRoot::getPublishTime() {
    return isoDateTimeToEpochSeconds(elem.attribute("publishTime", ""), MPD_UNSET_DOUBLE);
}

/**
 * @brief   set publishTime
 * @param   epochSeconds
 */
void DashMPDRoot::setPublishTime(double epochSeconds) {
    std::string isoDateTime;
    if (epochSecondsToIsoDateTime(epochSeconds, isoDateTime)) {
        SET_ATTRIBUTE_DOUBLE("publishTime", epochSeconds, isoDateTime);
    }
}

/**
 * @brief   Set or unset BaseURL
 * @param   value Base URL Value
 * @retval  Base URL
 */
shared_ptr<DashMPDBaseURL> DashMPDRoot::setBaseURLValue(const string &value) {
    if (value == MPD_UNSET_STRING) {
        removeChild("BaseURL");
        return getFirstChild<DashMPDBaseURL>("BaseURL");
    } else {
        auto baseurl = getFirstChild<DashMPDBaseURL>("BaseURL");
        if (baseurl->isNull()) baseurl = addChild<DashMPDBaseURL>("BaseURL");
        baseurl->setText(value);
        return baseurl;
    }
}

/**
 * @brief   Set or Unset timeShiftBufferDepth
 * @param   tsbDepth time Shift Buffer Depth value
 */
void DashMPDRoot::setTimeShiftBufferDepth(double tsbDepth) {
    SET_ATTRIBUTE_DOUBLE("timeShiftBufferDepth", tsbDepth, epochSecondsToIsoDuration(tsbDepth));
}

/**
 * @brief   Set or unset MediaPresentationDuration
 * @param   mpdDuration mediaPresentationDuration value
 */
void DashMPDRoot::setMediaPresentationDuration(double mpdDuration) {
    SET_ATTRIBUTE_DOUBLE("mediaPresentationDuration", mpdDuration, epochSecondsToIsoDuration(mpdDuration));
}

/**
 * @brief   set or unset MinimumUpdatePeriod
 * @param   period Period
 */
void DashMPDRoot::setMinimumUpdatePeriod(double period) {
    SET_ATTRIBUTE_DOUBLE("minimumUpdatePeriod", period, epochSecondsToIsoDuration(period));
}

/**
 * @brief   Insert Period
 * @param   period Period
 */
void DashMPDRoot::insertPeriodBefore(const shared_ptr<DashMPDPeriod> &period) {
    const std::lock_guard<std::mutex> lock(mBlockPeriodsUpdateMutex);
    getPeriods();  // initialize period vector
    auto ePeriod = period->elem.cloneTo(elem, true);
    auto _period = addChild<DashMPDPeriod>(ePeriod);
    periods.insert(periods.begin(), _period);  // insert before

    // sync index
    for (int i = 0; i < periods.size(); i++) {
        periods[i]->index = i;
    }
}

/**
 * @brief   adds Period
 * @param   period shared pointer to Period
 * @retval  period
 */
shared_ptr<DashMPDPeriod> DashMPDRoot::addPeriod(const shared_ptr<DashMPDPeriod> &period)
{
    const std::lock_guard<std::mutex> lock(mBlockPeriodsUpdateMutex);
    getPeriods();
    auto ePeriod = period->elem.cloneTo(elem, false);
    auto _period = addChild<DashMPDPeriod>(ePeriod);
    periods.push_back(_period);

    // sync index
    for (int i = 0; i < periods.size(); i++) {
        periods[i]->index = i;
    }

    return _period;
}

/**
 * @brief   adds Period
 * @param   period shared pointer to Period
 * @retval  period
 */
shared_ptr<DashMPDSupplementalProperty> DashMPDRoot::addSupplementalProperty(const shared_ptr<DashMPDSupplementalProperty> &supProp)
{
    getSupplementalProperties();
    auto eSupProp = supProp->elem.cloneTo(elem, false);
    auto _supProp = addChild<DashMPDSupplementalProperty>(eSupProp);
    supplementalProperties.push_back(_supProp);

    // sync index
    for (int i = 0; i < supplementalProperties.size(); i++) {
        supplementalProperties[i]->index = i;
    }

    return _supProp;
}

/**
* @brief   finds Period
* @param   period Id string
* @retval  period
*/
shared_ptr<DashMPDPeriod> DashMPDRoot::findPeriod(const string &periodId)
{
    auto periods = getPeriods();
    shared_ptr<DashMPDPeriod> retPeriod = nullptr;
    for (int iter = (int)periods.size() - 1; iter >= 0; iter--)
    {
        shared_ptr<DashMPDPeriod> currPeriod = periods.at(iter);
        if (currPeriod->getId() == periodId)
        {
            retPeriod = currPeriod;
            break;
        }
    }
    return retPeriod;
}

/**
 * @brief   finds Period
 * @param   period shared pointer to Period
 * @retval  period
 */
shared_ptr<DashMPDPeriod> DashMPDRoot::findPeriod(const shared_ptr<DashMPDPeriod> &period)
{
    getPeriods();
    auto iter = std::find_if(periods.begin(), periods.end(), content_equals(period));
    if (iter == periods.end())
    {
        return nullptr;
    }
    else
    {
        return *(iter);
    }
}

/**
 * @brief   Remove Period
 * @param   period Period
 */
void DashMPDRoot::removePeriod(shared_ptr<DashMPDPeriod> &period) {
    AAMPLOG_TRACE("Removing period with id %s", period->getId().c_str());
    const std::lock_guard<std::mutex> lock(mBlockPeriodsUpdateMutex);
    periods.erase(std::find_if(periods.begin(), periods.end(), content_equals(period)));
    removeChild(period);
    //Updating the period index  after removing the period.
    //Otherwise, after removing a period from the manifest, the index
    //mapped to the periods will be incorrect. That might lead to the out of
    //range exceptions and severe playback issues.

    for(int periodCount = 0; periodCount < periods.size(); periodCount++)
    {
	periods[periodCount]->index = periodCount;
	AAMPLOG_TRACE("Index updated as %d for period:%s", periodCount,periods[periodCount]->getId().c_str());
    }
}

/**
 * @brief   Get Base URL
 * @retval  Base URL
 */
std::string DashMPDAdaptationSet::getBaseUrl() {
    std::string baseUrl;
    auto parent = this->parent.lock();
    if(parent) {
        baseUrl = findBaseUrl(elem, parent->getBaseUrl());
    }
    return baseUrl;
}


/**
* @brief   Get SegmentTemplate
* @retval  Segment Template
*/
std::shared_ptr<DashMPDSegmentTemplate> DashMPDAdaptationSet::newSegmentTemplate() {
    removeChild("SegmentTemplate");
    segmentTemplate = addChild<DashMPDSegmentTemplate>("SegmentTemplate");
    return segmentTemplate;
}


/**
 * @brief   Get SegmentTemplate
 * @param   fromChildren Flag to indicate to get from children
 * @param   cached Flag for cached
 * @retval  Segment Template
 */
std::shared_ptr<DashMPDSegmentTemplate> DashMPDAdaptationSet::getSegmentTemplate(bool fromChildren, bool cached) {
    if (cached && segmentTemplate) return segmentTemplate;
    segmentTemplate = getFirstChild<DashMPDSegmentTemplate>("SegmentTemplate");
    if (!fromChildren && segmentTemplate->isNull()) {
        auto parent = this->parent.lock();
        if(parent) {
            segmentTemplate = parent->getSegmentTemplate();
        }
    }
    return segmentTemplate;
}

/**
 * @brief   Get SegmentAlignment
 * @retval  True if "segmentAlignment" is True
 */
bool DashMPDAdaptationSet::getSegmentAlignment() {
    return elem.attribute("segmentAlignment", "false") == "true";
}

/**
 * @brief   Remove Representation
 * @param   repr Representation
 */
void DashMPDAdaptationSet::removeRepresentation(shared_ptr<DashMPDRepresentation> &repr) {
    representations.erase(std::find_if(representations.begin(), representations.end(), content_equals(repr)));
    removeChild(repr);
}

/**
 * @brief   Remove SegmentLists
 */
void DashMPDAdaptationSet::removeSegmentLists() {
    for (auto &seg : getSegmentLists()) {
        removeChild(seg);
    }
    _parsedSegmentLists = false;
    segmentLists.clear();
}

/**
 * @brief   set BaseURL
 * @param   Base URL
 * @retval  Dash MPD Base URL
 */
shared_ptr<DashMPDBaseURL> DashMPDAdaptationSet::setBaseURLValue(std::string value) {
    if (value == MPD_UNSET_STRING) {
        removeChild("BaseURL");
        return getFirstChild<DashMPDBaseURL>("BaseURL");
    } else {
        auto baseurl = getFirstChild<DashMPDBaseURL>("BaseURL");
        if (baseurl->isNull()) baseurl = addChild<DashMPDBaseURL>("BaseURL");
        baseurl->setText(value);
        return baseurl;
    }
}

/**
 * @brief   Gets Segment Count
 * @retval  segment count
 */
int DashMPDAdaptationSet::getSegmentCount()
{
    int count = 0;
    auto segmentTemplate = getSegmentTemplate();
    if (!segmentTemplate)
    {
        auto representations = getRepresentations();
        if (representations.size() > 0)
        {
            segmentTemplate = representations.at(0)->getSegmentTemplate();
        }
    }
    if (segmentTemplate)
    {
        auto segmentTimeline = segmentTemplate->getSegmentTimeline();
        if (segmentTimeline)
        {
            auto timeline = *segmentTimeline;
            auto S = timeline.elem.firstChildElement("S");
            while (!S.isNull())
            {
                auto rep = stoll(S.attribute("r", "0"));
                count += rep + 1;
                S = S.nextSiblingElement("S");
            }
        }
    }
    else
    {
        auto representations = getRepresentations();
        if (representations.size() > 0)
        {
            auto segmentList = representations.at(0)->getSegmentLists().at(0);
            if (segmentList)
            {
                count = (int)segmentList->getSegmentURLs().size();
            }
        }
    }

    return count;
}

/**
 * @brief   Adds Representation
 * @param   repr pointer to DashMPDRepresentation
 * @retval  added Representation
 */
shared_ptr<DashMPDRepresentation> DashMPDAdaptationSet::addRepresentation(shared_ptr<DashMPDRepresentation> &repr)
{
    auto eRepresentation = repr->elem.cloneTo(elem, true);
    auto clonedRepresentation = addChild<DashMPDRepresentation>(eRepresentation);
    representations.push_back(clonedRepresentation);
    updateRepresentationsKeyToIndexMap();
    return clonedRepresentation;
}

/**
 * @brief   Gets Segment start Time
 * @retval  start Time
 */
long long DashMPDAdaptationSet::getSegmentStartTime()
{
    long long startTime = 0;
    auto segmentTemplate = getSegmentTemplate();
    if (!segmentTemplate)
    {
        auto representations = getRepresentations();
        if (representations.size() > 0)
        {
            segmentTemplate = representations.at(0)->getSegmentTemplate();
        }
    }
    if (segmentTemplate)
    {
       auto segmentTimeline = segmentTemplate->getSegmentTimeline();
       if (segmentTimeline)
       {
           auto timeline = *segmentTimeline;
           auto S = timeline.elem.firstChildElement("S");
           if (!S.isNull())
           {
               startTime = stoll(S.attribute("t", "0"));
           }
       }
       else
       {
           startTime = segmentTemplate->getPresentationTimeOffset();
       }
    }
    return startTime;
}

/**
 * @brief   merge AdaptationSet
 * @param   adaptationSet adaptation Set
 */
void DashMPDAdaptationSet::mergeAdaptationSet(const shared_ptr<DashMPDAdaptationSet> &adaptationSet) {
    // Merge representations
    bool hasNewRepresentation = false;
    bool appendSegmentTemplateToRepr = false;
    auto currentSegmentTemplate = getSegmentTemplate(true, false);
    auto otherSegmentTemplate = adaptationSet->getSegmentTemplate(true, false);
    getRepresentations();
    // If the segmentTemplates are different in BaseUrl/initialization/media values, append the other segmentTemplate to new representation
    if (!currentSegmentTemplate->isNull() && !otherSegmentTemplate->isNull()
        && ((currentSegmentTemplate->getInitializationAttr() != otherSegmentTemplate->getInitializationAttr())
        || (currentSegmentTemplate->getMedia() != otherSegmentTemplate->getMedia()))) {
            appendSegmentTemplateToRepr = true;
    }

    const std::vector<std::shared_ptr<DashMPDRepresentation>>& toMergeRepresentations = adaptationSet->getRepresentations();
    for (auto& toMergeRepresentation: toMergeRepresentations) {
        const std::string &key = toMergeRepresentation->getRepresentationKey();
        if (representationsKeyToIndexMap.find(key) != representationsKeyToIndexMap.end()) {
            // Merge representation
            auto representation = representations[representationsKeyToIndexMap[key]];
            representation->mergeRepresentation(toMergeRepresentation);
        } else {
            // Insert new representation
            AAMPLOG_WARN("New representation at the middle of manifest refresh %s", key.c_str());
            auto eRepresentation = toMergeRepresentation->elem.cloneTo(elem, true);
            auto clonedRepresentation = addChild<DashMPDRepresentation>(eRepresentation);
            representations.push_back(clonedRepresentation);
            hasNewRepresentation = true;
            // append segment template to newly added representation
            if (appendSegmentTemplateToRepr) {
                auto newSegmentTemplate = clonedRepresentation->appendSegmentTemplate();
                newSegmentTemplate->setMedia(otherSegmentTemplate->getMedia());
                newSegmentTemplate->setInitializationAttr(otherSegmentTemplate->getInitializationAttr());
                // What about startNumber and presentationTimeOffset... TBD
            }
            // append baseurl to newly added representation
            if (getBaseUrl() != adaptationSet->getBaseUrl()) {
                clonedRepresentation->setBaseURLValue(adaptationSet->getBaseUrl());
            }
        }
    }

    if (hasNewRepresentation) {
        updateRepresentationsKeyToIndexMap();
    }

    // Merge segmentTemplate
    if (currentSegmentTemplate->isNull()) {
        return;
    }
    if (otherSegmentTemplate->isNull()) {
        return;
    }

    currentSegmentTemplate->mergeSegmentTemplate(otherSegmentTemplate);

    // TODO merge segmentLists, not sure how to do it, ignore now.
}

/**
 * @brief   adjust Cut off based on SegmentTimeline
 * @param   Time
 */
void DashMPDSegmentTemplate::adjustCutoff(double time) {
    auto eTimeline = elem.firstChildElement("SegmentTimeline");

    if (eTimeline.isNull()) return;

    DomElement S = eTimeline.firstChildElement("S");

    long long curTime = 0;
    while (!S.isNull()) {

        auto startTime = stoll(S.attribute("t", "0"));
        if (startTime > 0)
            curTime = startTime;

        auto dur = stoll(S.attribute("d", "0"));
        auto rep = stoll(S.attribute("r", "0"));
        long finalRep = -1;
        for (int i = 0; i <= rep && curTime < time; i++) {
            curTime += dur;
            finalRep = i;
        }
        DomElement nxt = S.nextSiblingElement("S");
        if (finalRep != rep) {
            if (finalRep >= 0) {
                S.setAttribute("r", to_string(finalRep));
            } else {
                eTimeline.removeChild(S);
            }
        }
        S = nxt;
    }
}


/**
 * @brief   DashMPDDocument Constructor
 * @param   content
 */
DashMPDDocument::DashMPDDocument(const string &content) :xmlDoc(make_shared<DomDocument>()),root() {
    xmlDoc->setContent(content);
}

/**
 * @brief   Checks Document initialization
 * @retval  true id Document is initialized or false
 */
bool DashMPDDocument::isValid() {
    return xmlDoc->initialized();
}

/**
 * @brief   Get Root of Document
 * @retval  root
 */
shared_ptr<DashMPDRoot> DashMPDDocument::getRoot() {
    if (!root) {
        root = std::make_shared<DashMPDRoot>(xmlDoc->documentElement(), 0);
    }
    return root;
}

/**
 * @brief   Clone Dash MPD Document
 * @param   deep Flag to indicate recursive copy
 * @retval  New Cloned MPD Document
 */
shared_ptr<DashMPDDocument> DashMPDDocument::clone(bool deep) {
    auto cloneDoc = xmlDoc->cloneDoc(deep);
    auto mpdDoc = std::make_shared<DashMPDDocument>(cloneDoc);
    return mpdDoc;
}

string DashMPDDocument::toString() {
    return xmlDoc->toString();
}

/**
 * @brief   Merge Dash MPD Document
 * @param   newDocument Document pointer
 */
void DashMPDDocument::mergeDocuments(shared_ptr<DashMPDDocument> &newDocument) {
    // Sync periods from newDocument
    for(auto &newPeriod : newDocument->getRoot()->getPeriods()) {
        auto newPeriodId = newPeriod->getId();
        auto period = this->getRoot()->findPeriod(newPeriodId);
        if(period == nullptr) {
            this->getRoot()->addPeriod(newPeriod);
        } else {
            period->mergePeriod(newPeriod);
        }
    }
}

/**
 * @brief   Get TimeScale
 * @retval  TimeScale value
 */
long long int DashMPDSegmentBase::getTimeScale() {
    auto container = DomElement(elem.parentNode());
    string val(MPD_UNSET_STRING);
    while (true) {
        DomElement segInfo = container.firstChildElement("SegmentBase");
        if (segInfo.isNull()) segInfo = container.firstChildElement("SegmentTemplate");
        if (segInfo.isNull()) segInfo = container.firstChildElement("SegmentList");
        if (!segInfo.isNull()) {
            val = segInfo.attribute("timescale", MPD_UNSET_STRING);
        }

        if (val == MPD_UNSET_STRING) {
            container = DomElement(container.parentNode());
            if (container.isNull() || container.tagName() == "MPD") break;
        } else {
            break;
        }
    }
    if (val == MPD_UNSET_STRING) val = "1";
    return stoll(val);
}

/**
 * @brief   Set or unset timescale
 * @param   timescale timescale value
 */
void DashMPDSegmentBase::setTimeScale(long long int timescale) {
    SET_ATTRIBUTE_LONG("timescale", timescale, to_string(timescale));
}

/**
 * @brief   Get PresentationTimeOffset
 * @retval  Presentation Time Offset
 */
long long DashMPDSegmentBase::getPresentationTimeOffset() {
    return stoll(elem.attribute("presentationTimeOffset", "0"));
}

/**
 * @brief   Get Initialization
 * @retval  Initialization Dash MPD URL Type
 */
shared_ptr<DashMPDURLType> DashMPDSegmentBase::getInitialization() {
    if (initialization) return initialization;
    initialization = getFirstChild<DashMPDURLType>("Initialization");
    return initialization;
}

/**
 * @brief   Set Initialization
 * @param   source Source
 * @param   range Range
 */
void DashMPDSegmentBase::setInitialization(const string &source, const string &range) {
    auto init = getInitialization();
    if (init->isNull()) {
        init = addChild<DashMPDURLType>("Initialization");
    }
    init->setSourceURL(source);
    init->setRange(range);
}

/**
 * @brief   Set or Unset presentation Time Offset
 * @param   offset presentation Time Offset value
 */
void DashMPDSegmentBase::setPresentationTimeOffset(long long int offset) {
    SET_ATTRIBUTE_LONG("presentationTimeOffset", offset, to_string(offset));
}

/**
 * @brief   Get Initialization
 * @retval  initialization
 */
string DashMPDSegmentTemplate::getInitializationAttr() {
    return elem.attribute("initialization", MPD_UNSET_STRING);
}

/**
 * @brief   Set or Unset Media
 * @param   value Media value
 */
void DashMPDSegmentTemplate::setMedia(const string &value) {
    SET_ATTRIBUTE_STRING("media", value, value);
}

/**
 * @brief   Set or Unset Initialization
 * @param   value Initialization value
 */
void DashMPDSegmentTemplate::setInitializationAttr(const string &value) {
    SET_ATTRIBUTE_STRING("initialization", value, value);
}

/**
 * @brief   Merge Dash MPD SegmentTemplate
 * @param   segmentTemplate Dash MPD Segment Template
 */
void DashMPDSegmentTemplate::mergeSegmentTemplate(const shared_ptr<DashMPDSegmentTemplate> &segmentTemplate) {
    // startNumber is varying, but just assume the startNumber is increasing
    // so we can only ignore it.
    shared_ptr<DashMPDSegmentTimeline> segmentTimeline = getSegmentTimeline();
    if (segmentTimeline->isNull()) {
        return;
    }
    shared_ptr<DashMPDSegmentTimeline> otherSegmentTimeline = segmentTemplate->getSegmentTimeline();
    if (otherSegmentTimeline->isNull()) {
        return;
    }

    segmentTimeline->mergeTimeline(otherSegmentTimeline);
}

/**
 * @brief   Get Segment Timeline
 * @retval  Segment Timeline
 */
shared_ptr<DashMPDSegmentTimeline> DashMPDMultipleSegmentBase::getSegmentTimeline() {
    if (!segmentTimeline) {
        segmentTimeline = getFirstChild<DashMPDSegmentTimeline>("SegmentTimeline");
    }
    return segmentTimeline;
}

/**
 * @brief   Get Duration
 * @retval  Duration
 */
long long int  DashMPDMultipleSegmentBase::getDuration() {
    std::string val = elem.attribute("duration", "");
    long long int Duration = MPD_UNSET_LONG;
    /* adding a temporary try catch exception handler*/
    if (!val.empty()) {
	try {
		Duration =  stoll(val);
	}
	catch(const std::out_of_range) {
		AAMPLOG_ERR("Out of range: %s", val.c_str());
	}
	catch(const std::invalid_argument) {
		AAMPLOG_ERR("Invalid argument: %s", val.c_str());
	}
    }
    return Duration;
}

/**
 * @brief   Get Start Number
 * @retval  Start Number
 */
long long int DashMPDMultipleSegmentBase::getStartNumber() {
    return stoll(elem.attribute("startNumber", "1"));
}

/**
 * @brief   Set or Unset duration
 * @param   duration
 */
void DashMPDMultipleSegmentBase::setDuration(long long int duration) {
    if (duration == MPD_UNSET_LONG) elem.removeAttribute("duration");
    else elem.setAttribute("duration", to_string(duration));
}

/**
 * @brief   create Segment Timeline
 * @retval  Segment Timeline
 */
shared_ptr<DashMPDSegmentTimeline> DashMPDMultipleSegmentBase::createSegmentTimeline() {
    if (segmentTimeline) removeChild(segmentTimeline);
    segmentTimeline = addChild<DashMPDSegmentTimeline>("SegmentTimeline");
    return segmentTimeline;
}

/**
 * @brief   set or unset StartNumber
 * @param   start Number
 */
void DashMPDMultipleSegmentBase::setStartNumber(long long number) {
    SET_ATTRIBUTE_LONG("startNumber", number, to_string(number));
}

/**
 * @brief   Get Media
 * @retval  Media
 */
std::string DashMPDSegmentURL::getMedia() const{
    return elem.attribute("media", MPD_UNSET_STRING);
}

/**
 * @brief   Set or unset Media Range
 * @retval  media Range
 */
string DashMPDSegmentURL::getMediaRange() {
    return elem.attribute("mediaRange", MPD_UNSET_STRING);
}

/**
 * @brief   Set segment disk path, used for culling
 * @param   path
 */
void DashMPDSegmentURL::setSegmentPath(string path) {
    segmentPath = path;
}

/**
 * @brief   Get segment disk path
 * @retval  segmentPath 
 */
string DashMPDSegmentURL::getSegmentPath() {
    return segmentPath;
}

/**
 * @brief   Update media for segment url
 * @param   media 
 * @retval   media
 */
void DashMPDSegmentURL::setMedia(string media) {
    if (media == MPD_UNSET_STRING) elem.removeAttribute("media");
    else elem.setAttribute("media", media);
}

/**
 * @brief   Gets URLs count
 * @retval  URLs count
 */
int DashMPDSegmentList::getSegmentURLCount()
{
    if(segmentURLs.empty()) getSegmentURLs();
    return (int)segmentURLs.size();
}

/**
 * @brief   Gets Dash MPD Segment URLs
 * @retval  Dash MPD Segment URLs
 */
std::vector<shared_ptr<DashMPDSegmentURL>>& DashMPDSegmentList::getSegmentURLs() {
    if (!segmentURLs.empty()) return segmentURLs;
    getChildren(segmentURLs, "SegmentURL");
    return segmentURLs;
}

/**
 * @brief   Removes Dash MPD Segment URLs from segmentURLs vector
 * @param   segUrl Dash MPD Segment URL
 * @retval  Dash MPD Segment URLs
 */
void DashMPDSegmentList::removeSegmentUrl(shared_ptr<DashMPDSegmentURL> &segUrl)
{
    segmentURLs.erase(std::find_if(segmentURLs.begin(), segmentURLs.end(), content_equals(segUrl)));
    removeChild(segUrl);
}

/**
 * @brief   Append Dash MPD Segment URL
 * @retval  Dash MPD Segment URL
 */
shared_ptr<DashMPDSegmentURL> DashMPDSegmentList::appendSegmentURL() {
    if (segmentURLs.empty()) getSegmentURLs();
    auto segurl = addChild<DashMPDSegmentURL>("SegmentURL");
    segmentURLs.push_back(segurl);
    return segurl;
}

/**
 * @brief   Get SourceURL from "sourceURL" element attribute
 * @retval  sourceURL
 */
string DashMPDURLType::getSourceURL() {
    return elem.attribute("sourceURL", MPD_UNSET_STRING);
}

/**
 * @brief   Get Range from "range" element attribute
 * @retval  Range
 */
string DashMPDURLType::getRange() {
    return elem.attribute("range", MPD_UNSET_STRING);
}

/**
 * @brief   set or unset sourceURL
 * @param   source Source URL
 */
void DashMPDURLType::setSourceURL(const string &source) {
    if (source == MPD_UNSET_STRING) elem.removeAttribute("sourceURL");
    else elem.setAttribute("sourceURL", source);
}

/**
 * @brief   set or unset Range
 * @param   range
 */
void DashMPDURLType::setRange(const string &range) {
    if (range == MPD_UNSET_STRING) elem.removeAttribute("range");
    else elem.setAttribute("range", range);
}

/**
 * @brief   Append Segment
 * @retval  Dash MPD TimelineS
 */
shared_ptr<DashMPDTimelineS> DashMPDSegmentTimeline::appendSegment() {
    auto timeLineSegment =  addChild<DashMPDTimelineS>("S", ++timeLineId);
    timeLineSegments.push_back(timeLineSegment);
    return timeLineSegment;
}


/**
* @brief   Remove TimeLine Segment
* @param   Dash MPD TimelineS
*/
void DashMPDSegmentTimeline::removeTimeLine(std::shared_ptr<DashMPDTimelineS> &timeLine) {
    timeLineSegments.erase(std::find_if(timeLineSegments.begin(), timeLineSegments.end(), content_equals(timeLine)));
    removeChild(timeLine);
}

/**
* @brief   Return the segment Timeline
* @retval segment Timeline
*/
vector<shared_ptr<DashMPDTimelineS>>& DashMPDSegmentTimeline::getTimeLineSegments() {
    if (timeLineSegments.size() > 0) return timeLineSegments;
    getChildren(timeLineSegments, "S");
    return timeLineSegments;
}


/**
 * @brief   Merge TimeLines timeline Items and other Timeline Items
 * @param   timeline TimeLine
 */
void DashMPDSegmentTimeline::mergeTimeline(const shared_ptr<DashMPDSegmentTimeline> &timeline) {
    vector<TimelineItem> otherTimelineItems;
    extractTimeline(*timeline, otherTimelineItems);

    // Merge timelines, find which timeline item is exactly prior to the first timeline
    // of the new timeline items (otherTimelineItems)
    bool found = false;
    auto lastTimelineS = this->getTimeLineSegments().back();
    long long int duration = stoll(lastTimelineS->elem.attribute("d", "0"));
    long long int startTime = stoll(lastTimelineS->elem.attribute("t", "0"));
    long long int rep = stoll(lastTimelineS->elem.attribute("r", "0"));
    long long int boundaryStartTime = startTime + (duration * (rep+1));
    // Iterate from back to match the segments ASAP
    for (std::vector<TimelineItem>::reverse_iterator tItem= otherTimelineItems.rbegin(); tItem!=otherTimelineItems.rend(); tItem++) {
        if (boundaryStartTime == tItem->startTime) {
            found = true;
            break;
        }
    }

    // Found the joint point of two timeline series
    // Merge them into one timeline series
    // And modify the <S> (DashMPDTimelineS) elements
    if (found) {
        // Iterate all new timelineItems and merge the items into a same S if their duration is same from current record.
        for(auto &tItem: otherTimelineItems)
        {
            if(tItem.startTime < boundaryStartTime)
            {
                // Skip segments, timeline is already present
                continue;
            }
            else
            {
                if (duration != tItem.duration) {
                    if (tItem.startTime >= 0 && tItem.duration >= 0) {
                        // Save the last repeat count once again as it may have recalibrated.
                        lastTimelineS->setRepeat(rep);
                        // Add a S element for previous timeline items.
                        lastTimelineS = appendSegment();
                        lastTimelineS->setTime(tItem.startTime);
                        lastTimelineS->setDuration(tItem.duration);
                        lastTimelineS->setRepeat(0);
                    }
                    startTime = tItem.startTime;
                    duration = tItem.duration;
                    rep = 0;
                }
                else {
                    rep++;
                }
            }
        }

        // Save for the last one
        if (startTime >= 0 && duration >= 0) {
            // Add a S element for previous timeline items.
            //auto newTimelineS = appendSegment();
            lastTimelineS->setTime(startTime);
            lastTimelineS->setDuration(duration);
            lastTimelineS->setRepeat(rep);
        }
    }
}

/**
* @brief   Get segmentID
* @retval  segmentId
*/
unsigned int DashMPDTimelineS::getSegmentId() const {
    return segmentId;
}

/**
* @brief   Get Duration
* @retval  Duration
*/
long long DashMPDTimelineS::getDuration() {
    return duration;
}

/**
* @brief   Get Repeat count
* @retval  Repeat count
*/
long long DashMPDTimelineS::getRepeat() {
    return repeat;
}

/**
* @brief   Get Time
* @retval  Time
*/
long long DashMPDTimelineS::getTime() {
    return time;
}

/**
 * @brief   Set or Unset Duration
 * @param   d Duration
 */
void DashMPDTimelineS::setDuration(long long d) {
    if (d == MPD_UNSET_LONG) {
        elem.removeAttribute("d");
        duration = 0;
    }
    else {
        elem.setAttribute("d", to_string(d));
        duration = d;
    }
}

/**
 * @brief   Set or Unset Repeat
 * @param   r Repeat
 */
void DashMPDTimelineS::setRepeat(long long r) {
    if (r == MPD_UNSET_LONG) {
        elem.removeAttribute("r");
        repeat = -1;
    }
    else {
        elem.setAttribute("r", to_string(r));
        repeat = r;
    }
}

/**
 * @brief   Set or Unset Time
 * @param   t time
 */
void DashMPDTimelineS::setTime(long long t) {
    SET_ATTRIBUTE_LONG("t", t, to_string(t))
    time = t;
}

/**
* @brief   Get SupplementalProperty schemeIdUri
* @retval  schemeIdUri
*/
std::string DashMPDSupplementalProperty::getSchemeIdUri() {
    if (schemeIdUri.empty()) {
	schemeIdUri = elem.attribute("schemeIdUri", "");
    }
    return schemeIdUri;
}

/**
* @brief   Get SupplementalProperty value
* @retval  value
*/
std::string DashMPDSupplementalProperty::getValue() {
    if (value.empty()) {
	value = elem.attribute("value", "");
    }
    return value;
}

/**
* @brief   Get Label language
* @retval  lang
*/
std::string DashMPDLabel::getLanguage() {
    if (lang.empty()) {
        lang = elem.attribute("lang", "");
    }
    return lang;
}

/**
* @brief   Get DashMPDRole schemeIdUri
* @retval  schemeIdUri
*/
std::string DashMPDRole::getSchemeIdUri() {
    if (schemeIdUri.empty()) {
	schemeIdUri = elem.attribute("schemeIdUri", "");
    }
    return schemeIdUri;
}

/**
* @brief   Get DashMPDRole value
* @retval  value
*/
std::string DashMPDRole::getValue() {
    if (value.empty()) {
	value = elem.attribute("value", "");
    }
    return value;
}

/**
* @brief   Get DashMPDAccessibility schemeIdUri
* @retval  schemeIdUri
*/
std::string DashMPDAccessibility::getSchemeIdUri() {
    if (schemeIdUri.empty()) {
	schemeIdUri = elem.attribute("schemeIdUri", "");
    }
    return schemeIdUri;
}

/**
* @brief   Get DashMPDAccessibility value
* @retval  value
*/
std::string DashMPDAccessibility::getValue() {
    if (value.empty()) {
	value = elem.attribute("value", "");
    }
    return value;
}

/**
 * @brief   extract Timeline
 * @param   timeline Dash MPD Segment Timeline
 * @param   timelineItems Timeline Items
 */
void extractTimeline(DashMPDSegmentTimeline &timeline, vector<TimelineItem> &timelineItems) {

    DomElement S = timeline.elem.firstChildElement("S");

    long long curTime = 0;
    while (!S.isNull()) {

        auto startTime = stoll(S.attribute("t", "0"));
        if (startTime > 0)
            curTime = startTime;

        auto dur = stoll(S.attribute("d", "0"));
        auto rep = stoll(S.attribute("r", "0"));
        for (int i = 0; i <= rep; i++) {
            TimelineItem item{curTime, dur};
            timelineItems.push_back(item);
            curTime += dur;
        }
        S = S.nextSiblingElement("S");
    }
}


/**
 * @brief   adds Period at specific index
 * @param   period shared pointer to Period
 * @retval  period
 */
shared_ptr<DashMPDPeriod> DashMPDRoot::addPeriodAt(const shared_ptr<DashMPDPeriod> &period, int index)
{
    std::unique_lock<std::mutex> lock(mBlockPeriodsUpdateMutex);
    getPeriods();
    shared_ptr<DashMPDPeriod> _period = nullptr;
    if(index >= 0 && (index < periods.size())) {
        auto ePeriod = period->elem.cloneAt(elem, index);
        _period = addChild<DashMPDPeriod>(ePeriod);
        periods.insert(periods.begin() + index, _period);
        // sync index
        for (int i = 0; i < periods.size(); i++) {
            periods[i]->index = i;
            AAMPLOG_TRACE("Index updated as %d for period:%s", i,periods[i]->getId().c_str());
        }
    }
    else {
        lock.unlock();
        _period = addPeriod(period);
        lock.lock();
    }

    return _period;
}
