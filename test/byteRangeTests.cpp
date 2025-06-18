#include <gtest/gtest.h>
#include "priv_aamp.h"
#include "fragmentcollector_hls.h"

// Add this derived class definition
class TestTrackState : public TrackState {
public:
    TestTrackState(TrackType type, StreamAbstractionAAMP_HLS* parent, PrivateInstanceAAMP* aamp, const char* name,
                   id3_callback_t id3Handler,
                   ptsoffset_update_t ptsUpdate)
        : TrackState(type, parent, aamp, name, id3Handler, ptsUpdate) {}

    void RunFetchLoop() override {
        while (aamp->DownloadsAreEnabled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

// Modify the test case to use the derived class
TEST(ByteRangeTests, DownloadsAreEnabled) {
    AampConfig config;
    PrivateInstanceAAMP aamp(&config);
    StreamAbstractionAAMP_HLS streamAbstraction(&aamp, 0, 1.0, nullptr, nullptr);
    TestTrackState trackState(eTRACK_VIDEO, &streamAbstraction, &aamp, "video", nullptr, nullptr);

    std::thread fetchThread([&]() {
        trackState.RunFetchLoop();
    });

    aamp.DisableDownloads();
    fetchThread.join();

    ASSERT_FALSE(aamp.DownloadsAreEnabled());
}
