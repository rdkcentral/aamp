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

/**
 * @file AampVodStitcher.cpp
 * @brief Client-side VOD CDAI manifest stitcher (C++ port of manifest-genie).
 *
 * Stitching algorithm:
 *   1. Fetch all resolved ad MPDs in parallel.
 *   2. Walk the main MPD periods; for each insertion point split the period at
 *      the nearest segment boundary using SegmentTimeline arithmetic and insert
 *      the ad period(s) between the two halves.
 *   3. Re-emit every period with an explicit id, start, duration, and BaseURL
 *      so the result is a fully self-contained static multi-period MPD.
 */

#include "AampVodStitcher.h"
#include "admanager_mpd.h"
#include "priv_aamp.h"
#include "AampLogManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <future>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "downloader/AampCurlDownloader.h"
#include "AampConfig.h"

/* -------------------------------------------------------------------------
 * Internal string helpers  (mirrors manifest-genie GetAttr / SetAttr)
 * ---------------------------------------------------------------------- */

/**
 * @brief Extract the value of an XML attribute from an element opening tag.
 *        GetAttr("<foo bar=\"baz\">", "bar") -> "baz"
 *        Returns empty string when the attribute is absent.
 */
static std::string GetAttr(const std::string &element, const std::string &attr)
{
	std::string needle = attr + "=\"";
	size_t pos = element.find(needle);
	if (pos == std::string::npos)
		return {};
	pos += needle.size();
	size_t end = element.find('"', pos);
	if (end == std::string::npos)
		return {};
	return element.substr(pos, end - pos);
}

/**
 * @brief Set (or insert) an XML attribute in an element opening tag.
 *        SetAttr("<foo>", "bar", "baz") -> "<foo bar=\"baz\">"
 *        If the attribute is already present its value is replaced.
 */
static std::string SetAttr(std::string element, const std::string &attr, const std::string &value)
{
	std::string needle = attr + "=\"";
	size_t pos = element.find(needle);
	if (pos != std::string::npos)
	{
		pos += needle.size();
		size_t end = element.find('"', pos);
		if (end != std::string::npos)
			element = element.substr(0, pos) + value + element.substr(end);
	}
	else
	{
		size_t sp = element.find(' ');
		if (sp != std::string::npos)
			element = element.substr(0, sp + 1) + needle + value + "\"" + element.substr(sp);
		else
		{
			size_t gt = element.rfind('>');
			if (gt == std::string::npos) gt = element.size();
			element = element.substr(0, gt) + " " + needle + value + "\"" + element.substr(gt);
		}
	}
	return element;
}

/* -------------------------------------------------------------------------
 * ISO 8601 duration helpers  (mirrors manifest-genie ParseTime / PackTime)
 * ---------------------------------------------------------------------- */

/**
 * @brief Parse an ISO 8601 duration string to seconds.
 *        "PT1H2M3.456S" -> 3723.456
 */
static double ParseIsoDuration(const std::string &s)
{
	double result = 0.0;
	if (s.size() < 2 || s[0] != 'P' || s[1] != 'T')
		return result;
	size_t i = 2;
	while (i < s.size())
	{
		size_t j = i;
		while (j < s.size() && (std::isdigit(static_cast<unsigned char>(s[j])) || s[j] == '.'))
			j++;
		if (j == i || j >= s.size())
			break;
		double val = std::stod(s.substr(i, j - i));
		char unit = s[j];
		if      (unit == 'H') result += val * 3600.0;
		else if (unit == 'M') result += val * 60.0;
		else if (unit == 'S') result += val;
		i = j + 1;
	}
	return result;
}

/**
 * @brief Format seconds as an ISO 8601 duration string.
 *        3723.456 -> "PT1H2M3.456S"
 */
static std::string PackIsoDuration(double seconds)
{
	if (seconds < 0.0) seconds = 0.0;
	long long total = (long long)std::floor(seconds);
	double ms = seconds - (double)total;
	long long sec = total % 60; total -= sec;
	double secFrac = sec + ms;
	total /= 60;
	long long min = total % 60; total -= min;
	long long hour = total / 60;

	char buf[64];
	snprintf(buf, sizeof(buf), "PT%lldH%lldM%.3fS", hour, min, secFrac);
	return std::string(buf);
}

/* -------------------------------------------------------------------------
 * SegmentTimeline helpers  (mirrors UncompressTimeline / CompressTimeline)
 * ---------------------------------------------------------------------- */

/**
 * @brief Expand a <SegmentTimeline>...</SegmentTimeline> XML fragment into a
 *        flat vector of segment-start times (in timescale units).
 *
 * Each entry rc[i] is the start time of segment i; rc[i+1]-rc[i] is its
 * duration.  rc.back() is a sentinel equal to the start of the first segment
 * AFTER the last one (needed by CompressTimeline and GetTimelineIndex).
 */
static std::vector<uint64_t> UncompressTimeline(const std::string &xml)
{
	std::vector<uint64_t> rc;
	size_t pos = 0;
	uint64_t t = 0;
	bool first = true;

	while (true)
	{
		size_t spos = xml.find("<S ", pos);
		if (spos == std::string::npos)
			break;
		size_t epos = xml.find('>', spos);
		if (epos == std::string::npos)
			break;
		std::string entry = xml.substr(spos, epos - spos + 1);
		pos = epos + 1;

		std::string tStr = GetAttr(entry, "t");
		if (!tStr.empty())
			t = (uint64_t)std::stoull(tStr);
		else if (first)
			t = 0;

		std::string dStr = GetAttr(entry, "d");
		if (dStr.empty()) continue;
		uint64_t d = (uint64_t)std::stoull(dStr);

		std::string rStr = GetAttr(entry, "r");
		int r = rStr.empty() ? 0 : std::stoi(rStr);

		for (int rep = 0; rep <= r; rep++)
		{
			rc.push_back(t);
			t += d;
		}
		first = false;
	}

	if (!rc.empty())
		rc.push_back(t);
	return rc;
}

/**
 * @brief Compress a flat vector of segment-start times back into <S .../> XML.
 *        The input must include the sentinel end-time as the last element.
 *        Only the first <S> gets an explicit t= attribute; run-length encoding
 *        is applied via r= for consecutive equal-duration segments.
 */
static std::string CompressTimeline(const std::vector<uint64_t> &samples)
{
	if (samples.size() < 2)
		return {};

	std::ostringstream out;
	bool firstEntry = true;
	size_t i = 0;
	size_t n = samples.size() - 1; // last element is sentinel

	while (i < n)
	{
		uint64_t tStart = samples[i];
		uint64_t d = samples[i + 1] - samples[i];
		int r = 0;
		while (i + r + 2 < n && (samples[i + r + 2] - samples[i + r + 1]) == d)
			r++;

		out << "<S";
		if (firstEntry)
		{
			out << " t=\"" << tStart << "\"";
			firstEntry = false;
		}
		out << " d=\"" << d << "\"";
		if (r > 0)
			out << " r=\"" << r << "\"";
		out << "/>\n";

		i += r + 1;
	}
	return out.str();
}

/**
 * @brief Return the index of the first sample whose start time >= sec*timescale.
 *        Returns samples.size()-1 (the sentinel index) if sec is beyond the end.
 */
static size_t GetTimelineIndex(const std::vector<uint64_t> &samples,
                               uint64_t timescale, double sec)
{
	uint64_t pts = (uint64_t)(sec * (double)timescale);
	for (size_t i = 0; i < samples.size(); i++)
	{
		if (samples[i] >= pts)
			return i;
	}
	return samples.size() > 0 ? samples.size() - 1 : 0;
}

/* -------------------------------------------------------------------------
 * Period trimming  (mirrors manifest-genie TrimPeriod)
 * ---------------------------------------------------------------------- */

/**
 * @brief Trim a period's SegmentTemplate/SegmentTimeline blocks to the window
 *        [startSec, endSec).  Updates presentationTimeOffset and startNumber
 *        (defaulting startNumber to 1 per the DASH spec if absent).
 *
 * @param periodBody  Inner XML of the <Period> element (everything between
 *                    the opening '>' and '</Period>').
 * @param startSec    Start of the desired window relative to the period (0 =
 *                    keep from the beginning).
 * @param endSec      End of the desired window relative to the period (0 =
 *                    keep to the end).
 * @return Trimmed period body XML.
 */
static std::string TrimPeriod(const std::string &periodBody,
                              double startSec, double endSec)
{
	std::string result = periodBody;
	const std::string SEG_TMPL = "<SegmentTemplate ";

	size_t searchFrom = 0;
	while (true)
	{
		size_t tmplPos = result.find(SEG_TMPL, searchFrom);
		if (tmplPos == std::string::npos)
			break;

		size_t tmplEnd = result.find('>', tmplPos);
		if (tmplEnd == std::string::npos)
			break;

		std::string tmplTag = result.substr(tmplPos, tmplEnd - tmplPos + 1);

		std::string tsStr = GetAttr(tmplTag, "timescale");
		uint64_t timescale = tsStr.empty() ? 1 : (uint64_t)std::stoull(tsStr);

		std::string snStr = GetAttr(tmplTag, "startNumber");
		int startNumber = snStr.empty() ? 1 : std::stoi(snStr);

		const std::string TL_OPEN  = "<SegmentTimeline>";
		const std::string TL_CLOSE = "</SegmentTimeline>";

		size_t tlPos = result.find(TL_OPEN, tmplEnd);
		if (tlPos == std::string::npos)
		{
			searchFrom = tmplEnd + 1;
			continue;
		}
		size_t tlContent = tlPos + TL_OPEN.size();
		size_t tlEnd = result.find(TL_CLOSE, tlContent);
		if (tlEnd == std::string::npos)
		{
			searchFrom = tmplEnd + 1;
			continue;
		}

		std::string tlXml = result.substr(tlContent, tlEnd - tlContent);
		std::vector<uint64_t> samples = UncompressTimeline(tlXml);

		if (samples.size() < 2)
		{
			searchFrom = tlEnd + TL_CLOSE.size();
			continue;
		}

		size_t idx1 = 0;
		size_t idx2 = samples.size() - 1; // default: all segments (excluding sentinel)

		if (startSec > 0.0)
			idx1 = GetTimelineIndex(samples, timescale, startSec);
		if (endSec > 0.0)
		{
			idx2 = GetTimelineIndex(samples, timescale, endSec);
			if (idx2 < samples.size() - 1)
				idx2++; // include the segment that straddles the boundary
		}

		// Clamp to valid range
		if (idx1 >= samples.size() - 1)
			idx1 = samples.size() - 2;
		if (idx2 > samples.size() - 1)
			idx2 = samples.size() - 1;
		if (idx2 <= idx1)
			idx2 = idx1 + 1;

		// Build sliced sample vector (retain sentinel)
		std::vector<uint64_t> sliced(samples.begin() + idx1, samples.begin() + idx2 + 1);

		uint64_t pto = sliced[0];
		int newStartNumber = startNumber + (int)idx1;

		std::string compressed = CompressTimeline(sliced);

		// Replace the SegmentTimeline content in-place
		std::string newTl = TL_OPEN + "\n" + compressed + TL_CLOSE;
		result = result.substr(0, tlPos) + newTl + result.substr(tlEnd + TL_CLOSE.size());

		// Update the SegmentTemplate tag attributes (find again — string changed)
		size_t tmplPos2 = result.rfind(SEG_TMPL, tlPos);
		size_t tmplEnd2 = result.find('>', tmplPos2);
		if (tmplPos2 != std::string::npos && tmplEnd2 != std::string::npos)
		{
			std::string tag2 = result.substr(tmplPos2, tmplEnd2 - tmplPos2 + 1);
			tag2 = SetAttr(tag2, "presentationTimeOffset", std::to_string(pto));
			tag2 = SetAttr(tag2, "startNumber", std::to_string(newStartNumber));
			result = result.substr(0, tmplPos2) + tag2 + result.substr(tmplEnd2 + 1);
		}

		searchFrom = tmplPos2 + SEG_TMPL.size();
	}
	return result;
}

/* -------------------------------------------------------------------------
 * Period body extraction from MPD text
 * ---------------------------------------------------------------------- */

struct PeriodSlice
{
	std::string body;    // inner XML between '>' and '</Period>'
	double      duration; // in seconds
};

/**
 * @brief Extract all <Period> bodies and their durations from raw MPD XML.
 *        For single-period MPDs the duration falls back to
 *        mediaPresentationDuration.
 */
static std::vector<PeriodSlice> ExtractPeriods(const std::string &mpdText)
{
	std::vector<PeriodSlice> result;

	// Extract mediaPresentationDuration from the <MPD ...> opening tag only,
	// not from the entire document (avoids false matches in Period/AdaptationSet text).
	std::string mpdDur;
	{
		size_t mpdTagStart = mpdText.find("<MPD");
		size_t mpdTagEnd   = (mpdTagStart != std::string::npos)
		                     ? mpdText.find('>', mpdTagStart) : std::string::npos;
		if (mpdTagEnd != std::string::npos)
		{
			std::string mpdTag = mpdText.substr(mpdTagStart, mpdTagEnd - mpdTagStart + 1);
			mpdDur = GetAttr(mpdTag, "mediaPresentationDuration");
		}
	}

	size_t pos = 0;
	while (true)
	{
		size_t pStart = mpdText.find("<Period", pos);
		if (pStart == std::string::npos)
			break;

		size_t tagEnd = mpdText.find('>', pStart);
		if (tagEnd == std::string::npos)
			break;

		std::string tag = mpdText.substr(pStart, tagEnd - pStart + 1);

		size_t bodyStart = tagEnd + 1;
		size_t bodyEnd = mpdText.find("</Period>", bodyStart);
		if (bodyEnd == std::string::npos)
			break;

		std::string body = mpdText.substr(bodyStart, bodyEnd - bodyStart);

		std::string durStr = GetAttr(tag, "duration");
		if (durStr.empty())
			durStr = mpdDur;

		double dur = ParseIsoDuration(durStr);

		// If duration is still unknown, derive it from the SegmentTimeline
		// (handles ad MPDs that omit duration on both Period and MPD level).
		if (dur <= 0.0)
		{
			const std::string TL_OPEN  = "<SegmentTimeline>";
			const std::string TL_CLOSE = "</SegmentTimeline>";
			size_t tlPos = body.find(TL_OPEN);
			while (tlPos != std::string::npos)
			{
				size_t tlContent = tlPos + TL_OPEN.size();
				size_t tlEnd = body.find(TL_CLOSE, tlContent);
				if (tlEnd == std::string::npos) break;
				std::string tlXml = body.substr(tlContent, tlEnd - tlContent);
				std::vector<uint64_t> samples = UncompressTimeline(tlXml);

				// Find the timescale for this SegmentTemplate
				size_t tmplPos = body.rfind("<SegmentTemplate ", tlPos);
				uint64_t timescale = 1;
				if (tmplPos != std::string::npos)
				{
					size_t tmplEnd = body.find('>', tmplPos);
					if (tmplEnd != std::string::npos)
					{
						std::string tmplTag = body.substr(tmplPos, tmplEnd - tmplPos + 1);
						std::string tsStr = GetAttr(tmplTag, "timescale");
						if (!tsStr.empty())
							timescale = (uint64_t)std::stoull(tsStr);
					}
				}

				if (samples.size() >= 2 && timescale > 0)
				{
					double tlDur = (double)(samples.back() - samples.front()) / (double)timescale;
					if (tlDur > dur)
						dur = tlDur;
				}
				tlPos = body.find(TL_OPEN, tlEnd + TL_CLOSE.size());
			}
		}

		result.push_back({body, dur});
		pos = bodyEnd + 9; // strlen("</Period>")
	}
	return result;
}

/* -------------------------------------------------------------------------
 * Base URL derivation
 * ---------------------------------------------------------------------- */

static std::string BaseUrlFromLocator(const std::string &url)
{
	size_t slash = url.rfind('/');
	if (slash == std::string::npos)
		return url;
	return url.substr(0, slash + 1);
}

/* -------------------------------------------------------------------------
 * Parallel ad MPD fetching
 * ---------------------------------------------------------------------- */

struct AdFetchResult
{
	std::string breakId;
	double      insertionPointSec;
	double      durationSec;
	std::string mpdText;
	std::string effectiveUrl;
	bool        ok;
};

/** Maximum number of ad MPDs to fetch concurrently. */
static constexpr int kMaxParallelAdFetches = 6;

/**
 * @brief Fetch one VOD ad MPD using a private AampCurlDownloader instance.
 *        Called from std::async threads — each call owns its own CURL* handle
 *        so there is no shared-handle data race and no stack-use-after-return.
 */
static AdFetchResult FetchOneAdMPD(
	const std::string &breakId,
	double             insertionPointSec,
	double             durationSec,
	const std::string &url,
	const std::string &proxyName,
	const std::string &userAgent)
{
	AdFetchResult res;
	res.breakId           = breakId;
	res.insertionPointSec = insertionPointSec;
	res.durationSec       = durationSec;
	res.ok                = false;

	auto dnldCfg = std::make_shared<DownloadConfig>();
	dnldCfg->proxyName            = proxyName;
	dnldCfg->userAgentString      = userAgent;
	dnldCfg->iDownloadTimeout     = DEFAULT_CURL_TIMEOUT;
	dnldCfg->bNeedDownloadMetrics = true;

	auto dnldResp = std::make_shared<DownloadResponse>();

	AampCurlDownloader downloader;
	downloader.Initialize(dnldCfg);

	std::string fetchUrl = url;
	auto fetchStart = std::chrono::steady_clock::now();
	int rc = downloader.Download(fetchUrl, dnldResp);
	double totalPerformRequest = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - fetchStart).count();

	downloader.CleanupCurlHeaderResources();

	const auto &m = dnldResp->downloadCompleteMetrics;
	AAMPLOG_MIL("HttpRequestEnd: 3,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%g,%ld,%" BITSPERSECOND_FORMAT ",0,%.500s",
		eMEDIATYPE_MANIFEST, rc,
		totalPerformRequest, m.total, m.connect, m.startTransfer,
		m.resolve, m.appConnect, m.preTransfer, m.redirect,
		m.dlSize, m.reqSize, m.downloadbps,
		(dnldResp->sEffectiveUrl.empty() ? url : dnldResp->sEffectiveUrl).c_str());

	if (rc < 200 || rc >= 300 || dnldResp->mDownloadData.empty())
	{
		AAMPLOG_WARN("[VodStitcher] Failed to fetch ad MPD break=%s url=%s http=%d",
		             breakId.c_str(), url.c_str(), rc);
	}
	else
	{
		res.mpdText      = dnldResp->getString();
		res.effectiveUrl = dnldResp->sEffectiveUrl.empty() ? url : dnldResp->sEffectiveUrl;
		res.ok           = true;
		AAMPLOG_INFO("[VodStitcher] Fetched ad MPD break=%s url=%s size=%zu",
		             breakId.c_str(), res.effectiveUrl.c_str(), res.mpdText.size());
	}
	return res;
}

/**
 * @brief Fetch all resolved VOD ad MPDs in parallel (up to kMaxParallelAdFetches
 *        concurrent downloads) and return results in registration order.
 *
 * Each download uses its own AampCurlDownloader instance (and therefore its
 * own CURL* handle), so there is no shared-handle data race and no
 * stack-use-after-return hazard from GetFile's curl progress callbacks.
 */
static std::vector<AdFetchResult> FetchAdMPDsParallel(
	PrivateInstanceAAMP  *aamp,
	PrivateCDAIObjectMPD *cdaiObj)
{
	struct BreakTodo {
		std::string breakId;
		double      insertionPointSec;
		double      durationSec;
		std::string url;
	};
	std::vector<BreakTodo> todo;
	{
		std::lock_guard<std::recursive_mutex> lock(cdaiObj->mDaiMtx);
		for (const std::string &bid : cdaiObj->mVodAdBreakOrder)
		{
			auto vodIt = cdaiObj->mVodAdBreaks.find(bid);
			if (vodIt == cdaiObj->mVodAdBreaks.end())
				continue;
			const VodAdBreakInfo &info = vodIt->second;
			if (info.cancelled)
				continue;
			if (info.adUrl.empty())
			{
				AAMPLOG_WARN("[VodStitcher] Break %s has no ad URL — skipping",
				             info.breakId.c_str());
				continue;
			}
			todo.push_back({info.breakId, info.insertionPointSec,
			                info.breakDurationSec, info.adUrl});
		}
	}

	if (todo.empty())
		return {};

	// Snapshot proxy and user-agent once; safe to read outside mDaiMtx.
	std::string proxyName  = aamp->GetNetworkProxy();
	std::string userAgent  = aamp->mConfig->GetUserAgentString();

	// Launch up to kMaxParallelAdFetches downloads at a time using a sliding
	// window so we never exceed the concurrency cap.
	const int n = (int)todo.size();
	std::vector<std::future<AdFetchResult>> futures(n);

	auto launchFrom = [&](int idx) {
		const BreakTodo &t = todo[idx];
		futures[idx] = std::async(std::launch::async,
			FetchOneAdMPD,
			t.breakId, t.insertionPointSec, t.durationSec,
			t.url, proxyName, userAgent);
	};

	// Seed initial wave
	int launched = 0;
	while (launched < n && launched < kMaxParallelAdFetches)
		launchFrom(launched++);

	std::vector<AdFetchResult> results;
	results.reserve(n);

	// Collect results in order; launch next as each slot frees.
	for (int collected = 0; collected < n; collected++)
	{
		results.push_back(futures[collected].get());
		if (launched < n)
			launchFrom(launched++);
	}

	return results;
}

/* -------------------------------------------------------------------------
 * MPD envelope builder
 * ---------------------------------------------------------------------- */

/**
 * @brief Produce the MPD header (everything before the first <Period>).
 *        Copies namespace declarations from the original MPD but overrides
 *        type="static" and mediaPresentationDuration.
 */
static std::string BuildMPDHeader(const std::string &mainMpdText,
                                  double totalDurationSec)
{
	size_t pPos = mainMpdText.find("<Period");
	std::string preamble = (pPos != std::string::npos)
	                       ? mainMpdText.substr(0, pPos)
	                       : mainMpdText;

	preamble = SetAttr(preamble, "type", "static");
	preamble = SetAttr(preamble, "mediaPresentationDuration",
	                   PackIsoDuration(totalDurationSec));

	// Remove minimumUpdatePeriod — not applicable to stitched static MPD
	const std::string mup = "minimumUpdatePeriod=\"";
	size_t mupPos = preamble.find(mup);
	if (mupPos != std::string::npos)
	{
		size_t mupEnd = preamble.find('"', mupPos + mup.size());
		if (mupEnd != std::string::npos)
			preamble.erase(mupPos - 1, mupEnd - mupPos + mup.size() + 1);
	}

	return preamble;
}

/* -------------------------------------------------------------------------
 * Top-level stitcher
 * ---------------------------------------------------------------------- */

std::string BuildStitchedVodManifest(
	PrivateInstanceAAMP    *aamp,
	const std::string      &mainMpdText,
	const std::string      &mainMpdUrl,
	PrivateCDAIObjectMPD   *cdaiObj)
{
	if (!aamp || !cdaiObj)
		return {};

	// Wait until every non-cancelled registered break is resolved or failed.
	// This ensures midroll/postroll ads are included in the stitched MPD.
	// Timeout after 10s to avoid blocking playback indefinitely if DAI is slow.
	bool hasBreaks = false;
	{
		std::lock_guard<std::recursive_mutex> snapLk(cdaiObj->mDaiMtx);
		hasBreaks = !cdaiObj->mVodAdBreaks.empty();
	}
	if (hasBreaks)
	{
		const int kTimeoutMs = 10000;
		std::unique_lock<std::mutex> lk(cdaiObj->mVodAllAdsResolvedMtx);
		bool allDone = cdaiObj->mVodAllAdsResolvedCV.wait_for(
			lk, std::chrono::milliseconds(kTimeoutMs),
			[cdaiObj]{ return cdaiObj->AreAllVodAdsResolved(); });
		if (allDone)
			AAMPLOG_INFO("[VodStitcher] All VOD ads resolved — proceeding with full stitch");
		else
			AAMPLOG_WARN("[VodStitcher] Timeout waiting for all VOD ads — stitching with what is resolved");
	}
	// Bail out if nothing at all is resolved yet (no breaks registered or all failed before any resolved)
	if (!cdaiObj->AreAllVodAdsResolved())
	{
		std::lock_guard<std::recursive_mutex> lock(cdaiObj->mDaiMtx);
		bool hasAny = false;
		for (const auto &kv : cdaiObj->mVodAdBreaks)
		{
			if (kv.second.cancelled) continue;
			auto abIt = cdaiObj->mAdBreaks.find(kv.first);
			if (abIt != cdaiObj->mAdBreaks.end() &&
			    abIt->second.ads && !abIt->second.ads->empty() &&
			    abIt->second.ads->at(0).resolved)
			{ hasAny = true; break; }
		}
		if (!hasAny)
		{
			AAMPLOG_INFO("[VodStitcher] No resolved VOD ad breaks — skipping stitching");
			return {};
		}
	}

	// Fetch all ad MPDs in parallel — each download uses its own
	// AampCurlDownloader (and therefore its own CURL* handle).
	std::vector<AdFetchResult> adResults = FetchAdMPDsParallel(aamp, cdaiObj);

	// Build insertion map: insertionPointSec -> [AdFetchResult*, ...]
	// Ordered by insertion point; multiple ads at the same point are chained.
	std::map<double, std::vector<const AdFetchResult *>> insertionMap;
	for (const auto &r : adResults)
	{
		if (r.ok)
			insertionMap[r.insertionPointSec].push_back(&r);
		else
			AAMPLOG_WARN("[VodStitcher] Ad fetch failed for break=%s — playing through",
			             r.breakId.c_str());
	}

	if (insertionMap.empty())
	{
		AAMPLOG_WARN("[VodStitcher] All ad fetches failed — returning original MPD");
		return {};
	}

	// Extract main content periods
	std::vector<PeriodSlice> mainPeriods = ExtractPeriods(mainMpdText);
	if (mainPeriods.empty())
	{
		AAMPLOG_ERR("[VodStitcher] Could not extract periods from main MPD");
		return {};
	}

	std::string mainBaseUrl = BaseUrlFromLocator(mainMpdUrl);

	// Walk main periods and splice ad periods at insertion points.
	// insertionPoints are period-relative: we track periodOffset as we go.
	std::ostringstream output;
	int periodId      = 0;
	double totalDur   = 0.0;
	double periodOffset = 0.0; // cumulative start of current main period in timeline

	for (size_t pi = 0; pi < mainPeriods.size(); pi++)
	{
		const PeriodSlice &mp = mainPeriods[pi];
		double periodDur = mp.duration;

		// Collect all insertion points that fall within this period
		// (period-relative: [0, periodDur))
		double periodStart = periodOffset;
		double periodEnd   = periodOffset + periodDur;

		// Gather insertions within [periodStart, periodEnd].
		// Use <= on the upper bound so postrolls registered exactly at content
		// end (ipt == periodEnd of the last period) are included.
		bool isLastPeriod = (pi == mainPeriods.size() - 1);
		std::vector<std::pair<double, const std::vector<const AdFetchResult *> *>> localInsertions;
		for (auto &ins : insertionMap)
		{
			double ipt = ins.first;
			bool inRange = (ipt >= periodStart) &&
			               (isLastPeriod ? (ipt <= periodEnd + 0.001) : (ipt < periodEnd));
			if (inRange)
				localInsertions.push_back({std::min(ipt - periodStart, periodDur), &ins.second});
		}
		std::sort(localInsertions.begin(), localInsertions.end(),
		          [](const auto &a, const auto &b){ return a.first < b.first; });

		double cursor = 0.0; // position within this period

		for (auto &ins : localInsertions)
		{
			double spliceAt = ins.first; // period-relative splice point
			const std::vector<const AdFetchResult *> &ads = *ins.second;

			// Emit the pre-splice main content slice [cursor, spliceAt)
			if (spliceAt > cursor)
			{
				double sliceDur = spliceAt - cursor;
				std::string trimmed = TrimPeriod(mp.body, cursor, spliceAt);
				output << "\n<Period id=\"mc" << periodId++
				       << "\" start=\"" << PackIsoDuration(totalDur)
				       << "\" duration=\"" << PackIsoDuration(sliceDur) << "\">\n"
				       << "<BaseURL>" << mainBaseUrl << "</BaseURL>\n"
				       << trimmed
				       << "\n</Period>\n";
				totalDur += sliceDur;
			}

			// Emit all chained ad periods at this splice point
			for (const AdFetchResult *ad : ads)
			{
				std::vector<PeriodSlice> adPeriods = ExtractPeriods(ad->mpdText);
				std::string adBaseUrl = BaseUrlFromLocator(ad->effectiveUrl);

				if (adPeriods.empty())
				{
					AAMPLOG_WARN("[VodStitcher] No periods in ad MPD for break=%s — skipping ad",
					             ad->breakId.c_str());
					continue;
				}
				double adConsumed = 0.0;
				for (size_t ai = 0; ai < adPeriods.size(); ai++)
				{
					const PeriodSlice &ap = adPeriods[ai];
					double adSliceDur = ap.duration;
					if (adSliceDur <= 0.0) continue;

					std::string trimmedAd = TrimPeriod(ap.body, 0.0, 0.0);
					output << "\n<Period id=\"ad" << periodId++
					       << "\" start=\"" << PackIsoDuration(totalDur)
					       << "\" duration=\"" << PackIsoDuration(adSliceDur) << "\">\n"
					       << "<BaseURL>" << adBaseUrl << "</BaseURL>\n"
					       << trimmedAd
					       << "\n</Period>\n";
					totalDur   += adSliceDur;
					adConsumed += adSliceDur;
				}
				AAMPLOG_INFO("[VodStitcher] Inserted ad break=%s dur=%.3fs at timeline=%.3fs",
				             ad->breakId.c_str(), adConsumed, totalDur - adConsumed);
			}

			cursor = spliceAt; // continue main content from splice point
		}

		// Emit the remainder of the main period [cursor, periodDur)
		double remainDur = periodDur - cursor;
		if (remainDur > 0.0)
		{
			std::string trimmed = TrimPeriod(mp.body, cursor, 0.0);
			output << "\n<Period id=\"mc" << periodId++
			       << "\" start=\"" << PackIsoDuration(totalDur)
			       << "\" duration=\"" << PackIsoDuration(remainDur) << "\">\n"
			       << "<BaseURL>" << mainBaseUrl << "</BaseURL>\n"
			       << trimmed
			       << "\n</Period>\n";
			totalDur += remainDur;
		}

		periodOffset += periodDur;
	}

	// Assemble final MPD
	std::string header = BuildMPDHeader(mainMpdText, totalDur);
	std::string stitched = header + output.str() + "\n</MPD>\n";

	AAMPLOG_WARN("[VodStitcher] Stitched MPD: %zu periods, total duration=%.3fs, size=%zu bytes",
	             (size_t)periodId, totalDur, stitched.size());
	return stitched;
}
