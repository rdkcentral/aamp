/*
 * Custom XML Adapter for parsing custom media list XML and building a timeline object
 */
#ifndef CUSTOM_XML_ADAPTER_HPP
#define CUSTOM_XML_ADAPTER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "turbo_xml.hpp"
#include "gst-utils.h" // Assuming this contains MediaType enum and other necessary definitions


// CustomTimelineEntry holds info for each media segment or init
struct CustomTimelineEntry {
    std::string filename;
    bool isInitialization;
    int chunkSize;
    MediaType mediaType; // eMEDIATYPE_VIDEO or eMEDIATYPE_AUDIO
    // Add more fields as needed from your custom XML

    CustomTimelineEntry()
        : filename(""),
          isInitialization(false),
          chunkSize(std::numeric_limits<int>::max()),
          mediaType(eMEDIATYPE_AUDIO) // Assuming eMEDIATYPE_AUDIO is defined in gst-utils.h
    {}
};

// CustomTimeline holds the parsed timeline from the custom XML
class CustomTimeline {
public:
    std::vector<CustomTimelineEntry> entries;
    double start; // Start time in seconds
    double duration; // Total duration in seconds
    // Add more attributes as needed

    void Debug() const;
};

// Parse the custom XML and fill the timeline object
CustomTimeline parseCustomXml(const char* ptr, size_t size, const std::string &url);
void SetStartAndDuration(CustomTimeline &timeline);

#endif // CUSTOM_XML_ADAPTER_HPP
