/*
 * AampAvlog.h - AAMP PTS bookkeeping records for avlog.ko, written through the
 * avlog HAL (libavloghal.so.1 from the vendor layer).
 *
 * The HAL is loaded at run time with dlopen(), so AAMP has no build dependency on
 * the vendor layer and runs unchanged when the library or avlog.ko is missing
 * (every call is then a no-op). dlopen() is in libc since glibc 2.34; older
 * toolchains need -ldl.
 *
 * Records (video and audio only; subtitle records duplicated video and are not written):
 *   AAMP_JUMP     MediaTrack::HandleFragmentPositionJump: the new fragment does
 *                 not start where the last one ended. Written for |delta| > 10 us
 *                 (smaller values are float rounding); arms a capture window when
 *                 |delta| >= AAMP_AVLOG_ARM_US.
 *   AAMP_RESTAMP  every MediaTrack call site of IsoBmffHelper::RestampPts: the PTS
 *                 offset applied to this track or the ad flag changed; written once
 *                 per change. Arms a capture window when the offset moves by
 *                 >= AAMP_AVLOG_ARM_US while playing, and once per ad break at the
 *                 return to the programme (video ad flag 1 -> 0).
 *   AAMP_PERIOD   StreamAbstractionAAMP_MPD::UpdatePtsOffset at every new DASH period,
 *                 also when the offset step is 0: per-track start/duration from the
 *                 manifest timeline and the offset step. Written when the period is
 *                 selected for download, ahead of injection. Never arms.
 *   AAMP_AD       PrivateInstanceAAMP::FoundEventBreak: ad break or ad announced in
 *                 the manifest. Never arms.
 * AAMP_PERIOD and AAMP_AD use the HAL's generic avlog_hal_write(); with an older HAL
 * without it they are skipped and JUMP/RESTAMP still work.
 * A failed write is reported once per process on stderr (AAMP log).
 * adPlaying comes from the fragment (CachedFragment::adPlaying).
 */
#ifndef AAMP_AVLOG_H
#define AAMP_AVLOG_H

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

/* arm a capture window only above the normal 0/8/16 ms period steps and the
 * <= 24 ms jumps seen at every ad boundary */
#define AAMP_AVLOG_ARM_US        50000
/* |jump| at or below this is float rounding, not recorded */
#define AAMP_AVLOG_NOISE_US      10

/* values from <linux/avlog.h> */
#define AAMP_AVLOG_TYPE_USER     6u
#define AAMP_AVLOG_USER_PERIOD   3u
#define AAMP_AVLOG_USER_AD       4u
#define AAMP_AVLOG_MEDIA_ANY     0u
#define AAMP_AVLOG_MEDIA_VIDEO   1u
#define AAMP_AVLOG_MEDIA_AUDIO   2u
#define AAMP_AVLOG_MEDIA_SUB     3u
#define AAMP_AVLOG_UF_ARM_TIMER  (1u << 0)
#define AAMP_AVLOG_PERIOD_IDX_MASK   0xFFFFu
#define AAMP_AVLOG_PERIOD_AD_PLAYING (1u << 17)
#define AAMP_AVLOG_AD_BREAK      1u
#define AAMP_AVLOG_AD_AD         2u
#define AAMP_AVLOG_PAYLOAD_WORDS 13      /* 52 bytes */

/* the HAL entry points used here (avlog_hal.h) */
struct AampAvlogHal
{
    int (*init)(void) = nullptr;
    int (*jump)(uint32_t media, uint32_t flags, long long positionDeltaUs, long long lastInjectedUs,
                long long absPositionUs, uint32_t seq, uint32_t adPlaying) = nullptr;
    int (*restamp)(uint32_t media, uint32_t flags, long long ptsOffsetUs, long long prevOffsetUs,
                   long long absPositionUs, uint32_t timescale, uint32_t discontinuity,
                   uint32_t adPlaying) = nullptr;
    int (*write)(const void *payload) = nullptr;   /* optional; 52-byte payload */
    std::atomic<bool> ready{false};

    AampAvlogHal()
    {
        void *lib = dlopen("libavloghal.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!lib)
            return;
        init = reinterpret_cast<decltype(init)>(dlsym(lib, "avlog_hal_init"));
        jump = reinterpret_cast<decltype(jump)>(dlsym(lib, "avlog_hal_aamp_jump"));
        restamp = reinterpret_cast<decltype(restamp)>(dlsym(lib, "avlog_hal_aamp_restamp"));
        write = reinterpret_cast<decltype(write)>(dlsym(lib, "avlog_hal_write"));
        if (!jump || !restamp)
            init = nullptr;
    }
};

/* the HAL, or nullptr when it cannot log; init is retried because avlog.ko may load after AAMP started */
static inline AampAvlogHal *AampAvlogGet(void)
{
    static AampAvlogHal hal;
    if (!hal.init)
        return nullptr;
    if (!hal.ready.load(std::memory_order_acquire))
    {
        static std::atomic_flag busy = ATOMIC_FLAG_INIT;
        if (busy.test_and_set(std::memory_order_acquire))
            return nullptr;             /* another track is opening it; skip this record */
        if (!hal.ready.load(std::memory_order_acquire) && hal.init() == 0)
            hal.ready.store(true, std::memory_order_release);
        busy.clear(std::memory_order_release);
        if (!hal.ready.load(std::memory_order_acquire))
            return nullptr;
    }
    return &hal;
}

/* report the first failed write (e.g. avlog.ko rejecting the record) */
static inline void AampAvlogCheck(int rc, const char *what)
{
    static std::atomic<bool> reported{false};
    if (rc < 0 && !reported.exchange(true))
        fprintf(stderr, "AampAvlog: %s record not written: %s\n", what, strerror(-rc));
}

static inline uint32_t AampAvlogMedia(int trackType)
{
    return trackType == 0 ? AAMP_AVLOG_MEDIA_VIDEO :
           trackType == 1 ? AAMP_AVLOG_MEDIA_AUDIO :
           trackType == 2 ? AAMP_AVLOG_MEDIA_SUB : 0;
}

static inline long long AampAvlogUs(double seconds)
{
    return std::llround(seconds * 1e6);
}

static inline bool AampAvlogArm(long long us)
{
    return us >= AAMP_AVLOG_ARM_US || us <= -AAMP_AVLOG_ARM_US;
}

/* FNV-1a of a period id string, as in struct avlog_aamp_period / avlog_aamp_ad */
static inline uint32_t AampAvlogHash(const std::string &s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s)
        h = (h ^ c) * 16777619u;
    return h;
}

static inline void AampAvlogPut64(uint32_t *w, long long v)
{
    w[0] = static_cast<uint32_t>(static_cast<unsigned long long>(v) & 0xFFFFFFFFu);
    w[1] = static_cast<uint32_t>(static_cast<unsigned long long>(v) >> 32);
}

static inline uint32_t AampAvlogU32(long long v)
{
    return v < 0 ? 0u : v > 0xFFFFFFFFLL ? 0xFFFFFFFFu : static_cast<uint32_t>(v);
}

/* HandleFragmentPositionJump: called for every fragment; records a real non-zero delta */
static inline void AampAvlogPositionJump(int trackType, double positionDelta,
                                         double lastInjectedDuration, double absPosition,
                                         bool adPlaying)
{
    static std::atomic<uint32_t> seq{0};
    const uint32_t media = AampAvlogMedia(trackType);
    const long long deltaUs = AampAvlogUs(positionDelta);
    if (media != AAMP_AVLOG_MEDIA_VIDEO && media != AAMP_AVLOG_MEDIA_AUDIO)
        return;
    if (deltaUs <= AAMP_AVLOG_NOISE_US && deltaUs >= -AAMP_AVLOG_NOISE_US)
        return;
    AampAvlogHal *hal = AampAvlogGet();
    if (!hal)
        return;

    const uint32_t flags = AampAvlogArm(deltaUs) ? AAMP_AVLOG_UF_ARM_TIMER : 0u;
    AampAvlogCheck(hal->jump(media, flags, deltaUs, AampAvlogUs(lastInjectedDuration),
                             AampAvlogUs(absPosition), ++seq, adPlaying ? 1u : 0u), "AAMP_JUMP");
}

/* RestampPts call sites: called for every restamped fragment/chunk; records a change
 * of offset or ad flag */
static inline void AampAvlogRestamp(int trackType, double ptsOffsetSec, double absPosition,
                                    uint32_t timeScale, uint64_t discontinuityIndex, bool adPlaying)
{
    static long long last[2] = { 0, 0 };          /* video, audio; process-wide */
    static bool lastAd[2] = { false, false };
    const uint32_t media = AampAvlogMedia(trackType);
    if (media != AAMP_AVLOG_MEDIA_VIDEO && media != AAMP_AVLOG_MEDIA_AUDIO)
        return;
    const int i = trackType;                      /* 0 video, 1 audio */
    const long long offUs = AampAvlogUs(ptsOffsetSec);
    if (offUs == last[i] && adPlaying == lastAd[i])
        return;
    const long long prevUs = last[i];
    const bool prevAd = lastAd[i];
    last[i] = offUs;
    lastAd[i] = adPlaying;
    AampAvlogHal *hal = AampAvlogGet();
    if (!hal)
        return;

    uint32_t flags = 0;
    if (prevUs != 0 && AampAvlogArm(offUs - prevUs))
        flags |= AAMP_AVLOG_UF_ARM_TIMER;      /* large offset change while playing */
    if (media == AAMP_AVLOG_MEDIA_VIDEO && prevAd && !adPlaying)
        flags |= AAMP_AVLOG_UF_ARM_TIMER;      /* return from the ad break: one window per break */
    AampAvlogCheck(hal->restamp(media, flags, offUs, prevUs, AampAvlogUs(absPosition), timeScale,
                                static_cast<uint32_t>(discontinuityIndex), adPlaying ? 1u : 0u),
                   "AAMP_RESTAMP");
}

/* UpdatePtsOffset at a new period: struct avlog_aamp_period */
static inline void AampAvlogPeriod(int periodIdx, const std::string &periodId,
                                   double audioStart, double videoStart,
                                   double audioDuration, double videoDuration,
                                   double offsetStep, bool adPlaying)
{
    AampAvlogHal *hal = AampAvlogGet();
    if (!hal || !hal->write)
        return;
    uint32_t w[AAMP_AVLOG_PAYLOAD_WORDS] = {};
    w[0] = AAMP_AVLOG_TYPE_USER;
    w[1] = AAMP_AVLOG_USER_PERIOD;
    w[2] = AAMP_AVLOG_MEDIA_ANY;
    w[3] = 0;                                   /* never arms */
    w[4] = (static_cast<uint32_t>(periodIdx) & AAMP_AVLOG_PERIOD_IDX_MASK) |
           (adPlaying ? AAMP_AVLOG_PERIOD_AD_PLAYING : 0u);
    w[5] = AampAvlogHash(periodId);
    AampAvlogPut64(&w[6], AampAvlogUs(audioStart));
    AampAvlogPut64(&w[8], AampAvlogUs(videoStart));
    w[10] = AampAvlogU32(AampAvlogUs(audioDuration));
    w[11] = AampAvlogU32(AampAvlogUs(videoDuration));
    w[12] = static_cast<uint32_t>(AampAvlogUs(offsetStep));   /* signed */
    AampAvlogCheck(hal->write(w), "AAMP_PERIOD");
}

/* FoundEventBreak: struct avlog_aamp_ad */
static inline void AampAvlogAd(bool isDAIEvent, const std::string &periodId,
                               uint32_t durationMs, uint64_t startMs)
{
    AampAvlogHal *hal = AampAvlogGet();
    if (!hal || !hal->write)
        return;
    uint32_t w[AAMP_AVLOG_PAYLOAD_WORDS] = {};
    w[0] = AAMP_AVLOG_TYPE_USER;
    w[1] = AAMP_AVLOG_USER_AD;
    w[2] = AAMP_AVLOG_MEDIA_ANY;
    w[3] = 0;                                   /* never arms */
    w[4] = isDAIEvent ? AAMP_AVLOG_AD_AD : AAMP_AVLOG_AD_BREAK;
    w[5] = AampAvlogHash(periodId);
    w[6] = durationMs;
    AampAvlogPut64(&w[7], static_cast<long long>(startMs));
    AampAvlogCheck(hal->write(w), "AAMP_AD");
}

#endif /* AAMP_AVLOG_H */
