#pragma once

#include "transport/ObjectSource.h"

#include <condition_variable>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nmqe {

/**
 * Capture file: "NMQE CAPTURE 1\n" followed by little-endian records
 *   u8 type = 1 track:  u64 subId, str track
 *   u8 type = 2 object: u64 subId, u64 group, u64 object, i64 monoNs, i64 wallMs, u32 size, bytes
 *   u8 type = 3 event:  u8 kind, u64 subId, u64 group, u64 count, i64 monoNs, i64 wallMs, str detail
 * where str is u16 length + UTF-8 bytes.
 */
class CaptureWriter
{
public:
	bool open(const std::string &path);
	void close();

	void writeTrack(uint64_t subId, const std::string &track);
	void writeObject(uint64_t subId, const ReceivedObject &object);
	void writeEvent(const SourceEvent &event);

private:
	std::mutex mMutex;
	std::ofstream mFile;
};

/**
 * Decorates any Object source and records what passes through it, so the
 * player has no dependency on capture.
 */
class RecordingObjectSource final : public ObjectSource, private ObjectSourceListener
{
public:
	RecordingObjectSource(std::unique_ptr<ObjectSource> inner, std::shared_ptr<CaptureWriter> writer);

	bool start(ObjectSourceListener *listener) override;
	bool subscribe(const TrackSubscription &subscription) override;
	bool unsubscribe(uint64_t subId) override { return mInner->unsubscribe(subId); }
	void stop() override { mInner->stop(); }
	std::string describe() const override { return mInner->describe() + " (recording)"; }

private:
	void onSourceEvent(const SourceEvent &event) override;
	void onObject(uint64_t subId, ReceivedObject object) override;

	std::unique_ptr<ObjectSource> mInner;
	std::shared_ptr<CaptureWriter> mWriter;
	ObjectSourceListener *mListener = nullptr;
};

/**
 * Replays a capture through the same ObjectSource seam as the live adapter.
 * Objects keep their captured relative timing (or run as fast as possible),
 * get fresh receive stamps, and are held per track until that track is
 * subscribed.
 */
class ReplayObjectSource final : public ObjectSource
{
public:
	ReplayObjectSource(std::string path, bool realTime);
	~ReplayObjectSource() override;

	bool start(ObjectSourceListener *listener) override;
	bool subscribe(const TrackSubscription &subscription) override;
	bool unsubscribe(uint64_t subId) override;
	void stop() override;
	std::string describe() const override;

private:
	struct Record
	{
		uint8_t type = 0;
		uint64_t subId = 0;
		uint64_t group = 0;
		uint64_t object = 0;
		uint8_t kind = 0;
		int64_t monoNs = 0;
		std::string text;
		std::shared_ptr<const ByteBuffer> payload;
	};

	bool load(std::string &error);
	void run();
	void deliver(const Record &record, const std::string &track);

	std::string mPath;
	bool mRealTime;
	ObjectSourceListener *mListener = nullptr;
	std::vector<Record> mRecords;
	std::map<uint64_t, std::string> mCapturedTracks;     // captured subId -> track

	std::mutex mMutex;
	std::mutex mDeliverMutex;                            // keeps held and live deliveries in capture order
	std::condition_variable mCv;
	std::map<std::string, uint64_t> mSubscribed;         // track -> live subId
	std::map<std::string, std::vector<const Record *>> mHeld;
	bool mStopping = false;
	std::thread mThread;
};

} // namespace nmqe
