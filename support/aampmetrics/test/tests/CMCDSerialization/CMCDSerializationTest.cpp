/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 *   Copyright 2024 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/**
 * @file CMCDSerializationTest.cpp
 * @brief End-to-end compliance and lock-in tests for the CMCD serialization path.
 *
 * This suite links the REAL CMCDHeaders.cpp base, CMCDSerializer.cpp, and all four
 * real subclass .cpp files. It deliberately does NOT link the Fake library — the
 * no-op Fake CMCDHeaders::BuildCMCDCustomHeaders override would intercept the real
 * serializer and cause every assertion to pass vacuously (RESEARCH Pitfall 5).
 *
 * Tests cover CTA-5004 §3 requirements:
 *   SER-01 — br/tb rounded to nearest 100 kbps
 *   SER-02 — bl rounded to nearest 100 ms
 *   SER-03 — sid/nor/nrr serialized as quoted-string tokens
 *   SER-04 — keys alphabetically sorted within each group
 *   SER-05 — exact CMCD header group key names (with single trailing ':')
 *   SER-06 — bs emitted as bare token when starving; omitted when not
 *   SER-07 — each key appears in its correct CTA-5004 header group
 *   CMP-01 — Comcast custom keys (com.comcast-dns/fb/lb) retained in CMCD-Request
 *   CMP-02 — emission is purely a function of BuildCMCDCustomHeaders; the on/off
 *             gate lives in AampCMCDCollector::CMCDGetHeaders behind bCMCDEnabled
 *             and is not invoked here (do not duplicate that gate in this suite)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "CMCDSerializer.h"
#include "VideoCMCDHeaders.h"
#include "AudioCMCDHeaders.h"
#include "ManifestCMCDHeaders.h"
#include "SubtitleCMCDHeaders.h"

using ::testing::HasSubstr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * @brief Invoke BuildCMCDCustomHeaders on the given subclass instance and return the map.
 */
static std::unordered_map<std::string, std::vector<std::string>>
BuildHeaders(CMCDHeaders& obj)
{
    std::unordered_map<std::string, std::vector<std::string>> headers;
    obj.BuildCMCDCustomHeaders(headers);
    return headers;
}

/**
 * @brief Return the joined value string for a group key, or "" if the key is absent.
 *
 * Group key strings include the trailing ':' as emitted by CMCDGroupToHeaderKey
 * (e.g. "CMCD-Object:", "CMCD-Session:"). An absent key returns "".
 */
static std::string JoinedValue(
    const std::unordered_map<std::string, std::vector<std::string>>& headers,
    const std::string& groupKey)
{
    auto it = headers.find(groupKey);
    if (it == headers.end() || it->second.empty())
    {
        return "";
    }
    return it->second.at(0);
}

// ---------------------------------------------------------------------------
// SER-01: br and tb rounded to nearest 100 kbps
// ---------------------------------------------------------------------------

/**
 * SER-01: Video subclass rounds br and tb to nearest 100 kbps.
 * SetBitrate(3842)  -> br=3800  (3842 + 50 = 3892 -> /100=38 -> *100=3800)
 * SetTopBitrate(6100) -> tb=6100 (already multiple of 100)
 * SetTopBitrate(5950) -> tb=6000 (5950 + 50 = 6000 -> /100=60 -> *100=6000)
 */
TEST(CMCDSerialization_Video, RoundsBrAndTbToNearest100)
{
    VideoCMCDHeaders v;
    v.SetBitrate(3842);
    v.SetTopBitrate(6100);
    v.SetSessionId("test-sid");

    auto headers = BuildHeaders(v);
    const std::string obj = JoinedValue(headers, "CMCD-Object:");
    EXPECT_THAT(obj, HasSubstr("br=3800"));
    // 6100 is already on a 100 boundary; rounds to 6100
    EXPECT_THAT(obj, HasSubstr("tb=6100"));
}

/**
 * SER-01 (tb rounds down): SetTopBitrate(5942) -> tb=5900.
 */
TEST(CMCDSerialization_Video, RoundsTbDown)
{
    VideoCMCDHeaders v;
    v.SetBitrate(0);    // omitted (rounds to 0)
    v.SetTopBitrate(5942);
    v.SetSessionId("test-sid");

    auto headers = BuildHeaders(v);
    const std::string obj = JoinedValue(headers, "CMCD-Object:");
    EXPECT_THAT(obj, HasSubstr("tb=5900"));
}

// ---------------------------------------------------------------------------
// SER-02: bl rounded to nearest 100 ms
// ---------------------------------------------------------------------------

/**
 * SER-02: bufferLength in ms is rounded to nearest 100.
 * SetBufferLength(2367) -> bl=2400 (2367 + 50 = 2417 -> /100=24 -> *100=2400)
 */
TEST(CMCDSerialization_Video, BufferLengthRoundedToNearest100)
{
    VideoCMCDHeaders v;
    v.SetBufferLength(2367);
    v.SetNextUrl("../seg35.m4s");
    v.SetSessionId("test-sid");

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");
    EXPECT_THAT(req, HasSubstr("bl=2400"));
}

/**
 * SER-02 (bl rounds down): SetBufferLength(2349) -> bl=2300.
 */
TEST(CMCDSerialization_Video, BufferLengthRoundedDown)
{
    VideoCMCDHeaders v;
    v.SetBufferLength(2349);
    v.SetNextUrl("../seg35.m4s");
    v.SetSessionId("test-sid");

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");
    EXPECT_THAT(req, HasSubstr("bl=2300"));
}

// ---------------------------------------------------------------------------
// SER-03: quoted-string token types
// ---------------------------------------------------------------------------

/**
 * SER-03: sid is wrapped in double-quotes (quoted-string, not a bare token).
 */
TEST(CMCDSerialization_Session, SidIsQuoted)
{
    VideoCMCDHeaders v;
    v.SetSessionId("6e2fb550-c457");

    auto headers = BuildHeaders(v);
    EXPECT_EQ(JoinedValue(headers, "CMCD-Session:"), "sid=\"6e2fb550-c457\"");
}

/**
 * SER-03: nor value is wrapped in double-quotes in CMCD-Request.
 */
TEST(CMCDSerialization_Request, NorIsQuoted)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetNextUrl("../seg35.m4s");
    // dnsLookUptime = 0 and no range -> else branch -> nor="..."

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");
    // nor must appear with a quoted value
    EXPECT_THAT(req, HasSubstr("nor=\""));
}

/**
 * SER-03: nrr value is wrapped in double-quotes in CMCD-Request.
 */
TEST(CMCDSerialization_Request, NrrIsQuoted)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetNextRange("0-1048575");
    // mNextRange non-empty and dnsLookUptime==0 -> nrr branch

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");
    EXPECT_THAT(req, HasSubstr("nrr=\"0-1048575\""));
}

// ---------------------------------------------------------------------------
// SER-04: keys alphabetically sorted within each group
// ---------------------------------------------------------------------------

/**
 * SER-04 / SER-01 combined: CMCD-Object value is exactly "br=3800,ot=v,tb=6000"
 * (alphabetical: br < ot < tb; br=3842->3800, tb=6000 stays as 6000).
 */
TEST(CMCDSerialization_Object, KeysAlphabeticallySorted)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetBitrate(3842);   // rounds to 3800
    v.SetTopBitrate(6000); // stays 6000
    v.SetMediaType("VIDEO");  // ot=v

    auto headers = BuildHeaders(v);
    EXPECT_EQ(JoinedValue(headers, "CMCD-Object:"), "br=3800,ot=v,tb=6000");
}

/**
 * SER-04: CMCD-Request keys sorted: bl < com.comcast-dns < com.comcast-fb < com.comcast-lb < nor.
 * ASCII order: 'b' < 'c' < 'n'; com.comcast-* keys sort before nor.
 */
TEST(CMCDSerialization_Request, KeysAlphabeticallySortedWithComcastKeys)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetBufferLength(2000);
    v.SetNextUrl("../seg35.m4s");
    // Drive the dns>0 branch so all Comcast keys are emitted
    v.SetNetworkMetrics(2, 10, 5);  // firstByte=2, lastByte=10, dnsLookUptime=5

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");
    // Full expected value: bl=2000,com.comcast-dns=5,com.comcast-fb=2,com.comcast-lb=10,nor="..."
    EXPECT_THAT(req, HasSubstr("bl=2000"));
    EXPECT_THAT(req, HasSubstr("com.comcast-dns=5"));
    EXPECT_THAT(req, HasSubstr("com.comcast-fb=2"));
    EXPECT_THAT(req, HasSubstr("com.comcast-lb=10"));
    // com.comcast-* keys must appear BEFORE nor in the sorted string
    const std::size_t comPos = req.find("com.comcast-dns");
    const std::size_t norPos = req.find("nor=");
    EXPECT_LT(comPos, norPos) << "com.comcast-* keys must sort before nor (ASCII: c < n)";
}

// ---------------------------------------------------------------------------
// SER-05: exact CMCD group key names with single trailing ':'
// ---------------------------------------------------------------------------

/**
 * SER-05: The four group map keys are exactly "CMCD-Object:", "CMCD-Request:",
 * "CMCD-Session:", "CMCD-Status:".  One trailing ':' only, no double-colon,
 * no trailing space, no typo.
 */
TEST(CMCDSerialization_HeaderNames, ExactGroupKeys)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetBitrate(1000);
    v.SetTopBitrate(5000);
    v.SetBufferLength(1000);
    v.SetBufferStarvation(true);
    v.SetNextUrl("../seg.m4s");

    auto headers = BuildHeaders(v);

    // Exact key names must be present
    EXPECT_EQ(headers.count("CMCD-Object:"),  1u);
    EXPECT_EQ(headers.count("CMCD-Request:"), 1u);
    EXPECT_EQ(headers.count("CMCD-Session:"), 1u);
    EXPECT_EQ(headers.count("CMCD-Status:"),  1u);

    // No variants with double-colon or extra characters
    EXPECT_EQ(headers.count("CMCD-Object::"),  0u);
    EXPECT_EQ(headers.count("CMCD-Session::"), 0u);
    EXPECT_EQ(headers.count("CMCD-Request::"), 0u);
    EXPECT_EQ(headers.count("CMCD-Status::"),  0u);
    EXPECT_EQ(headers.count("CMCD-Object"),    0u);
    EXPECT_EQ(headers.count("CMCD-Session"),   0u);
}

// ---------------------------------------------------------------------------
// SER-06: bs bare token present/absent based on bufferStarvation
// ---------------------------------------------------------------------------

/**
 * SER-06: bufferStarvation=true emits CMCD-Status: with value "bs" (bare token, no '=').
 */
TEST(CMCDSerialization_Status, BsBareTokenWhenStarving)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetBufferStarvation(true);
    v.SetNextUrl("../seg.m4s");

    auto headers = BuildHeaders(v);
    EXPECT_EQ(JoinedValue(headers, "CMCD-Status:"), "bs");
}

/**
 * SER-06: bufferStarvation=false — "CMCD-Status:" key must be absent from the map.
 */
TEST(CMCDSerialization_Status, BsOmittedWhenNotStarving)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetBufferStarvation(false);
    v.SetNextUrl("../seg.m4s");

    auto headers = BuildHeaders(v);
    EXPECT_EQ(headers.find("CMCD-Status:"), headers.end())
        << "CMCD-Status: key must be absent when bufferStarvation is false";
}

// ---------------------------------------------------------------------------
// SER-07: key→group mapping correctness
// ---------------------------------------------------------------------------

/**
 * SER-07: ot/br/tb appear only in CMCD-Object; bl/nor/com.comcast-* only in CMCD-Request;
 * sid only in CMCD-Session; bs only in CMCD-Status.
 */
TEST(CMCDSerialization_GroupMapping, KeysInCorrectGroups)
{
    VideoCMCDHeaders v;
    v.SetSessionId("my-session-uuid");
    v.SetBitrate(2000);
    v.SetTopBitrate(4000);
    v.SetBufferLength(1500);
    v.SetBufferStarvation(true);
    v.SetNextUrl("../seg.m4s");

    auto headers = BuildHeaders(v);

    // ot, br, tb must be in CMCD-Object (not in Request/Status/Session)
    const std::string obj = JoinedValue(headers, "CMCD-Object:");
    EXPECT_THAT(obj, HasSubstr("ot="));
    EXPECT_THAT(obj, HasSubstr("br="));
    EXPECT_THAT(obj, HasSubstr("tb="));

    // bl and nor must be in CMCD-Request
    const std::string req = JoinedValue(headers, "CMCD-Request:");
    EXPECT_THAT(req, HasSubstr("bl="));
    EXPECT_THAT(req, HasSubstr("nor="));

    // sid must be in CMCD-Session and not leak into Object/Request/Status
    EXPECT_THAT(JoinedValue(headers, "CMCD-Session:"), HasSubstr("sid="));
    EXPECT_THAT(obj, ::testing::Not(HasSubstr("sid=")));
    EXPECT_THAT(req, ::testing::Not(HasSubstr("sid=")));

    // bs must be in CMCD-Status and not leak into Object/Request
    const std::string status = JoinedValue(headers, "CMCD-Status:");
    EXPECT_EQ(status, "bs");
    EXPECT_THAT(obj, ::testing::Not(HasSubstr("bs")));
    EXPECT_THAT(req, ::testing::Not(HasSubstr("bs")));
}

// ---------------------------------------------------------------------------
// CMP-01: Comcast custom keys retained in CMCD-Request
// ---------------------------------------------------------------------------

/**
 * CMP-01: com.comcast-dns, com.comcast-fb, com.comcast-lb all appear in
 * CMCD-Request when network metrics are set (dns > 0 branch).
 */
TEST(CMCDSerialization_Comcast, CustomKeysRetained)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetNextUrl("../seg.m4s");
    // SetNetworkMetrics(startTransferTime=fb, totalTime=lb, dnsLookUpTime=dns)
    v.SetNetworkMetrics(2, 10, 5);  // firstByte=2, lastByte=10, dnsLookUptime=5

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");

    EXPECT_THAT(req, HasSubstr("com.comcast-dns=5"));
    EXPECT_THAT(req, HasSubstr("com.comcast-fb=2"));
    EXPECT_THAT(req, HasSubstr("com.comcast-lb=10"));
}

/**
 * CMP-01 (nrr branch): com.comcast-fb and com.comcast-lb present when using
 * byte-range requests (nrr branch).
 */
TEST(CMCDSerialization_Comcast, CustomFbLbRetainedInNrrBranch)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetNextRange("0-1048575");
    v.SetNetworkMetrics(3, 15, 0);  // dns=0, so nrr branch; fb=3, lb=15

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");

    EXPECT_THAT(req, HasSubstr("com.comcast-fb=3"));
    EXPECT_THAT(req, HasSubstr("com.comcast-lb=15"));
    // nrr present and quoted
    EXPECT_THAT(req, HasSubstr("nrr=\"0-1048575\""));
    // dns must NOT appear (it was 0 so we took nrr branch, not dns branch)
    EXPECT_THAT(req, ::testing::Not(HasSubstr("com.comcast-dns")));
}

// ---------------------------------------------------------------------------
// Integration: base Session + subclass Object/Request/Status all present together
// ---------------------------------------------------------------------------

/**
 * Integration: both base CMCD-Session (from CMCDHeaders::BuildCMCDCustomHeaders)
 * and subclass CMCD-Object/Request/Status entries coexist in the output map.
 * The base's CMCD-Session: entry must not be overwritten by the subclass call.
 * (SerializeToCMCDMap merges without clearing — this test confirms that contract.)
 */
TEST(CMCDSerialization_Video, BaseSessionAndSubclassGroupsCoexist)
{
    VideoCMCDHeaders v;
    v.SetSessionId("6e2fb550-c457");
    v.SetBitrate(2000);
    v.SetTopBitrate(5000);
    v.SetBufferLength(1000);
    v.SetNextUrl("../seg.m4s");

    auto headers = BuildHeaders(v);

    // Base CMCD-Session: must be present and correctly quoted
    EXPECT_EQ(JoinedValue(headers, "CMCD-Session:"), "sid=\"6e2fb550-c457\"");

    // Subclass CMCD-Object: must also be present
    const std::string obj = JoinedValue(headers, "CMCD-Object:");
    EXPECT_FALSE(obj.empty()) << "CMCD-Object: must be present alongside CMCD-Session:";
    EXPECT_THAT(obj, HasSubstr("ot=v"));

    // Subclass CMCD-Request: must also be present
    const std::string req = JoinedValue(headers, "CMCD-Request:");
    EXPECT_FALSE(req.empty()) << "CMCD-Request: must be present alongside CMCD-Session:";
}

// ---------------------------------------------------------------------------
// Manifest subclass: ot=m in CMCD-Object, Session still present
// ---------------------------------------------------------------------------

/**
 * CMP-02 / SER-07: ManifestCMCDHeaders emits ot=m in CMCD-Object and a quoted
 * sid in CMCD-Session. It does not emit CMCD-Request or CMCD-Status (no bl/nor/bs
 * for manifests in the current implementation).
 *
 * Note: The on/off gate for all CMCD emission lives in
 * AampCMCDCollector::CMCDGetHeaders behind the bCMCDEnabled flag, which is not
 * invoked by BuildCMCDCustomHeaders. This test exercises the serializer path
 * directly; CMP-02 gate coverage is a collector-level concern.
 */
TEST(CMCDSerialization_Manifest, EmitsOtMAndQuotedSid)
{
    ManifestCMCDHeaders m;
    m.SetSessionId("manifest-session-id");

    auto headers = BuildHeaders(m);

    EXPECT_EQ(JoinedValue(headers, "CMCD-Object:"), "ot=m");
    EXPECT_EQ(JoinedValue(headers, "CMCD-Session:"), "sid=\"manifest-session-id\"");
    // Manifest does not carry bl/nor/bs — Request and Status must be absent
    EXPECT_EQ(headers.find("CMCD-Request:"), headers.end())
        << "CMCD-Request: must be absent for manifest requests";
    EXPECT_EQ(headers.find("CMCD-Status:"), headers.end())
        << "CMCD-Status: must be absent for manifest requests";
}

// ---------------------------------------------------------------------------
// Subtitle subclass: ot=s
// ---------------------------------------------------------------------------

/**
 * SER-07: SubtitleCMCDHeaders emits ot=s in CMCD-Object and a quoted sid.
 */
TEST(CMCDSerialization_Subtitle, EmitsOtS)
{
    SubtitleCMCDHeaders s;
    s.SetSessionId("subtitle-session-id");

    auto headers = BuildHeaders(s);

    EXPECT_EQ(JoinedValue(headers, "CMCD-Object:"), "ot=s");
    EXPECT_EQ(JoinedValue(headers, "CMCD-Session:"), "sid=\"subtitle-session-id\"");
}

// ---------------------------------------------------------------------------
// Audio subclass: ot=a and ot=i
// ---------------------------------------------------------------------------

/**
 * SER-07: AudioCMCDHeaders emits ot=a for normal audio and ot=i for init segments.
 */
TEST(CMCDSerialization_Audio, EmitsOtA)
{
    AudioCMCDHeaders a;
    a.SetSessionId("audio-session-id");
    a.SetMediaType("AUDIO");  // -> ot=a
    a.SetBitrate(256);
    a.SetTopBitrate(320);

    auto headers = BuildHeaders(a);

    const std::string obj = JoinedValue(headers, "CMCD-Object:");
    EXPECT_THAT(obj, HasSubstr("ot=a"));
    EXPECT_EQ(JoinedValue(headers, "CMCD-Session:"), "sid=\"audio-session-id\"");
}

TEST(CMCDSerialization_Audio, EmitsOtIForInitSegment)
{
    AudioCMCDHeaders a;
    a.SetSessionId("audio-session-id");
    a.SetMediaType("INIT_AUDIO");  // -> ot=i
    a.SetBitrate(256);
    a.SetTopBitrate(320);

    auto headers = BuildHeaders(a);

    const std::string obj = JoinedValue(headers, "CMCD-Object:");
    EXPECT_THAT(obj, HasSubstr("ot=i"));
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

/**
 * SER-06 / omit-zero: br and tb that round to zero are omitted from CMCD-Object.
 * Setting both to 0 means the Object group still has ot (a bare token) but no br/tb.
 */
TEST(CMCDSerialization_Video, ZeroBitrateOmittedFromObject)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    v.SetBitrate(0);
    v.SetTopBitrate(0);
    v.SetNextUrl("../seg.m4s");

    auto headers = BuildHeaders(v);
    const std::string obj = JoinedValue(headers, "CMCD-Object:");
    // ot=v must still be present
    EXPECT_THAT(obj, HasSubstr("ot=v"));
    // br and tb must NOT appear when zero
    EXPECT_THAT(obj, ::testing::Not(HasSubstr("br=")));
    EXPECT_THAT(obj, ::testing::Not(HasSubstr("tb=")));
}

/**
 * CMP-02 / empty-state path: calling BuildCMCDCustomHeaders on a default-constructed
 * subclass does not crash and emits a CMCD-Session: key (with an empty-string sid).
 * This confirms the serializer handles zero-value state gracefully.
 */
TEST(CMCDSerialization_EmptyState, DoesNotCrash)
{
    ManifestCMCDHeaders m;
    // No setters called — all members at their default zero/empty values.

    std::unordered_map<std::string, std::vector<std::string>> headers;
    ASSERT_NO_FATAL_FAILURE(m.BuildCMCDCustomHeaders(headers));

    // CMCD-Object: must be present (ot=m)
    EXPECT_EQ(JoinedValue(headers, "CMCD-Object:"), "ot=m");
    // CMCD-Session: must be present (sid="")
    EXPECT_EQ(JoinedValue(headers, "CMCD-Session:"), "sid=\"\"");
}

// ---------------------------------------------------------------------------
// WR-01 regression: nor must not appear when nextUrl is empty (IN-03)
// ---------------------------------------------------------------------------

/**
 * WR-01 / IN-03: Default-constructed VideoCMCDHeaders (no nextUrl set) must not
 * emit nor at all. The empty-nor="" emission violated CTA-5004 §3 optional-key rule.
 * Covers both the dns=0/range-empty else-branch and the dns>0 branch.
 */
TEST(CMCDSerialization_Video, NorOmittedWhenNextUrlEmpty)
{
    VideoCMCDHeaders v;
    v.SetSessionId("test-sid");
    // No SetNextUrl — nextUrl remains empty string.
    // No SetNextRange — mNextRange remains empty string.
    // dnsLookUptime defaults to 0.

    auto headers = BuildHeaders(v);
    const std::string req = JoinedValue(headers, "CMCD-Request:");

    // nor must not appear at all when nextUrl is empty.
    EXPECT_THAT(req, ::testing::Not(HasSubstr("nor=")));
}

/**
 * WR-01 regression (Audio): same guard applies to AudioCMCDHeaders.
 */
TEST(CMCDSerialization_Audio, NorOmittedWhenNextUrlEmpty)
{
    AudioCMCDHeaders a;
    a.SetSessionId("test-sid");
    // No SetNextUrl — nextUrl remains empty string.

    auto headers = BuildHeaders(a);
    const std::string req = JoinedValue(headers, "CMCD-Request:");

    EXPECT_THAT(req, ::testing::Not(HasSubstr("nor=")));
}

// ---------------------------------------------------------------------------
// WR-03 regression: SerializeToCMCDMap merges rather than overwrites same group
// ---------------------------------------------------------------------------

/**
 * WR-03: Two sequential SerializeToCMCDMap calls that both target CMCD-Session
 * must produce a single alphabetically-sorted value containing tokens from both
 * calls — not just the second call's tokens.
 *
 * Simulates a future pattern where a base class seeds sid and a subclass adds
 * additional Session keys (e.g. cid, sf, st) via a second SerializeToCMCDMap call.
 */
TEST(CMCDSerialization_Merge, TwoCallsSameGroupMergesAndSorts)
{
    std::unordered_map<std::string, std::vector<std::string>> out;

    // First call: seed CMCD-Session with sid.
    std::vector<CMCDEntry> first{
        CMCDEntry{"sid", "my-session", CMCDGroup::Session, false, true}
    };
    SerializeToCMCDMap(first, out);

    // Confirm first call wrote CMCD-Session.
    ASSERT_EQ(out.count("CMCD-Session:"), 1u);
    EXPECT_EQ(out.at("CMCD-Session:").at(0), "sid=\"my-session\"");

    // Second call: add sf and st to the same CMCD-Session group.
    std::vector<CMCDEntry> second{
        CMCDEntry{"sf", "d", CMCDGroup::Session},   // bare token: sf=d
        CMCDEntry{"st", "v", CMCDGroup::Session}    // bare token: st=v
    };
    SerializeToCMCDMap(second, out);

    // CMCD-Session must now contain all three tokens, alpha-sorted: sf < sid < st.
    const std::string session = JoinedValue(out, "CMCD-Session:");
    EXPECT_EQ(session, "sf=d,sid=\"my-session\",st=v")
        << "Merged CMCD-Session must be alphabetically sorted: sf < sid < st";

    // Confirm sid token was not dropped by the merge.
    EXPECT_THAT(session, HasSubstr("sid=\"my-session\""));
}
