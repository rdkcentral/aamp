/*
 * Implementation of Custom XML Adapter for parsing custom media list XML
 */
#include "custom_xml_adapter.hpp"
#include <cstdio>
#include <cstring>

void CustomTimeline::Debug() const {
    printf("CustomTimeline: start=%f, duration=%f\n", start, duration);
    for (const auto& entry : entries) {
        printf("  file=%s, isInit=%d, chunkSize=%d mediaType=%d\n",
               entry.filename.c_str(), entry.isInitialization, entry.chunkSize, (int)entry.mediaType);
    }
}

CustomTimeline parseCustomXml(const char* ptr, size_t size, const std::string &url) {
    CustomTimeline timeline;
    XmlNode root("document", ptr, size);

    // Compute BaseURL from input url
    auto BaseURL = url.substr(0, url.find_last_of("/") + 1);

    // Find the <MediaList> node
    const XmlNode* mediaList = nullptr;
    for (const auto& child : root.children) {
        if (child->tagName == "MediaList") {
            mediaList = child;
            break;
        }
    }
    if (!mediaList) {
        printf("No <MediaList> found in custom XML\n");
        return timeline;
    }

    // Parse VideoList and AudioList
    double start = 0.0;
    for (const auto& listNode : mediaList->children) {
        if (listNode->tagName == "VideoList" || listNode->tagName == "AudioList") {
            listNode->hasAttribute("start") ? start = std::stod(listNode->getAttribute("start")) : start = 0.0;
            if(timeline.start < start) {
                // Set the maximum start time
                timeline.start = start;
                printf("Setting timeline start to: %f based on %s\n", timeline.start, listNode->tagName.c_str());
            }
            for (const auto& entryNode : listNode->children) {
                // Video or Audio entry
                if (entryNode->tagName == "Video" || entryNode->tagName == "Audio") {
                    CustomTimelineEntry entry;
                    entry.mediaType = (listNode->tagName == "VideoList") ? eMEDIATYPE_VIDEO : eMEDIATYPE_AUDIO;
                    // Attributes
                    if (entryNode->hasAttribute("isInitialization"))
                        entry.isInitialization = (entryNode->getAttribute("isInitialization") == "true" || entryNode->getAttribute("isInitialization") == "1");
                    if (entryNode->hasAttribute("chunkSize"))
                        entry.chunkSize = std::stoi(entryNode->getAttribute("chunkSize"));
                    // Filename is a child node
                    for (const auto& fileNode : entryNode->children) {
                        if (fileNode->tagName == "Filename") {
                            // Prepend BaseURL to filename if not already absolute
                            if (!fileNode->innerHTML.empty() && fileNode->innerHTML.find("://") == std::string::npos && fileNode->innerHTML[0] != '/') {
                                entry.filename = BaseURL + fileNode->innerHTML;
                            } else {
                                entry.filename = fileNode->innerHTML;
                            }
                            break;
                        }
                    }
                    timeline.entries.push_back(entry);
                }
            }
        }
    }
    return timeline;
}