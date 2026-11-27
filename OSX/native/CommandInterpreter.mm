/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#import <Cocoa/Cocoa.h>
#import "CommandInterpreter.h"
#import "PlayerEngine.h"
#undef __ACCESSIBILITY__ // Apple Accessibility.h uses the same guard as Accessibility.hpp
#include "main_aamp.h"
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using Args = std::string;
using Handler = std::function<std::string(PlayerInstanceAAMP &, const Args &)>;

struct Entry
{
	std::string usage;
	std::string help;
	Handler fn;
};
using Table = std::map<std::string, Entry>;

std::string Str(bool v) { return v ? "true" : "false"; }
std::string Str(const std::string &v) { return v; }
template <typename T> std::string Str(const std::vector<T> &v)
{
	std::string out;
	for (const auto &item : v)
	{
		out += (out.empty() ? "" : ", ") + std::to_string(item);
	}
	return "[" + out + "]";
}
template <typename T> std::string Str(T v) { return std::to_string(v); }

bool ParseBool(const std::string &a, bool &out)
{
	if (a == "on" || a == "true" || a == "1" || a == "yes") { out = true; return true; }
	if (a == "off" || a == "false" || a == "0" || a == "no") { out = false; return true; }
	return false;
}

std::string Trim(const std::string &s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return "";
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

// Splits "word rest of line" into the first word and the trimmed remainder.
void SplitFirst(const std::string &line, std::string &first, std::string &rest)
{
	std::string t = Trim(line);
	size_t sp = t.find_first_of(" \t");
	first = t.substr(0, sp);
	rest = (sp == std::string::npos) ? "" : Trim(t.substr(sp));
}

std::vector<std::string> Words(const std::string &s)
{
	std::istringstream in(s);
	std::vector<std::string> out;
	std::string w;
	while (in >> w) out.push_back(w);
	return out;
}

const char *kOk = "ok";
}

#define GET(NAME, EXPR) \
	_getters[NAME] = {"", "", [](PlayerInstanceAAMP &p, const Args &) -> std::string { return Str(EXPR); }}

#define SET_BOOL(NAME, CALL) \
	_setters[NAME] = {"on|off", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string { \
		bool v; if (!ParseBool(a, v)) return "usage: set " NAME " on|off"; p.CALL(v); return kOk; }}

#define SET_INT(NAME, CALL) \
	_setters[NAME] = {"<int>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string { \
		if (a.empty()) return "usage: set " NAME " <int>"; p.CALL(std::stoi(a)); return kOk; }}

#define SET_BPS(NAME, CALL) \
	_setters[NAME] = {"<bps>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string { \
		if (a.empty()) return "usage: set " NAME " <bps>"; p.CALL((BitsPerSecond)std::stoll(a)); return kOk; }}

#define SET_DBL(NAME, CALL) \
	_setters[NAME] = {"<number>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string { \
		if (a.empty()) return "usage: set " NAME " <number>"; p.CALL(std::stod(a)); return kOk; }}

#define SET_CSTR(NAME, CALL) \
	_setters[NAME] = {"<text>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string { \
		p.CALL(a.c_str()); return kOk; }}

#define SET_STR(NAME, CALL) \
	_setters[NAME] = {"<text>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string { \
		std::string copy(a); p.CALL(copy); return kOk; }}

@implementation CommandInterpreter
{
	__unsafe_unretained PlayerEngine *_engine;
	NSString *_logPath;
	Table _actions;
	Table _getters;
	Table _setters;
}

- (id)initWithEngine:(PlayerEngine *)engine logPath:(NSString *)logPath
{
	self = [super init];
	if (self)
	{
		_engine = engine;
		_logPath = [logPath copy];
		[self registerActions];
		[self registerGetters];
		[self registerSetters];
	}
	return self;
}

- (void)registerActions
{
	PlayerEngine *engine = _engine;
	NSString *logPath = _logPath;

	_actions["tune"] = {"<url> [contentType]", "tune and start playback",
		[engine](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			auto w = Words(a);
			if (w.empty()) return "usage: tune <url> [contentType]";
			if (w.size() > 1)
			{
				p.Tune(w[0].c_str(), true, w[1].c_str());
				return kOk;
			}
			[engine tune:[NSString stringWithUTF8String:w[0].c_str()]];
			return kOk;
		}};
	_actions["load"] = _actions["tune"];
	_actions["play"] = {"", "resume or restart the last URL",
		[engine](PlayerInstanceAAMP &, const Args &) -> std::string { [engine play]; return kOk; }};
	_actions["pause"] = {"", "pause playback",
		[engine](PlayerInstanceAAMP &, const Args &) -> std::string { [engine pause]; return kOk; }};
	_actions["stop"] = {"", "stop playback",
		[engine](PlayerInstanceAAMP &, const Args &) -> std::string { [engine stop]; return kOk; }};
	_actions["seek"] = {"<seconds>", "seek to position relative to tune time",
		[](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			if (a.empty()) return "usage: seek <seconds>";
			p.Seek(std::stod(a));
			return kOk;
		}};
	_actions["seekpaused"] = {"<seconds>", "seek and stay paused",
		[](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			if (a.empty()) return "usage: seekpaused <seconds>";
			p.Seek(std::stod(a), true);
			return kOk;
		}};
	_actions["live"] = {"", "seek to live edge",
		[](PlayerInstanceAAMP &p, const Args &) -> std::string { p.SeekToLive(); return kOk; }};
	_actions["rate"] = {"<rate>", "set trick-play rate (0 pause, 1 play, 2/4/16 ff, negative rewind)",
		[](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			if (a.empty()) return "usage: rate <rate>";
			p.SetRate(std::stof(a));
			return kOk;
		}};
	_actions["pauseat"] = {"<seconds>", "pause when position is reached",
		[](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			if (a.empty()) return "usage: pauseat <seconds>";
			p.PauseAt(std::stod(a));
			return kOk;
		}};
	_actions["rateandseek"] = {"<rate> <seconds>", "set rate and seek atomically",
		[](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			auto w = Words(a);
			if (w.size() < 2) return "usage: rateandseek <rate> <seconds>";
			p.SetRateAndSeek(std::stoi(w[0]), std::stod(w[1]));
			return kOk;
		}};
	_actions["resetconfig"] = {"", "reset AAMP configuration to defaults",
		[](PlayerInstanceAAMP &p, const Args &) -> std::string { p.ResetConfiguration(); return kOk; }};
	_actions["header"] = {"<name> <value>", "add custom HTTP header",
		[](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			auto w = Words(a);
			if (w.size() < 2) return "usage: header <name> <value>";
			p.AddCustomHTTPHeader(w[0], std::vector<std::string>(w.begin() + 1, w.end()));
			return kOk;
		}};
	_actions["config"] = {"[name=value | json]", "apply AAMP config, or print the full config when no argument",
		[](PlayerInstanceAAMP &p, const Args &a) -> std::string {
			if (a.empty()) return p.GetAAMPConfig();
			return p.InitAAMPConfig(a.c_str()) ? kOk : "config rejected";
		}};
	_actions["progress"] = {"on|off", "print PROGRESS events in this terminal (off by default)",
		[engine](PlayerInstanceAAMP &, const Args &a) -> std::string {
			bool v;
			if (!ParseBool(a, v)) return "usage: progress on|off";
			engine.logProgressEvents = v;
			return kOk;
		}};
	_actions["log"] = {"[lines]", "print the last lines of output.log (default 40)",
		[logPath](PlayerInstanceAAMP &, const Args &a) -> std::string {
			int n = a.empty() ? 40 : std::max(1, std::stoi(a));
			NSString *text = [NSString stringWithContentsOfFile:logPath encoding:NSUTF8StringEncoding error:nil];
			if (!text) return "cannot read " + std::string(logPath.UTF8String);
			NSArray *lines = [text componentsSeparatedByString:@"\n"];
			NSUInteger from = lines.count > (NSUInteger)n ? lines.count - n : 0;
			return [[lines subarrayWithRange:NSMakeRange(from, lines.count - from)] componentsJoinedByString:@"\n"].UTF8String;
		}};
	_actions["logfile"] = {"", "print the output.log path",
		[logPath](PlayerInstanceAAMP &, const Args &) -> std::string { return logPath.UTF8String; }};
	_actions["quit"] = {"", "quit the application",
		[](PlayerInstanceAAMP &, const Args &) -> std::string {
			dispatch_async(dispatch_get_main_queue(), ^{ [NSApp terminate:nil]; });
			return "bye";
		}};
	_actions["exit"] = _actions["quit"];
}

- (void)registerGetters
{
	GET("state", std::string([PlayerEngine nameForState:(int)p.GetState()].UTF8String));
	GET("position", p.GetPlaybackPosition());
	GET("duration", p.GetPlaybackDuration());
	GET("rate", p.GetPlaybackRate());
	GET("live", p.IsLive());
	GET("id", p.GetId());
	GET("volume", p.GetAudioVolume());
	GET("mute", p.GetVideoMute());
	GET("zoom", p.GetVideoZoom());
	GET("videobitrate", (long long)p.GetVideoBitrate());
	GET("audiobitrate", (long long)p.GetAudioBitrate());
	GET("videobitrates", p.GetVideoBitrates());
	GET("audiobitrates", p.GetAudioBitrates());
	GET("initialbitrate", (long long)p.GetInitialBitrate());
	GET("initialbitrate4k", (long long)p.GetInitialBitrate4k());
	GET("minbitrate", (long long)p.GetMinimumBitrate());
	GET("maxbitrate", (long long)p.GetMaximumBitrate());
	GET("rampdownlimit", p.GetRampDownLimit());
	GET("initialbufferduration", p.GetInitialBufferDuration());
	GET("audiotrack", p.GetAudioTrack());
	GET("texttrack", p.GetTextTrack());
	GET("cc", p.GetCCStatus());
	GET("drm", p.GetDRM());
	GET("preferreddrm", (int)p.GetPreferredDRM());
	GET("audiolanguage", p.GetAudioLanguage());
	GET("preferredlanguages", p.GetPreferredLanguages());
	GET("videotracks", p.GetAvailableVideoTracks());
	GET("audiotracks", p.GetAvailableAudioTracks());
	GET("alltracks", p.GetAvailableAudioTracks(true));
	GET("texttracks", p.GetAvailableTextTracks());
	GET("audiotrackinfo", p.GetAudioTrackInfo());
	GET("texttrackinfo", p.GetTextTrackInfo());
	GET("preferredaudioproperties", p.GetPreferredAudioProperties());
	GET("preferredtextproperties", p.GetPreferredTextProperties());
	GET("textstyle", p.GetTextStyle());
	GET("videorectangle", p.GetVideoRectangle());
	GET("appname", p.GetAppName());
	GET("sessionid", p.GetSessionId());
	GET("playbackstats", p.GetPlaybackStats());
	GET("videoplaybackquality", p.GetVideoPlaybackQuality());
	GET("thumbnailtracks", p.GetAvailableThumbnailTracks());
	GET("manifest", p.GetManifest());
	GET("config", p.GetAAMPConfig());
	GET("oobcc", p.IsOOBCCRenderingSupported());
}

- (void)registerSetters
{
	SET_INT("volume", SetAudioVolume);
	SET_BOOL("mute", SetVideoMute);
	SET_BOOL("subtitlemute", SetSubtitleMute);
	SET_BPS("videobitrate", SetVideoBitrate);
	SET_BPS("audiobitrate", SetAudioBitrate);
	SET_BPS("initialbitrate", SetInitialBitrate);
	SET_BPS("initialbitrate4k", SetInitialBitrate4K);
	SET_BPS("minbitrate", SetMinimumBitrate);
	SET_BPS("maxbitrate", SetMaximumBitrate);
	SET_INT("rampdownlimit", SetRampDownLimit);
	SET_INT("initrampdownlimit", SetInitRampdownLimit);
	SET_INT("initialbufferduration", SetInitialBufferDuration);
	SET_DBL("speed", SetPlaybackSpeed);
	SET_DBL("networktimeout", SetNetworkTimeout);
	SET_DBL("manifesttimeout", SetManifestTimeout);
	SET_DBL("playlisttimeout", SetPlaylistTimeout);
	SET_INT("downloadbuffer", SetDownloadBufferSize);
	SET_CSTR("networkproxy", SetNetworkProxy);
	SET_CSTR("licensereqproxy", SetLicenseReqProxy);
	SET_INT("downloadstalltimeout", SetDownloadStallTimeout);
	SET_INT("downloadstarttimeout", SetDownloadStartTimeout);
	SET_INT("downloadlowbwtimeout", SetDownloadLowBWTimeout);
	SET_DBL("liveoffset", SetLiveOffset);
	SET_DBL("liveoffset4k", SetLiveOffset4K);
	SET_INT("stalltimeout", SetStallTimeout);
	SET_INT("stallerrorcode", SetStallErrorCode);
	SET_INT("reportinterval", SetReportInterval);
	SET_INT("initfragtimeoutretrycount", SetInitFragTimeoutRetryCount);
	SET_INT("precachetime", SetPreCacheTimeWindow);
	SET_INT("vodtrickplayfps", SetVODTrickplayFPS);
	SET_INT("lineartrickplayfps", SetLinearTrickplayFPS);
	SET_INT("maxplaylistcachesize", SetMaxPlaylistCacheSize);
	SET_INT("segmentinjectfailcount", SetSegmentInjectFailCount);
	SET_INT("segmentdecryptfailcount", SetSegmentDecryptFailCount);
	SET_INT("tuneeventconfig", SetTuneEventConfig);
	SET_INT("pausedbehavior", SetPausedBehavior);
	SET_INT("audiotrack", SetAudioTrack);
	SET_INT("contentprotectiontimeout", SetContentProtectionDataUpdateTimeout);
	SET_CSTR("language", SetLanguage);
	SET_CSTR("subtitlelanguage", SetPreferredSubtitleLanguage);
	SET_CSTR("preferredtextlanguages", SetPreferredTextLanguages);
	SET_CSTR("preferredcodec", SetPreferredCodec);
	SET_CSTR("preferredlabels", SetPreferredLabels);
	SET_CSTR("preferredrenditions", SetPreferredRenditions);
	SET_CSTR("licensecustomdata", SetLicenseCustomData);
	SET_STR("sessiontoken", SetSessionToken);
	SET_STR("textstyle", SetTextStyle);
	SET_STR("appname", SetAppName);
	SET_STR("useragent", SetUserAgent);
	SET_BOOL("cc", SetCCStatus);
	SET_BOOL("stereoonly", SetStereoOnlyPlayback);
	SET_BOOL("bulktimedmeta", SetBulkTimedMetaReport);
	SET_BOOL("bulktimedmetalive", SetBulkTimedMetaReportLive);
	SET_BOOL("retuneunpaired", SetRetuneForUnpairedDiscontinuity);
	SET_BOOL("retunegsterror", SetRetuneForGSTInternalError);
	SET_BOOL("anonymousrequest", SetAnonymousRequest);
	SET_BOOL("avgbwforabr", SetAvgBWForABR);
	SET_BOOL("asynctune", SetAsyncTuneConfig);
	SET_BOOL("westerossink", SetWesterosSinkConfig);
	SET_BOOL("licensecaching", SetLicenseCaching);
	SET_BOOL("outputresolutioncheck", SetOutputResolutionCheck);
	SET_BOOL("matchingbaseurl", SetMatchingBaseUrlConfig);
	SET_BOOL("propagateuri", SetPropagateUriParameters);
	SET_BOOL("sslverifypeer", SetSslVerifyPeerConfig);
	SET_BOOL("newabr", SetNewABRConfig);
	SET_BOOL("newadbreaker", SetNewAdBreakerConfig);
	SET_BOOL("base64licensewrapping", SetBase64LicenseWrapping);
	SET_BOOL("disable4k", SetDisable4K);
	SET_BOOL("audioonly", SetAudioOnlyPlayback);
	SET_BOOL("nativecc", SetNativeCCRendering);
	SET_BOOL("reportvideopts", SetReportVideoPTS);
	SET_BOOL("absolutetimeline", SetUseAbsoluteTimeline);
	SET_BOOL("repairiframes", SetRepairIframes);
	SET_BOOL("seekablerange", EnableSeekableRange);
	SET_BOOL("videorectangleenable", EnableVideoRectangle);

	_setters["zoom"] = {"0..6", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		if (a.empty()) return "usage: set zoom <0..6>";
		p.SetVideoZoom((VideoZoomMode)std::stoi(a));
		return kOk;
	}};
	_setters["videorectangle"] = {"<x> <y> <w> <h>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		auto w = Words(a);
		if (w.size() < 4) return "usage: set videorectangle <x> <y> <w> <h>";
		p.SetVideoRectangle(std::stoi(w[0]), std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[3]));
		return kOk;
	}};
	_setters["texttrack"] = {"<id>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		if (a.empty()) return "usage: set texttrack <id>";
		p.SetTextTrack(std::stoi(a));
		return kOk;
	}};
	_setters["thumbnailtrack"] = {"<index>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		if (a.empty()) return "usage: set thumbnailtrack <index>";
		return p.SetThumbnailTrack(std::stoi(a)) ? kOk : "failed";
	}};
	_setters["preferredlanguages"] = {"<csv> [rendition] [type] [codec] [labels]", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		auto w = Words(a);
		if (w.empty()) return "usage: set preferredlanguages <csv> [rendition] [type] [codec] [labels]";
		p.SetPreferredLanguages(w[0].c_str(),
			w.size() > 1 ? w[1].c_str() : NULL, w.size() > 2 ? w[2].c_str() : NULL,
			w.size() > 3 ? w[3].c_str() : NULL, w.size() > 4 ? w[4].c_str() : NULL);
		return kOk;
	}};
	_setters["licenseserver"] = {"<url>", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		p.SetLicenseServerURL(a.c_str());
		return kOk;
	}};
	_setters["videotracks"] = {"<bps> [bps...]", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		std::vector<BitsPerSecond> list;
		for (const auto &w : Words(a)) list.push_back((BitsPerSecond)std::stoll(w));
		p.SetVideoTracks(list);
		return kOk;
	}};
	_setters["config"] = {"name=value | json", "", [](PlayerInstanceAAMP &p, const Args &a) -> std::string {
		return p.InitAAMPConfig(a.c_str()) ? kOk : "config rejected";
	}};
}

- (NSString *)helpText
{
	std::ostringstream out;
	out << "Commands:\n";
	for (const auto &kv : _actions)
	{
		out << "  " << kv.first << (kv.second.usage.empty() ? "" : " " + kv.second.usage) << "\n      " << kv.second.help << "\n";
	}
	out << "  get <name>              read a value\n  set <name> <value>      write a value\n  help get | help set     list names\n";
	return [NSString stringWithUTF8String:out.str().c_str()];
}

- (NSString *)namesOf:(const Table &)table title:(const char *)title
{
	std::ostringstream out;
	out << title << ":\n";
	int col = 0;
	for (const auto &kv : table)
	{
		std::string label = kv.first + (kv.second.usage.empty() ? "" : " " + kv.second.usage);
		out << "  " << label << ((++col % 2) ? std::string(label.size() < 38 ? 38 - label.size() : 1, ' ') : "\n");
	}
	out << "\n";
	return [NSString stringWithUTF8String:out.str().c_str()];
}

- (NSString *)execute:(NSString *)line
{
	std::string cmd, rest;
	SplitFirst(line.UTF8String ?: "", cmd, rest);
	if (cmd.empty())
	{
		return @"";
	}
	PlayerInstanceAAMP *player = _engine.player;
	if (!player)
	{
		return @"player not available";
	}
	try
	{
		if (cmd == "help" || cmd == "?")
		{
			if (rest == "get") return [self namesOf:_getters title:"Getters"];
			if (rest == "set") return [self namesOf:_setters title:"Setters"];
			return [self helpText];
		}
		if (cmd == "get" || cmd == "set")
		{
			Table &table = (cmd == "get") ? _getters : _setters;
			std::string name, value;
			SplitFirst(rest, name, value);
			if (name.empty()) return [self namesOf:table title:(cmd == "get" ? "Getters" : "Setters")];
			auto it = table.find(name);
			if (it == table.end()) return [NSString stringWithFormat:@"unknown %s '%s' (try: help %s)", cmd.c_str(), name.c_str(), cmd.c_str()];
			return [NSString stringWithUTF8String:it->second.fn(*player, value).c_str()];
		}
		auto it = _actions.find(cmd);
		if (it == _actions.end())
		{
			return [NSString stringWithFormat:@"unknown command '%s' (try: help)", cmd.c_str()];
		}
		return [NSString stringWithUTF8String:it->second.fn(*player, rest).c_str()];
	}
	catch (const std::exception &e)
	{
		return [NSString stringWithFormat:@"bad argument: %s", e.what()];
	}
}

- (NSArray<NSString *> *)completionsForLine:(NSString *)line
{
	std::string text = line.UTF8String ?: "";
	bool endsWithSpace = !text.empty() && (text.back() == ' ');
	auto words = Words(text);
	const Table *table = nullptr;
	std::string prefix;
	if (words.empty() || (words.size() == 1 && !endsWithSpace))
	{
		prefix = words.empty() ? "" : words[0];
		NSMutableArray *out = [NSMutableArray array];
		for (const auto &kv : _actions)
			if (kv.first.compare(0, prefix.size(), prefix) == 0) [out addObject:@(kv.first.c_str())];
		for (const char *name : {"get", "set", "help"})
			if (std::string(name).compare(0, prefix.size(), prefix) == 0) [out addObject:@(name)];
		return out;
	}
	if (words[0] == "get") table = &_getters;
	else if (words[0] == "set") table = &_setters;
	if (!table || words.size() > 2 || (words.size() == 2 && endsWithSpace))
	{
		return @[];
	}
	prefix = words.size() == 2 ? words[1] : "";
	NSMutableArray *out = [NSMutableArray array];
	for (const auto &kv : *table)
		if (kv.first.compare(0, prefix.size(), prefix) == 0) [out addObject:@(kv.first.c_str())];
	return out;
}

@end
