#include "Catalog.h"

#include "common/Base64.h"

#include <cjson/cJSON.h>

#include <cstring>
#include <memory>
#include <set>

namespace nmqe {

namespace {

const std::set<std::string> kTrackFields = {
	"name", "namespace", "packaging", "role", "codec", "mimeType", "width", "height",
	"framerate", "samplerate", "channelConfig", "bitrate", "initData", "initRef",
	"renderGroup", "altGroup", "isLive", "targetLatency", "depends",
};
const std::set<std::string> kRootFields = {"version", "tracks"};
const std::set<std::string> kDeltaFields = {"deltaUpdate", "addTracks", "removeTracks", "cloneTracks"};

std::string getString(const cJSON *object, const char *key)
{
	const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	return cJSON_IsString(item) && item->valuestring ? item->valuestring : std::string();
}

std::optional<double> getNumber(const cJSON *object, const char *key)
{
	const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	if (cJSON_IsNumber(item)) return item->valuedouble;
	return std::nullopt;
}

} // namespace

const CatalogTrack *Catalog::find(const std::string &name) const
{
	for (const auto &track : tracks)
	{
		if (track.name == name) return &track;
	}
	return nullptr;
}

const CatalogTrack *Catalog::select(const std::string &role, const std::string &name) const
{
	if (!name.empty()) return find(name);
	for (const auto &track : tracks)
	{
		if (track.isMedia() && track.role == role) return &track;
	}
	return nullptr;
}

std::optional<Catalog> Catalog::parse(const uint8_t *data, size_t size, std::string &error)
{
	std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
		cJSON_ParseWithLength(reinterpret_cast<const char *>(data), size), cJSON_Delete);
	if (!root || !cJSON_IsObject(root.get()))
	{
		error = "catalog is not a JSON object";
		return std::nullopt;
	}

	Catalog catalog;
	catalog.version = static_cast<int64_t>(getNumber(root.get(), "version").value_or(0));
	for (const cJSON *field = root->child; field; field = field->next)
	{
		if (kDeltaFields.count(field->string)) catalog.isDelta = true;
		else if (!kRootFields.count(field->string)) catalog.unknownFields.emplace_back(field->string);
	}
	if (catalog.isDelta)
	{
		error = "MSF delta catalog updates are not supported";
		return catalog;
	}

	const cJSON *tracks = cJSON_GetObjectItemCaseSensitive(root.get(), "tracks");
	if (!cJSON_IsArray(tracks))
	{
		error = "catalog has no tracks array";
		return std::nullopt;
	}

	for (const cJSON *item = tracks->child; item; item = item->next)
	{
		if (!cJSON_IsObject(item)) continue;
		CatalogTrack track;
		track.name = getString(item, "name");
		track.ns = getString(item, "namespace");
		track.packaging = getString(item, "packaging");
		track.role = getString(item, "role");
		track.codec = getString(item, "codec");
		track.mimeType = getString(item, "mimeType");
		track.width = static_cast<int>(getNumber(item, "width").value_or(0));
		track.height = static_cast<int>(getNumber(item, "height").value_or(0));
		track.framerate = getNumber(item, "framerate").value_or(0);
		track.samplerate = static_cast<int>(getNumber(item, "samplerate").value_or(0));
		track.channelConfig = getString(item, "channelConfig");
		track.bitrate = static_cast<int64_t>(getNumber(item, "bitrate").value_or(0));
		track.initRef = getString(item, "initRef");
		if (auto group = getNumber(item, "renderGroup")) track.renderGroup = static_cast<int64_t>(*group);
		if (auto group = getNumber(item, "altGroup")) track.altGroup = static_cast<int64_t>(*group);
		track.isLive = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "isLive"));
		track.targetLatencyMs = static_cast<int64_t>(getNumber(item, "targetLatency").value_or(0));
		if (const cJSON *depends = cJSON_GetObjectItemCaseSensitive(item, "depends"); cJSON_IsArray(depends))
		{
			for (const cJSON *dep = depends->child; dep; dep = dep->next)
			{
				if (cJSON_IsString(dep)) track.depends.emplace_back(dep->valuestring);
			}
		}
		const std::string initData = getString(item, "initData");
		if (!initData.empty())
		{
			auto decoded = base64Decode(initData);
			if (!decoded)
			{
				error = "track " + track.name + " has malformed initData";
				return std::nullopt;
			}
			track.initData = std::move(*decoded);
		}
		for (const cJSON *field = item->child; field; field = field->next)
		{
			if (!kTrackFields.count(field->string)) track.unknownFields.emplace_back(field->string);
		}
		if (track.name.empty())
		{
			error = "catalog track without a name";
			return std::nullopt;
		}
		catalog.tracks.push_back(std::move(track));
	}
	return catalog;
}

} // namespace nmqe
