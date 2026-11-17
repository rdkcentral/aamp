/*
 * AampAvlog.h - AAMP PTS bookkeeping records for avlog.ko, written through the
 * avlog HAL (libavloghal.so.1 from the vendor layer).
 *
 * The HAL is loaded at run time with dlopen(), so AAMP has no build dependency on
 * the vendor layer and runs unchanged when the library or avlog.ko is missing
 * (every call is then a no-op). dlopen() is in libc since glibc 2.34; older
 * toolchains need -ldl.
 *
 * Records:
 *   AAMP_JUMP     MediaTrack::HandleFragmentPositionJump: the new fragment does
 *                 not start where the last one ended. Written for any non-zero
 *                 delta (AAMP itself only acts on delta > 0); asks avlog.ko for a
 *                 capture window when |delta| >= AAMP_AVLOG_ARM_US.
 *   AAMP_RESTAMP  every MediaTrack call site of IsoBmffHelper::RestampPts (whole
 *                 fragments and LL-DASH chunks): the PTS offset AAMP applies to
 *                 this track changed; written once per change with the old and new
 *                 offset and the position of the first fragment carrying it. A
 *                 change while playing (previous offset != 0) also asks for a
 *                 capture window; the first offset after start is recorded only.
 *   AAMP_PERIOD   StreamAbstractionAAMP_MPD::UpdatePtsOffset at a new period: the
 *                 per-track period start and duration AAMP reads from the manifest
 *                 timeline and the PTS offset step it derives from them (offset +=
 *                 min(start) of the old period + max(duration) - min(start) of the new
 *                 one), plus whether a DAI ad is known for the period and AAMP's ad state.
 *   AAMP_AD       PrivateInstanceAAMP::FoundEventBreak: an ad break or ad announced in
 *                 the manifest (SCTE-35 / CDAI event), with its duration.
 *   PERIOD and AD are sent straight to /dev/avlog with AVLOG_IOC_PUT_REC (no HAL
 *   entry point needed, so the vendor layer is unchanged).
 * A failed write is reported once per process on stderr (AAMP log).
 * adPlaying comes from the fragment (CachedFragment::adPlaying, set from the CDAI
 * ad state in CacheFragment). A change of the ad flag also writes an AAMP_RESTAMP
 * record (offset unchanged, no capture window), so every ad start/end is marked.
 */
#ifndef AAMP_AVLOG_H
#define AAMP_AVLOG_H

#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>

/* arm a capture window only for a real jump, not for float noise */
#define AAMP_AVLOG_ARM_US        1000

/* values from <linux/avlog.h>: enum avlog_media, AVLOG_UF_ARM_TIMER */
#define AAMP_AVLOG_MEDIA_VIDEO   1u
#define AAMP_AVLOG_MEDIA_AUDIO   2u
#define AAMP_AVLOG_MEDIA_SUB     3u
#define AAMP_AVLOG_UF_ARM_TIMER  (1u << 0)
#define AAMP_AVLOG_TYPE_USER     6u
#define AAMP_AVLOG_USER_PERIOD   3u        /* AVLOG_USER_AAMP_PERIOD */
#define AAMP_AVLOG_USER_AD       4u        /* AVLOG_USER_AAMP_AD */
#define AAMP_AVLOG_PERIOD_DAI_AD     (1u << 16)  /* period word: a DAI ad is known for this period */
#define AAMP_AVLOG_PERIOD_AD_PLAYING (1u << 17)  /* period word: AAMP ad state IN_ADBREAK_AD_PLAYING */
#define AAMP_AVLOG_AD_BREAK      1u        /* avlog_aamp_ad.event: ad break announced (isDAIEvent 0) */
#define AAMP_AVLOG_AD_AD         2u        /* avlog_aamp_ad.event: ad announced (isDAIEvent 1) */
/* AVLOG_IOC_PUT_REC = _IOW(0xA7, 1, struct avlog_user), struct avlog_user = 13 x __u32 */
#define AAMP_AVLOG_IOC_PUT_REC   _IOW(0xA7, 1, uint32_t[13])

/* the HAL entry points used here (avlog_hal.h) */
struct AampAvlogHal
{
    int (*init)(void) = nullptr;
    int (*jump)(uint32_t media, uint32_t flags, long long positionDeltaUs, long long lastInjectedUs,
                long long absPositionUs, uint32_t seq, uint32_t adPlaying) = nullptr;
    int (*restamp)(uint32_t media, uint32_t flags, long long ptsOffsetUs, long long prevOffsetUs,
                   long long absPositionUs, uint32_t timescale, uint32_t discontinuity,
                   uint32_t adPlaying) = nullptr;
    std::atomic<bool> ready{false};

    AampAvlogHal()
    {
        void *lib = dlopen("libavloghal.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!lib)
            return;
        init = reinterpret_cast<decltype(init)>(dlsym(lib, "avlog_hal_init"));
        jump = reinterpret_cast<decltype(jump)>(dlsym(lib, "avlog_hal_aamp_jump"));
        restamp = reinterpret_cast<decltype(restamp)>(dlsym(lib, "avlog_hal_aamp_restamp"));
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

/* HandleFragmentPositionJump: called for every fragment; records only a non-zero delta */
static inline void AampAvlogPositionJump(int trackType, double positionDelta,
                                         double lastInjectedDuration, double absPosition,
                                         bool adPlaying)
{
    static std::atomic<uint32_t> seq{0};
    const long long deltaUs = AampAvlogUs(positionDelta);
    if (deltaUs == 0)
        return;
    AampAvlogHal *hal = AampAvlogGet();
    if (!hal)
        return;

    const uint32_t media = AampAvlogMedia(trackType);
    uint32_t flags = 0;
    if ((media == AAMP_AVLOG_MEDIA_VIDEO || media == AAMP_AVLOG_MEDIA_AUDIO) &&
        (deltaUs >= AAMP_AVLOG_ARM_US || deltaUs <= -AAMP_AVLOG_ARM_US))
        flags |= AAMP_AVLOG_UF_ARM_TIMER;
    AampAvlogCheck(hal->jump(media, flags, deltaUs, AampAvlogUs(lastInjectedDuration),
                             AampAvlogUs(absPosition), ++seq, adPlaying ? 1u : 0u), "AAMP_JUMP");
}

/* RestampPts call sites: called for every restamped fragment/chunk; records a change of offset */
static inline void AampAvlogRestamp(int trackType, double ptsOffsetSec, double absPosition,
                                    uint32_t timeScale, uint64_t discontinuityIndex, bool adPlaying)
{
    static long long last[3] = { 0, 0, 0 };   /* per track type, process-wide */
    static bool lastAd[3] = { false, false, false };
    const long long offUs = AampAvlogUs(ptsOffsetSec);
    if (trackType < 0 || trackType > 2 || (offUs == last[trackType] && adPlaying == lastAd[trackType]))
        return;
    const long long prevUs = last[trackType];
    last[trackType] = offUs;
    lastAd[trackType] = adPlaying;
    AampAvlogHal *hal = AampAvlogGet();
    if (!hal)
        return;

    const uint32_t media = AampAvlogMedia(trackType);
    uint32_t flags = 0;
    if (prevUs != 0 && offUs != prevUs && (media == AAMP_AVLOG_MEDIA_VIDEO || media == AAMP_AVLOG_MEDIA_AUDIO))
        flags |= AAMP_AVLOG_UF_ARM_TIMER;      /* offset changed while playing */
    AampAvlogCheck(hal->restamp(media, flags, offUs, prevUs, AampAvlogUs(absPosition), timeScale,
                                static_cast<uint32_t>(discontinuityIndex), adPlaying ? 1u : 0u),
                   "AAMP_RESTAMP");
}

/* ---- PERIOD / AD: 52-byte USER payloads sent straight to /dev/avlog ---- */

/* one fd per translation unit that writes PERIOD/AD records (two in AAMP); retried while
 * avlog.ko is not loaded, at most once a second */
static inline int AampAvlogFd(void)
{
    static std::atomic<int> fd{-1};
    static std::atomic<long long> lastTry{0};
    int f = fd.load(std::memory_order_acquire);
    if (f >= 0)
        return f;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ts.tv_sec == lastTry.exchange(ts.tv_sec))
        return -1;
    f = open("/dev/avlog", O_WRONLY | O_CLOEXEC);
    if (f < 0)
        return -1;
    int expected = -1;
    if (!fd.compare_exchange_strong(expected, f))
    {
        close(f);                       /* another thread opened it first */
        return expected;
    }
    return f;
}

static inline void AampAvlogPut(const uint32_t rec[13], const char *what)
{
    int f = AampAvlogFd();
    if (f < 0)
        return;
    int rc;
    do
        rc = ioctl(f, AAMP_AVLOG_IOC_PUT_REC, rec);
    while (rc < 0 && errno == EINTR);
    AampAvlogCheck(rc < 0 ? -errno : 0, what);
}

/* FNV-1a of the DASH period id, so a record can be matched to the AAMP log line */
static inline uint32_t AampAvlogId(const std::string &id)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : id)
        h = (h ^ c) * 16777619u;
    return h;
}

static inline void AampAvlogLoHi(uint32_t *w, double seconds)
{
    const uint64_t v = static_cast<uint64_t>(AampAvlogUs(seconds));
    w[0] = static_cast<uint32_t>(v);
    w[1] = static_cast<uint32_t>(v >> 32);
}

/* UpdatePtsOffset, new period: what AAMP read from the timeline and what it did with it */
static inline void AampAvlogPeriod(int periodIdx, const std::string &periodId,
                            double audioStart, double videoStart,
                            double audioDuration, double videoDuration,
                            double offsetStep, bool daiAd, bool adPlaying)
{
    uint32_t r[13] = {0};
    r[0] = AAMP_AVLOG_TYPE_USER;
    r[1] = AAMP_AVLOG_USER_PERIOD;
    r[2] = 0;                                           /* media: any */
    r[3] = 0;                                           /* flags: never arms */
    r[4] = (static_cast<uint32_t>(periodIdx) & 0xFFFFu) |
           (daiAd ? AAMP_AVLOG_PERIOD_DAI_AD : 0u) | (adPlaying ? AAMP_AVLOG_PERIOD_AD_PLAYING : 0u);
    r[5] = AampAvlogId(periodId);
    AampAvlogLoHi(&r[6], audioStart);
    AampAvlogLoHi(&r[8], videoStart);
    r[10] = static_cast<uint32_t>(AampAvlogUs(audioDuration));
    r[11] = static_cast<uint32_t>(AampAvlogUs(videoDuration));
    r[12] = static_cast<uint32_t>(static_cast<int32_t>(AampAvlogUs(offsetStep)));   /* signed */
    AampAvlogPut(r, "AAMP_PERIOD");
}

/* FoundEventBreak: an ad break (isDAIEvent 0) or an ad (isDAIEvent 1) announced in the manifest */
static inline void AampAvlogAd(const std::string &periodId, uint32_t durationMs, bool isDaiEvent, uint64_t startMs)
{
    uint32_t r[13] = {0};
    r[0] = AAMP_AVLOG_TYPE_USER;
    r[1] = AAMP_AVLOG_USER_AD;
    r[2] = 0;
    r[3] = 0;
    r[4] = isDaiEvent ? AAMP_AVLOG_AD_AD : AAMP_AVLOG_AD_BREAK;
    r[5] = AampAvlogId(periodId);
    r[6] = durationMs;
    r[7] = static_cast<uint32_t>(startMs);
    r[8] = static_cast<uint32_t>(startMs >> 32);
    AampAvlogPut(r, "AAMP_AD");
}

#endif /* AAMP_AVLOG_H */
