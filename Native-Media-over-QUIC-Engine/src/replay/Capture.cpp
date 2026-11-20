#include "Capture.h"

#include "common/Clock.h"
#include "common/Log.h"

#include <cstring>

namespace nmqe {

namespace {

constexpr char kMagic[] = "NMQECAP1\n";
constexpr size_t kMagicSize = sizeof(kMagic) - 1;
constexpr uint8_t kTrackRecord = 1;
constexpr uint8_t kObjectRecord = 2;
constexpr uint8_t kEventRecord = 3;

template <typename T>
void put(std::ofstream &out, T value)
{
	out.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

void putString(std::ofstream &out, const std::string &text)
{
	const uint16_t size = static_cast<uint16_t>(std::min<size_t>(text.size(), UINT16_MAX));
	put(out, size);
	out.write(text.data(), size);
}

class Reader
{
public:
	explicit Reader(std::ifstream &in) : mIn(in) {}
	template <typename T>
	bool get(T &value) { return static_cast<bool>(mIn.read(reinterpret_cast<char *>(&value), sizeof(value))); }
	bool getString(std::string &text)
	{
		uint16_t size = 0;
		if (!get(size)) return false;
		text.resize(size);
		return size == 0 || static_cast<bool>(mIn.read(text.data(), size));
	}
	bool getBytes(ByteBuffer &bytes, uint32_t size)
	{
		bytes.resize(size);
		return size == 0 || static_cast<bool>(mIn.read(reinterpret_cast<char *>(bytes.data()), size));
	}

private:
	std::ifstream &mIn;
};

} // namespace

bool CaptureWriter::open(const std::string &path)
{
	std::lock_guard<std::mutex> lock(mMutex);
	mFile.open(path, std::ios::binary | std::ios::trunc);
	if (!mFile) return false;
	mFile.write(kMagic, kMagicSize);
	return true;
}

void CaptureWriter::close()
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (mFile.is_open()) mFile.close();
}

void CaptureWriter::writeTrack(uint64_t subId, const std::string &track)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (!mFile.is_open()) return;
	put(mFile, kTrackRecord);
	put(mFile, subId);
	putString(mFile, track);
}

void CaptureWriter::writeObject(uint64_t subId, const ReceivedObject &object)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (!mFile.is_open() || !object.payload) return;
	put(mFile, kObjectRecord);
	put(mFile, subId);
	put(mFile, object.id.group);
	put(mFile, object.id.object);
	put(mFile, object.receiveMonoNs);
	put(mFile, object.receiveWallMs);
	put(mFile, static_cast<uint32_t>(object.payload->size()));
	mFile.write(reinterpret_cast<const char *>(object.payload->data()), static_cast<std::streamsize>(object.payload->size()));
}

void CaptureWriter::writeEvent(const SourceEvent &event)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (!mFile.is_open()) return;
	put(mFile, kEventRecord);
	put(mFile, static_cast<uint8_t>(event.kind));
	put(mFile, event.subId);
	put(mFile, event.group);
	put(mFile, event.objectCount);
	put(mFile, event.monoNs);
	put(mFile, event.wallMs);
	putString(mFile, event.detail);
}

RecordingObjectSource::RecordingObjectSource(std::unique_ptr<IObjectSource> inner, std::shared_ptr<CaptureWriter> writer)
	: mInner(std::move(inner)), mWriter(std::move(writer))
{
}

bool RecordingObjectSource::start(IObjectSourceListener *listener)
{
	mListener = listener;
	return mInner->start(this);
}

bool RecordingObjectSource::subscribe(const TrackSubscription &subscription)
{
	mWriter->writeTrack(subscription.subId, subscription.track);
	return mInner->subscribe(subscription);
}

void RecordingObjectSource::onSourceEvent(const SourceEvent &event)
{
	if (event.kind != SourceEvent::Kind::Closed) mWriter->writeEvent(event);
	mListener->onSourceEvent(event);
}

void RecordingObjectSource::onObject(uint64_t subId, ReceivedObject object)
{
	mWriter->writeObject(subId, object);
	mListener->onObject(subId, std::move(object));
}

ReplayObjectSource::ReplayObjectSource(std::string path, bool realtime) : mPath(std::move(path)), mRealtime(realtime) {}

ReplayObjectSource::~ReplayObjectSource()
{
	stop();
}

std::string ReplayObjectSource::describe() const
{
	return "replay " + mPath + (mRealtime ? " (realtime)" : " (fast)");
}

bool ReplayObjectSource::load(std::string &error)
{
	std::ifstream in(mPath, std::ios::binary);
	if (!in)
	{
		error = "cannot open " + mPath;
		return false;
	}
	char magic[kMagicSize];
	if (!in.read(magic, kMagicSize) || memcmp(magic, kMagic, kMagicSize) != 0)
	{
		error = mPath + " is not a capture file";
		return false;
	}
	Reader reader(in);
	uint8_t type = 0;
	while (reader.get(type))
	{
		Record record;
		record.type = type;
		int64_t wall = 0;
		bool ok = false;
		if (type == kTrackRecord)
		{
			ok = reader.get(record.subId) && reader.getString(record.text);
			if (ok) mCapturedTracks[record.subId] = record.text;
			continue;
		}
		if (type == kObjectRecord)
		{
			uint32_t size = 0;
			auto payload = std::make_shared<ByteBuffer>();
			ok = reader.get(record.subId) && reader.get(record.group) && reader.get(record.object) &&
			     reader.get(record.monoNs) && reader.get(wall) && reader.get(size) && reader.getBytes(*payload, size);
			record.payload = std::move(payload);
		}
		else if (type == kEventRecord)
		{
			ok = reader.get(record.kind) && reader.get(record.subId) && reader.get(record.group) &&
			     reader.get(record.object) && reader.get(record.monoNs) && reader.get(wall) && reader.getString(record.text);
		}
		if (!ok)
		{
			LOG_WARN("replay", "truncated record in %s after %zu records", mPath.c_str(), mRecords.size());
			break;
		}
		mRecords.push_back(std::move(record));
	}
	return true;
}

bool ReplayObjectSource::start(IObjectSourceListener *listener)
{
	std::string error;
	if (!load(error))
	{
		LOG_ERROR("replay", "%s", error.c_str());
		return false;
	}
	mListener = listener;
	mStopping = false;
	mThread = std::thread(&ReplayObjectSource::run, this);
	return true;
}

bool ReplayObjectSource::subscribe(const TrackSubscription &subscription)
{
	std::lock_guard<std::mutex> ordering(mDeliverMutex);
	std::vector<const Record *> held;
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mSubscribed[subscription.track] = subscription.subId;
		held.swap(mHeld[subscription.track]);
	}
	SourceEvent event;
	event.kind = SourceEvent::Kind::Subscribed;
	event.subId = subscription.subId;
	event.detail = subscription.track;
	event.monoNs = monoNs();
	event.wallMs = wallMs();
	mListener->onSourceEvent(event);
	for (const Record *record : held) deliver(*record, subscription.track);
	return true;
}

bool ReplayObjectSource::unsubscribe(uint64_t subId)
{
	std::lock_guard<std::mutex> lock(mMutex);
	for (auto it = mSubscribed.begin(); it != mSubscribed.end(); ++it)
	{
		if (it->second == subId)
		{
			mSubscribed.erase(it);
			return true;
		}
	}
	return false;
}

void ReplayObjectSource::stop()
{
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mStopping = true;
	}
	mCv.notify_all();
	if (mThread.joinable()) mThread.join();
}

void ReplayObjectSource::deliver(const Record &record, const std::string &track)
{
	uint64_t subId = 0;
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mSubscribed.find(track);
		if (it == mSubscribed.end())
		{
			mHeld[track].push_back(&record);
			return;
		}
		subId = it->second;
	}
	if (record.type == kObjectRecord)
	{
		ReceivedObject object;
		object.id.track = track;
		object.id.group = record.group;
		object.id.object = record.object;
		object.payload = record.payload;
		object.receiveMonoNs = monoNs();
		object.receiveWallMs = wallMs();
		mListener->onObject(subId, std::move(object));
		return;
	}
	SourceEvent event;
	event.kind = static_cast<SourceEvent::Kind>(record.kind);
	event.subId = subId;
	event.group = record.group;
	event.objectCount = record.object;
	event.detail = record.text;
	event.monoNs = monoNs();
	event.wallMs = wallMs();
	mListener->onSourceEvent(event);
}

void ReplayObjectSource::run()
{
	const int64_t startMono = monoNs();
	const int64_t firstCaptured = mRecords.empty() ? 0 : mRecords.front().monoNs;
	for (const Record &record : mRecords)
	{
		if (mRealtime)
		{
			const int64_t due = startMono + (record.monoNs - firstCaptured);
			std::unique_lock<std::mutex> lock(mMutex);
			mCv.wait_for(lock, std::chrono::nanoseconds(std::max<int64_t>(0, due - monoNs())), [this] { return mStopping; });
		}
		{
			std::lock_guard<std::mutex> lock(mMutex);
			if (mStopping) return;
		}
		std::lock_guard<std::mutex> ordering(mDeliverMutex);
		const auto track = mCapturedTracks.find(record.subId);
		const bool sessionEvent = record.type == kEventRecord && record.subId == 0;
		if (sessionEvent)
		{
			SourceEvent event;
			event.kind = static_cast<SourceEvent::Kind>(record.kind);
			event.detail = record.text;
			event.monoNs = monoNs();
			event.wallMs = wallMs();
			// The replay keeps running past the captured session's end.
			if (event.kind != SourceEvent::Kind::Closed) mListener->onSourceEvent(event);
		}
		else if (track != mCapturedTracks.end() &&
		         !(record.type == kEventRecord && static_cast<SourceEvent::Kind>(record.kind) == SourceEvent::Kind::Subscribed))
		{
			deliver(record, track->second);
		}
	}
	SourceEvent end;
	end.kind = SourceEvent::Kind::Closed;
	end.detail = "end of capture";
	end.monoNs = monoNs();
	end.wallMs = wallMs();
	mListener->onSourceEvent(end);
}

} // namespace nmqe
