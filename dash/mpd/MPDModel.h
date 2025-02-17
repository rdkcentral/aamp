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
// MPDModel.h


/**
 * This file declares classes related to the MPD model representation. The models should
 * follow the ISO standard regarding to variable resolutions.
 *
 * For consistency, the files in this model should be independent of the rest of Fog project. Complex
 * algorithms should be placed in Utils.
 */

/**
 * @file MPDModel.h
 * @brief
 */

#ifndef FOG_CLI_DASHMODEL_H
#define FOG_CLI_DASHMODEL_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cfloat>
#include <climits>
#include <stdexcept>
#include <mutex>
#include "../xml/DomElement.h"
#include "../xml/DomNode.h"

// #include "AampLogManager.h"
#include "AampConfig.h"


// Be compatible with macOS
#ifndef LONG_LONG_MIN
#define LONG_LONG_MIN LLONG_MIN
#endif

class DashMPDAdaptationSet;

class DashMPDRepresentation;

class DashMPDEventStream;

class DashMPDEvent;

using std::string;
using std::shared_ptr;

#define MPD_UNSET_STRING "$$UNSET$$"
#define MPD_UNSET_DOUBLE (-DBL_MAX)
#define MPD_UNSET_LONG LONG_LONG_MIN
#define FOG_INTERRUPTED_PERIOD_SUFFIX "FogPeriod-"

// Must implement logging
//void AAMPLOG_ERR(const char *fmt, ...);
//void AAMPLOG_INFO(const char *fmt, ...);

/**
 * @class   DashMPDAny
 * @brief   DashMPDAny
 */
class DashMPDAny : public std::enable_shared_from_this<DashMPDAny> {
    virtual void dummy() {};

public:
	virtual ~DashMPDAny(){}
	
protected:
    const double UNDEFINED_DOUBLE = MPD_UNSET_DOUBLE + 1;
    const long long UNDEFINED_LONG = MPD_UNSET_LONG + 1;
    const string UNDEFINED_STRING = "$$UNDEFINED$$";

    template<class Parent>
    shared_ptr<Parent> getParent() {
        // first cast parent as element
        shared_ptr<Parent> dashElem = std::dynamic_pointer_cast<Parent>(shared_from_this());
        return dashElem;
    }
};


/**
 * @class   IncorrectTagException
 * @brief   Incorrect Tag Exception
 */
class IncorrectTagException : public std::runtime_error {

public:
    explicit IncorrectTagException(const std::string &__arg) : runtime_error(__arg) {}
};

/**
 * @class   ElemVector
 * @brief   Element Vector
 */
class ElemVector : public std::vector<DomElement> {

public:
    ElemVector(DomElement parent, string childName);
};

/**
 * @class   DashMPDElement
 * @brief   Dash MPD Element
 */
template<class Parent>
class DashMPDElement : public DashMPDAny {
public:
    DomElement elem;
    std::weak_ptr<Parent> parent;
    int index;

    typedef Parent ParentType;
protected:


    DashMPDElement(std::shared_ptr<Parent> parent, DomElement elem, int index) : elem(elem), parent(parent),
                                                                                 index(index) {}

    template<typename Child>
    void getChildren(std::vector<std::shared_ptr<Child>> &out, string childName) {
        auto self = std::dynamic_pointer_cast<typename Child::ParentType>(shared_from_this());
        ElemVector elemVector(elem, childName);
        int idx = 0;
        for (auto e : elemVector) {
            auto child = std::shared_ptr<Child>(new Child(self, e, idx++));
            out.push_back(child);
        }
    }

    template<class Child>
    shared_ptr<Child> getFirstChild(const string &childName) {
        auto self = std::dynamic_pointer_cast<typename Child::ParentType>(shared_from_this());
        auto childElem = elem.firstChildElement(childName);
        auto child = std::shared_ptr<Child>(new Child(self, childElem, 0));
        return child;
    }

    template<class Child>
    shared_ptr<Child> addChild(DomElement childElem) {
        auto self = std::dynamic_pointer_cast<typename Child::ParentType>(shared_from_this());
        auto child = std::shared_ptr<Child>(new Child(self, childElem, 0));
        return child;
    }

    template<class Child>
    shared_ptr<Child> addChild(const string &childName) {
        auto self = std::dynamic_pointer_cast<typename Child::ParentType>(shared_from_this());
        auto childElem = elem.addChildElement(childName);
        auto child = std::shared_ptr<Child>(new Child(self, childElem, 0));
        return child;
    }

    template<class Child>
    shared_ptr<Child> addChild(const string &childName, unsigned int id) {
        auto self = std::dynamic_pointer_cast<typename Child::ParentType>(shared_from_this());
        auto childElem = elem.addChildElement(childName);
        auto child = std::shared_ptr<Child>(new Child(self, childElem, 0, id));
        return child;
    }

protected:
    template<typename Child>
    void removeChild(shared_ptr<Child> &child) {
        elem.removeChild(child->elem);
    }

    void removeChildren(const string &elementName) {
        auto eChild = elem.firstChildElement(elementName);
        while (!eChild.isNull()) {
            elem.removeChild(eChild);
            eChild = elem.firstChildElement(elementName);
        }
    }

public:

    /**
     * Removes a child element.
     *
     * Note: when calling this method you must make sure any cached instance is removed as well.
     */
    void removeChild(const string &elementName) {
        auto eChild = elem.firstChildElement(elementName);
        if (!eChild.isNull()) elem.removeChild(eChild);
    }

    /**
     * Manually sets a string attribute. If value == MPD_UNSET_VALUE, removes the attribute.
     *
     * Note: when calling this method you must make sure any cached value is remove as well.
     */
    void setAttribute(const string &attr, const string &value) {
        if (value == MPD_UNSET_STRING) elem.removeAttribute(attr);
        else elem.setAttribute(attr, value);
    }

    bool isNull() { return elem.isNull(); }

    std::string getTag() {
        if (elem.isNull()) return "";
        return elem.tagName();
    }

    std::string getText();

    void setText(std::string text);

};

template<class Parent>
std::string DashMPDElement<Parent>::getText() {
    return elem.text();
}


template<class Parent>
void DashMPDElement<Parent>::setText(std::string text) {
    elem.setText(text);
}

/**
 * @class   DashMPDBaseURL
 * @brief   Dash MPD Base URL
 */
class DashMPDBaseURL : public DashMPDElement<DashMPDAny> {
public:
    DashMPDBaseURL(const shared_ptr<DashMPDAny> &parent, const DomElement &elem, int index)
            : DashMPDElement(parent, elem, index) {}

};

/**
 * @class   DashMPDSupplementalProperty
 * @brief   Dash MPD Supplemental Property
 */
class DashMPDSupplementalProperty : public DashMPDElement<DashMPDAny> {

    std::string value;
    std::string schemeIdUri;
public:
    DashMPDSupplementalProperty(const shared_ptr<DashMPDAny> &parent, const DomElement &elem, int index)
            : DashMPDElement(parent, elem, index),value(),schemeIdUri() {
		if (elem.isNull()) return;
		if (elem.tagName() != "SupplementalProperty") {
		    AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
		}
	    }

    std::string getSchemeIdUri();
    std::string getValue();
};

/**
 * @class   DashMPDLabel
 * @brief   Dash MPD Label
 */
class DashMPDLabel : public DashMPDElement<DashMPDAny> {

    std::string lang;
public:
    DashMPDLabel(const shared_ptr<DashMPDAny> &parent, const DomElement &elem, int index)
            : DashMPDElement(parent, elem, index),lang() {
		if (elem.isNull()) return;
		if (elem.tagName() != "Label") {
		    AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
		}
	    }

    std::string getLanguage();
};

/**
 * @class   DashMPDRole
 * @brief   Dash MPD Role
 */
class DashMPDRole : public DashMPDElement<DashMPDAny> {

    std::string value;
    std::string schemeIdUri;
public:
    DashMPDRole(const shared_ptr<DashMPDAny> &parent, const DomElement &elem, int index)
            : DashMPDElement(parent, elem, index),value(),schemeIdUri() {
		if (elem.isNull()) return;
		if (elem.tagName() != "Role") {
		    AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
		}
	    }

    std::string getSchemeIdUri();
    std::string getValue();
};

/**
 * @class   DashMPDAccessibility
 * @brief   Dash MPD Accessibility
 */
class DashMPDAccessibility : public DashMPDElement<DashMPDAny> {

    std::string value;
    std::string schemeIdUri;
public:
    DashMPDAccessibility(const shared_ptr<DashMPDAny> &parent, const DomElement &elem, int index)
            : DashMPDElement(parent, elem, index),value(),schemeIdUri() {
		if (elem.isNull()) return;
		if (elem.tagName() != "Accessibility") {
		    AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
		}
	    }

    std::string getSchemeIdUri();
    std::string getValue();
};

class DashMPDPeriod;

// Represents a root MPD element of the manifest
/**
 * @class   DashMPDRoot
 * @brief   Represents a root MPD element of the manifest
 */
class DashMPDRoot : public DashMPDElement<DashMPDAny> {

    std::vector<std::shared_ptr<DashMPDPeriod>> periods;

    std::vector<std::shared_ptr<DashMPDSupplementalProperty>> supplementalProperties;

    double _fetchTimeCache = UNDEFINED_DOUBLE;

public:

    DashMPDRoot(DomElement elem, const int &index) : DashMPDElement(std::shared_ptr<DashMPDAny>(), elem, index),
                                                        periods(),supplementalProperties(),mBlockPeriodsUpdateMutex() {
        if (elem.isNull()) return;
        if (elem.tagName() != "MPD") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
        }
    }

    std::string getBaseUrlValue();

    shared_ptr<DashMPDBaseURL> setBaseURLValue(const string &value);

    /**
     * Take this mutex when adding or removing a period, or when we need to block this.
     */
    std::mutex mBlockPeriodsUpdateMutex;

    /**
     * Returns the Location of this MPD. Clients should generally always specify this value for proper resolution
     * of BaseURL.
     */
    std::string getLocation();

    /**
     * Sets the location of the MPD file. Clients should generally always specify this value for proper resolution
     */
    void setLocation(string location);

    /**
     * When this manifest was fetched. (in seconds since epoch)
     *
     * Represent the "FetchTime" variable in the ISO documents, but it's not part of the
     * ISO manifest data. We're adding as an extra argument to make sure all relevant state
     * is serialized along the document.
     */
    void setFetchTime(double fetchTime);

    /**
     * When this manifest was fetched. (in seconds since epoch)
     *
     * Represent the "FetchTime" variable in the ISO documents, but it's not part of the
     * ISO manifest data. We're adding as an extra argument to make sure all relevant state
     * is serialized along the document.
     */
    double getFetchTime();

    /**
     * Insert a Period tree into the MPD as first item
     */
    void insertPeriodBefore(const shared_ptr<DashMPDPeriod> &period);

    std::vector<std::shared_ptr<DashMPDPeriod>> getPeriods();

    std::vector<std::shared_ptr<DashMPDSupplementalProperty>> getSupplementalProperties();

    double getAvailabilityStartTime();

    void setAvailabilityStartTime(double availStartTime);

    double getPublishTime();

    void setPublishTime(double epochSeconds);

    double getAvailabilityEndTime();

    double getMinimumUpdatePeriod();

    void setMinimumUpdatePeriod(double period);

    double getMaxSegmentDuration();

    bool isDynamic();

    void setDynamic(bool dynamic);

    double getSuggestedPresentationDelay();

    std::string getLocalRelativePathFromURL(std::string &targetUrl_);

    void setRelativeBaseUrlElement(DashMPDAny &e, std::string parentUrl_);

    double getTimeShiftBufferDepth();

    void setTimeShiftBufferDepth(double tsbDepth);

    void setAvailabilityEndTime(double epochSeconds);

    double getMediaPresentationDuration();

    void setMediaPresentationDuration(double mediaDuration);

    void removePeriod(shared_ptr<DashMPDPeriod> &period);

    shared_ptr<DashMPDPeriod> findPeriod(const shared_ptr<DashMPDPeriod> &period);

    shared_ptr<DashMPDPeriod> findPeriod(const string &periodId);

    shared_ptr<DashMPDPeriod> addPeriod(const shared_ptr<DashMPDPeriod> &period);

    shared_ptr<DashMPDPeriod> addPeriodAt(const shared_ptr<DashMPDPeriod> &period, int index = -1);

    shared_ptr<DashMPDSupplementalProperty> addSupplementalProperty(const shared_ptr<DashMPDSupplementalProperty> &supProp);
};

class DashMPDTimelineS;

/**
 * @class   DashMPDSegmentTimeline
 * @brief   Dash MPD Segment Timeline
 */
class DashMPDSegmentTimeline : public DashMPDElement<DashMPDAny> {
    std::vector<std::shared_ptr<DashMPDTimelineS>> timeLineSegments;
    unsigned int timeLineId;
public:
    DashMPDSegmentTimeline(const std::shared_ptr<DashMPDAny> &parent, DomElement &elem, const int &index)
            : DashMPDElement(parent, elem, index),timeLineSegments(),timeLineId(0) {
        if (elem.isNull()) return;
        if (elem.tagName() != "SegmentTimeline") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
            return;
        }
        timeLineId = 0;
    }

    /**
     * Appends a new "S" element to SegmentTimeline
     */
    shared_ptr<DashMPDTimelineS> appendSegment();

    std::vector<std::shared_ptr<DashMPDTimelineS>>& getTimeLineSegments();

    void removeTimeLine(std::shared_ptr<DashMPDTimelineS> &timeLine);

    void mergeTimeline(const shared_ptr<DashMPDSegmentTimeline> &timeline);
};

/**
 * @class   DashMPDTimelineS
 * @brief   Dash MPD Time lineS
 */
class DashMPDTimelineS : public DashMPDElement<DashMPDSegmentTimeline> {
private:
    unsigned int segmentId;
    long long time;
    long long duration;
    long long repeat;
public:
    DashMPDTimelineS(const shared_ptr<DashMPDSegmentTimeline> &parent, const DomElement &elem, int index, unsigned int segmentId = 0)
            : DashMPDElement(parent, elem, index),segmentId(),time(),duration(),repeat() {
        this->segmentId = segmentId;
        time = -1;
        duration = -1;
        repeat = -1;
        if (elem.isNull()) return;
        if (elem.tagName() != "S") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
            return;
        }
    }

    void setDuration(long long d);

    void setRepeat(long long r);

    void setTime(long long t);

    long long getDuration();

    long long getRepeat();

    long long getTime();

    unsigned int getSegmentId() const;

    bool operator==(const DashMPDTimelineS &b) const {
        return getSegmentId() == b.getSegmentId();
    }
};

/**
 * @class   DashMPDURLType
 * @brief   Dash MPD URL Type
 */
class DashMPDURLType : public DashMPDElement<DashMPDAny> {
public:
    DashMPDURLType(const shared_ptr<DashMPDAny> &parent, const DomElement &elem,
                   int index) : DashMPDElement(parent, elem, index) {}

    string getSourceURL();

    void setSourceURL(const string &source);

    string getRange();

    void setRange(const string &range);
};

/**
 * @class   DashMPDSegmentBase
 * @brief   Dash MPD Segment Base
 */
class DashMPDSegmentBase : public DashMPDElement<DashMPDAny> {
private:
    shared_ptr<DashMPDURLType> initialization;
public:
    DashMPDSegmentBase(const std::shared_ptr<DashMPDAny> &parent, DomElement &elem, const int &index)
            : DashMPDElement(parent, elem, index),initialization() {}

    long long int getTimeScale();

    void setTimeScale(long long int timescale);

    long long getPresentationTimeOffset();

    void setPresentationTimeOffset(long long offset);

    shared_ptr<DashMPDURLType> getInitialization();

    void setInitialization(const string &source, const string &range = MPD_UNSET_STRING);
};

/**
 * @class   DashMPDMultipleSegmentBase
 * @brief   Dash MPD Multiple Segment Base
 */
class DashMPDMultipleSegmentBase : public DashMPDSegmentBase {
    shared_ptr<DashMPDSegmentTimeline> segmentTimeline;
public:

    DashMPDMultipleSegmentBase(const std::shared_ptr<DashMPDAny> &parent, DomElement &elem, const int &index)
            : DashMPDSegmentBase(parent, elem, index),segmentTimeline() {}

    shared_ptr<DashMPDSegmentTimeline> getSegmentTimeline();

    /**
     * Creates a new SegmentTimeline element, removing any existing one.
     */
    shared_ptr<DashMPDSegmentTimeline> createSegmentTimeline();

    long long int getDuration();

    void setDuration(long long int duration);

    long long int getStartNumber();

    void setStartNumber(long long number);
};

class DashMPDSegmentList;

/**
 * @class   DashMPDSegmentURL
 * @brief   Dash MPD Segment URL
 */
class DashMPDSegmentURL : public DashMPDElement<DashMPDSegmentList> {
    std::string segmentPath;
public:
    DashMPDSegmentURL(const shared_ptr<DashMPDSegmentList> &parent, const DomElement &elem, int index)
            : DashMPDElement(parent, elem, index),segmentPath() {
        if (elem.isNull()) return;
        if (elem.tagName() != "SegmentURL") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
            return;
        }
    }

    std::string getMedia() const;

    void setMedia(std::string media);

    std::string getMediaRange();

    std::string getSegmentPath();

    void setSegmentPath(std::string path);

    bool operator==(const DashMPDSegmentURL &b) const {
        return getMedia() == b.getMedia();
    }
};

/**
 * @class   DashMPDSegmentList
 * @brief   Dash MPD Segment List
 */
class DashMPDSegmentList : public DashMPDMultipleSegmentBase {
    std::vector<shared_ptr<DashMPDSegmentURL>> segmentURLs;
public:
    DashMPDSegmentList(const shared_ptr<DashMPDAny> &parent, DomElement &elem, const int &index)
            : DashMPDMultipleSegmentBase(parent, elem, index),segmentURLs() {}

    std::vector<shared_ptr<DashMPDSegmentURL>>& getSegmentURLs();

    int getSegmentURLCount();

    shared_ptr<DashMPDSegmentURL> appendSegmentURL();

    void removeSegmentUrl(shared_ptr<DashMPDSegmentURL>& segUrl);
};

// Represents a SegmentTemplate anywhere in the manifest
/**
 * @class   DashMPDSegmentTemplate
 * @brief   Represents a SegmentTemplate anywhere in the manifest
 */
class DashMPDSegmentTemplate : public DashMPDMultipleSegmentBase {
    long long startNumber;
    long long lastSegNumber; //Added to handle scenarios where fragments can be missed due to bad network
public:

    DashMPDSegmentTemplate(const std::shared_ptr<DashMPDAny> &parent, DomElement &elem, const int &index)
            : DashMPDMultipleSegmentBase(parent, elem, index),startNumber(0),lastSegNumber(0) {
        startNumber = -1;
        lastSegNumber = -1;
        if (elem.isNull()) return;
        if (elem.tagName() != "SegmentTemplate") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
            return;
        }
    }

    long long getStartNumber();

    void setStartNumber(long long num);

    long long getLastSegNumber();

    void setLastSegNumber(long long num);

    string getMedia();

    void setMedia(const string &value);

    // deletes segments created after <para>time</para>
    void adjustCutoff(double time);

    string getInitializationAttr();

    void setInitializationAttr(const string &value);

    void mergeSegmentTemplate(const shared_ptr<DashMPDSegmentTemplate> &segmentTemplate);

};


// Represents a Period in the manifest
/**
 * @class   DashMPDPeriod
 * @brief   Represents a Period in the manifest
 */
class DashMPDPeriod : public DashMPDElement<DashMPDRoot> {

    std::vector<std::shared_ptr<DashMPDAdaptationSet>> adaptationSets;

    // A map. getAdaptationSetKey() --> index
    std::unordered_map<std::string, int> adaptationSetsKeyToIndexMap;

    // cached periodStart value
    double _periodStart = UNDEFINED_DOUBLE;

    // cached periodDuration value
    double _duration = UNDEFINED_DOUBLE;

    std::vector<std::shared_ptr<DashMPDSegmentList>> segmentLists;

    bool _parsedSegmentLists = false;

    void updateAdaptationSetsKeyToIndexMap();

public:

    DashMPDPeriod(const std::shared_ptr<DashMPDRoot> &parent, DomElement &elem, const int &index)
            : DashMPDElement(parent, elem, index),adaptationSets(),adaptationSetsKeyToIndexMap(),segmentLists() {
        if (elem.isNull()) return;
        if (elem.tagName() != "Period") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
        }
    }

    /**
     * Calculate the PeriodStart as specified in 5.3.2 of the spec.
     */
    double getStart();

    /**
     * Set the start attribute of the period
     */
    void setStart(double start);

    double getEnd();


    /**
     * Period duration in seconds
     */
    double getDuration();

    /**
     * Set the duration attribute
     */
    void setDuration(double duration);

    /**
    *   @brief  Set the period ID
    *
    *   @param[in] string id - id to be set 
    *   @return None
    */
    void setId(const string &id);

    bool isActive(double time);

    std::string getBaseUrl();

    std::shared_ptr<DashMPDSegmentTemplate> getSegmentTemplate();

    std::vector<std::shared_ptr<DashMPDAdaptationSet>> getAdaptationSets();

    void removeAdaptationSet(shared_ptr<DashMPDAdaptationSet> &adaptationSet);

    std::string getId() const;

    std::string getPeriodKey() const;

    std::vector<std::shared_ptr<DashMPDSegmentList>> getSegmentLists();

    /**
     * Remove all child SegmentList elements
     */
    void removeSegmentLists();

    void removeSegmentDetails();

    void dumpPeriodBoundaries();

    /**
     * @brief Update period start time based on PTS offset
     */
    void updateStartTimeToPTSOffset();

    /**
     * @brief Update period start time based on PTS offset
     */
    void updateStartTime();

    std::shared_ptr<DashMPDEventStream> getEventStream();

    void setEventStream(std::shared_ptr<DashMPDEventStream> &eventStream);

    bool operator==(const DashMPDPeriod &b) const {
        return getPeriodKey() == b.getPeriodKey();
    }

    /**
     * Merge a period tree content into an existing period
     */
    void mergePeriod(const shared_ptr<DashMPDPeriod> &period);

    shared_ptr<DashMPDAdaptationSet> findAdaptationSet(const shared_ptr<DashMPDAdaptationSet> & adaptationSet);

    /**
     * @brief Checking whether current period is VSS period or not
     * @retval bool true if vss period, else false
     */
    bool isVssEarlyAvailablePeriod();

    /**
     * @brief Check whether period is added by Fog as a duplicate period
     * @retval bool true if fog duplicate period, else false
     */
    bool isDuplicatePeriod();
};

/**
* @class   DashMPDEvent
* @brief   Represents scte35 events published in mpd manifest
*/
class DashMPDEvent : public DashMPDElement<DashMPDEventStream> {
    long long presentationTime;
    long long duration;
    std::string eventData;
public:
    DashMPDEvent(const std::shared_ptr<DashMPDEventStream> &parent, DomElement &elem, const int &index)
        : DashMPDElement(parent, elem, index),presentationTime(0),duration(0),eventData() {
        if (elem.isNull()) return;
        if (elem.tagName() != "Event") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
        }
        presentationTime = -1;
        duration = -1;
    }

    long long getPresentationTime();
    long long getDuration();
    std::string getEventData();
};


/**
* @class   DashMPDEventStream
* @brief   Represents EventStream  published in mpd manifest
*/
class DashMPDEventStream : public DashMPDElement<DashMPDPeriod> {

    std::vector<shared_ptr<DashMPDEvent>> events;
    long long timeScale;
    std::string schemeIdUri;

public:
    DashMPDEventStream(const std::shared_ptr<DashMPDPeriod> &parent, DomElement &elem, const int &index)
        : DashMPDElement(parent, elem, index),events(),timeScale(),schemeIdUri() {
        if (elem.isNull()) return;
        if (elem.tagName() != "EventStream") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
        }
        timeScale = 0;
    }

    long long getTimeScale();

    std::vector<shared_ptr<DashMPDEvent>>& getEvents();

    std::string getSchemeIdUri();
};


// Represents an AdaptationSet in the manifest
/**
 * @class   DashMPDAdaptationSet
 * @brief   Represents an AdaptationSet in the manifest
 */
class DashMPDAdaptationSet : public DashMPDElement<DashMPDPeriod> {

    std::vector<std::shared_ptr<DashMPDRepresentation>> representations;
    std::unordered_map<std::string, int> representationsKeyToIndexMap;

    std::shared_ptr<DashMPDSegmentTemplate> segmentTemplate;
    std::vector<std::shared_ptr<DashMPDSegmentList>> segmentLists;
    std::vector<std::shared_ptr<DashMPDRole>> roles;
    std::vector<std::shared_ptr<DashMPDAccessibility>> accessibility;
    std::vector<std::shared_ptr<DashMPDLabel>> labels;
    std::vector<std::shared_ptr<DashMPDSupplementalProperty>> supplementalProperties;
    bool _parsedSegmentLists = false;

    void updateRepresentationsKeyToIndexMap();
public:
    DashMPDAdaptationSet(const std::shared_ptr<DashMPDPeriod> &parent, DomElement &elem, const int &index)
            : DashMPDElement(parent, elem, index),representations(),representationsKeyToIndexMap(),segmentTemplate(),segmentLists(),
                                                    roles(),accessibility(), labels(),supplementalProperties() {
        if (elem.isNull()) return;
        if (elem.tagName() != "AdaptationSet") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
        }
    }

    // If fromChildren is true, then it won't get the segment template from its parent.
    std::shared_ptr<DashMPDSegmentTemplate> getSegmentTemplate(bool fromChildren = false, bool cached = true);

    std::string getBaseUrl();

    string getMimeType();

    string getLanguage();

    std::shared_ptr<DashMPDSegmentTemplate>  newSegmentTemplate();

    std::vector<std::shared_ptr<DashMPDRepresentation>> getRepresentations();

    std::vector<std::shared_ptr<DashMPDRole>> getRoles();

    std::vector<std::shared_ptr<DashMPDAccessibility>> getAccessibility();

    std::vector<std::shared_ptr<DashMPDLabel>> getLabels();

    std::vector<std::shared_ptr<DashMPDSupplementalProperty>> getSupplementalProperties();

    string getContentType();

    string getLabel();

    // If the Adaptation Set has as "Role" element with "primary" or "main" value
    bool isRolePrimary();
    
    bool isIframeTrack();

    bool isTextTrack();

    /**
    *   @brief  Check if this adaptationSet is identical to another.
    *           Checks are made comparing initialization, base url and media urls
    *           The initialization headers should be different if there's any change
    *           in content protection, that's why we are not comparing content protection.
    *   @param[in] adaptationSet - shared pointer to the
    *              Adaptation Set to compare with.
    *   @return bool - true if adaptationSets are Identical, false otherwise
    */
    bool isIdenticalAdaptionSet(const std::shared_ptr<DashMPDAdaptationSet>& adaptationSet);

    /**
    *   @brief   Get the initialization source url for this Adaptation set
    *   @return  string - returns the initialization url
    */
    std::string getInitUrl();

    std::string getAdaptationSetKey() const {
        string adaptationSetKey;
        auto parent = this->parent.lock();
        if(parent) {
            adaptationSetKey =  parent->getPeriodKey() + "-" + elem.attribute("id",std::to_string(index));
        }
        return adaptationSetKey;
    }

    /**
     * @brief   Get id from "id" element attribute
     * @retval  id
     */
    std::string getId() const;

    std::vector<std::shared_ptr<DashMPDSegmentList>> getSegmentLists();

    /**
     * Remove all child SegmentList elements
     */
    void removeSegmentLists();

    bool getSegmentAlignment();

    void removeRepresentation(shared_ptr<DashMPDRepresentation> &repr);

    bool operator==(const DashMPDAdaptationSet &b) const {
        return getAdaptationSetKey() == b.getAdaptationSetKey();
    }

    shared_ptr<DashMPDBaseURL> setBaseURLValue(std::string value);

    void mergeAdaptationSet(const shared_ptr<DashMPDAdaptationSet> &adaptationSet);

    /**
    *   @brief  Get the number of segments available in this adaptation set
    *
    *   @return int - count of segments in the adaptation set
    */
    int getSegmentCount();

    long long getSegmentStartTime();

    shared_ptr<DashMPDRepresentation> addRepresentation(shared_ptr<DashMPDRepresentation> &repr);

};
// Represents a Representation in the manifest
/**
 * @class   DashMPDRepresentation
 * @brief   Represents a Representation in the manifest
 */
class DashMPDRepresentation : public DashMPDElement<DashMPDAdaptationSet> {

    mutable string representationKey;

    std::shared_ptr<DashMPDSegmentTemplate> segmentTemplate;

    std::vector<std::shared_ptr<DashMPDSegmentList>> segmentLists;

    std::vector<std::shared_ptr<DashMPDSupplementalProperty>> supplementalProperties;

public:
    DashMPDRepresentation(const std::shared_ptr<DashMPDAdaptationSet> &parent, DomElement &elem, const int &index)
            : DashMPDElement(parent, elem, index),representationKey(),segmentTemplate(),segmentLists(),supplementalProperties() {
        if (elem.isNull()) return;
        if (elem.tagName() != "Representation") {
            AAMPLOG_ERR("%s", ("IncorrectTagException:" + elem.tagName()).c_str());
        }
    };

public:
    // If fromChildren is true, then it won't get the segment template from its parent.
    std::shared_ptr<DashMPDSegmentTemplate> getSegmentTemplate(bool fromChildren = false, bool cached = true);

    std::vector<shared_ptr<DashMPDSegmentList>> getSegmentLists();

    std::shared_ptr<DashMPDSegmentBase> getSingleSegmentBase();

    std::string getBaseUrl();

    /**
     * @brief   set BaseURL
     * @param   Base URL
     * @retval  Dash MPD Base URL
     */
    shared_ptr<DashMPDBaseURL> setBaseURLValue(std::string value);

    long long int getBandwidth();
    
    int getWidth();

    int getHeight();

    string getId() const;

    string getMimeType();

    // A unique key identifying the representation this segment belongs must match
    // DashSegment::getRepresentationKey
    string getRepresentationKey() const;

    /**
     * Creates a SegmentList element and appends to Representation
     */
    std::shared_ptr<DashMPDSegmentList> appendSegmentList();

    /**
    * Creates a SegmentTemplate element and appends to Representation
    */
    std::shared_ptr<DashMPDSegmentTemplate> appendSegmentTemplate();

    // Look for startWithSAP attribute in representation and in parent
    int getStartWithSAP();

    /**
     * Remove all child SegmentList elements
     */
    void removeSegmentLists();

    // Equality operator
    bool operator==(const DashMPDRepresentation &b) const {
        return getRepresentationKey() == b.getRepresentationKey();
    }

    /**
     * Get "codecs" attribute in hierarchy
     */
    std::string getCodecs();

    /**
     * Set "codecs" attribute on element. This effectively remove the attribute if present in the
     * in the AdaptationSet.
     */
    void setCodecs(const string &value);

    void mergeRepresentation(const shared_ptr<DashMPDRepresentation> &representation);

    std::vector<std::shared_ptr<DashMPDSupplementalProperty>> getSupplementalProperties();

    shared_ptr<DashMPDSupplementalProperty> addSupplementalProperty(const shared_ptr<DashMPDSupplementalProperty> &supProp);
};

/**
 * @class   DashMPDPeriod
 * @brief   The MPD document
 */
class DashMPDDocument {
    // The MPD document
    shared_ptr<DomDocument> xmlDoc;
    shared_ptr<DashMPDRoot> root;

public:
    explicit DashMPDDocument(const string &content);

    explicit DashMPDDocument( shared_ptr<DomDocument> xmlDoc) :xmlDoc(xmlDoc),root() {};
    bool isValid();

    shared_ptr<DashMPDRoot> getRoot();

    shared_ptr<DashMPDDocument> clone(bool deep);

    string toString();

    void mergeDocuments(shared_ptr<DashMPDDocument> &newDocument);

};

/**
 * Represents a "decoded" DASH Segment instance.
 *
 * These instances are not materialized in the MPD but are created on-the-fly based on the current state of
 * MPD document using a "MPDSegmenter".
 */
/**
 * @class   MPDSegment
 * @brief   Represents a "decoded" DASH Segment instance
 */
class MPDSegment {
public:

    /**
     * The Representation this Segment is for.
     */
    string representationKey;

    /**
    * The AdaptationSet this Segment is from.
    */
    string adaptationKey;

    /**
    * The Period this Segment is from.
    */
    string periodId;

    /**
     * Indicate this is an initialization segment.
     */
    bool isInit = false;

    /**
     * The resolved absolute URL to download this segment.
     */
    string url;

    /**
     * The presentation start time. Init segments are assigned -1.
     */
    double startTime = -1;

    /**
     * When this segment is available in wall-clock time. Only relevant for dynamic streams.
     */
    double availabilityStartTime = 0;

    /**
     * When this segment cease to be available in wall-clock time. Only relevant for dynamic streams.
     */
    double availabilityEndTime = 0;

    /**
     * The duration of the segment (in seconds)
     */
    double duration = 0;

    /**
    * scaled duration of segment as in mpd
    */
    long long scaledDuration = 0;

    /**
     * If the client should only download a partial byte-range of the segment. Returned in
     * byte-range-spec format of RFC 2616 specification.
     */
    string rangeSpec;

    /**
     * The "Time" of the segment (in scaled values). Init segments are assigned -1
     */
    long long scaledStart = 0;

    /**
     * The "Number" of the segment (in scaled values). Init segments are assigned -1
     */
    long long number = 0;

    MPDSegment() : representationKey(), adaptationKey(), periodId(), isInit(false), url(), startTime(-1),
                   availabilityStartTime(0), availabilityEndTime(0), duration(0), scaledDuration(0), rangeSpec(),
                   scaledStart(0), number(0) {}
};


/**
 * @struct  TimelineItem
 * @brief   Stores Starttime and Duration
 */
struct TimelineItem {
    long long int startTime;
    long long int duration;
};

std::string findBaseUrl(DomElement &element, const std::string &current, bool isFile = false);

void extractTimeline(DashMPDSegmentTimeline &timeline, std::vector<TimelineItem> &timelineItems);

#endif //FOG_CLI_DASHMODEL_H
