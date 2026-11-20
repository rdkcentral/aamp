#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nmqe {

/**
 * One track of an MSF catalog, as published by the reference publication.
 * Only the fields the POC uses are modelled; anything else is recorded in
 * `unknownFields` so a catalog change is visible rather than silently ignored.
 * CMSF conformance is not assumed from CMAF packaging.
 */
struct CatalogTrack
{
	std::string name;
	std::string ns;                 // optional per-track namespace
	std::string packaging;          // "cmaf", an event timeline, ...
	std::string role;               // "video", "audio", ...
	std::string codec;              // RFC 6381 codec string
	std::string mimeType;
	int width = 0;
	int height = 0;
	double framerate = 0;
	int samplerate = 0;
	std::string channelConfig;
	int64_t bitrate = 0;
	std::vector<uint8_t> initData;  // decoded CMAF header (ftyp+moov) when published in the catalog
	std::string initRef;
	std::optional<int64_t> renderGroup;
	std::optional<int64_t> altGroup;
	bool isLive = false;
	int64_t targetLatencyMs = 0;
	std::vector<std::string> depends;
	std::vector<std::string> unknownFields;

	bool isMedia() const { return packaging == "cmaf"; }
};

struct Catalog
{
	int64_t version = 0;
	std::vector<CatalogTrack> tracks;
	std::vector<std::string> unknownFields;
	bool isDelta = false;           // MSF delta update; not supported by the POC

	const CatalogTrack *find(const std::string &name) const;
	/** The first CMAF track with `role`, or the named one when `name` is non-empty. */
	const CatalogTrack *select(const std::string &role, const std::string &name) const;

	/** Parses an MSF catalog JSON document. */
	static std::optional<Catalog> parse(const uint8_t *data, size_t size, std::string &error);
};

} // namespace nmqe
