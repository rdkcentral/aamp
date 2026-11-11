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
 * A failed write is reported once per process on stderr (AAMP log).
 * The adPlaying field is always 0 in this build: the 2.11.1.x VIPA train has no
 * IsAdPlaying(); ad periods show as Idx >= 1 in GetStartAndDurationForPtsRestamping.
 */
#ifndef AAMP_AVLOG_H
#define AAMP_AVLOG_H

#include <dlfcn.h>
#include <stdint.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

/* arm a capture window only for a real jump, not for float noise */
#define AAMP_AVLOG_ARM_US        1000

/* values from <linux/avlog.h>: enum avlog_media, AVLOG_UF_ARM_TIMER */
#define AAMP_AVLOG_MEDIA_VIDEO   1u
#define AAMP_AVLOG_MEDIA_AUDIO   2u
#define AAMP_AVLOG_MEDIA_SUB     3u
#define AAMP_AVLOG_UF_ARM_TIMER  (1u << 0)

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
    const long long offUs = AampAvlogUs(ptsOffsetSec);
    if (trackType < 0 || trackType > 2 || offUs == last[trackType])
        return;
    const long long prevUs = last[trackType];
    last[trackType] = offUs;
    AampAvlogHal *hal = AampAvlogGet();
    if (!hal)
        return;

    const uint32_t media = AampAvlogMedia(trackType);
    uint32_t flags = 0;
    if (prevUs != 0 && (media == AAMP_AVLOG_MEDIA_VIDEO || media == AAMP_AVLOG_MEDIA_AUDIO))
        flags |= AAMP_AVLOG_UF_ARM_TIMER;      /* offset changed while playing */
    AampAvlogCheck(hal->restamp(media, flags, offUs, prevUs, AampAvlogUs(absPosition), timeScale,
                                static_cast<uint32_t>(discontinuityIndex), adPlaying ? 1u : 0u),
                   "AAMP_RESTAMP");
}

#endif /* AAMP_AVLOG_H */
