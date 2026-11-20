#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace nmqe {

/** Builds the fields of one JSON object. Keys are trusted literals; string values are escaped. */
class Json
{
public:
	Json &add(const char *key, int64_t value);
	Json &add(const char *key, uint64_t value);
	Json &add(const char *key, int value) { return add(key, static_cast<int64_t>(value)); }
	Json &add(const char *key, unsigned value) { return add(key, static_cast<uint64_t>(value)); }
	Json &add(const char *key, double value);
	Json &add(const char *key, bool value);
	Json &add(const char *key, const std::string &value);
	Json &add(const char *key, const char *value);
	/** Adds an already-serialized JSON value (object, array, number). */
	Json &raw(const char *key, const std::string &json);
	const std::string &body() const { return mBody; }
	std::string object() const { return "{" + mBody + "}"; }

	static std::string escape(const std::string &text);

private:
	void key(const char *name);
	std::string mBody;
};

/**
 * JSON Lines telemetry. One object per line with schema version, run ID,
 * monotonic time, wallclock, category and event. Emission never blocks the
 * caller on I/O: lines are queued for a writer thread, and a bounded queue
 * drops (and counts) rather than grows.
 */
class Telemetry
{
public:
	static constexpr int kSchemaVersion = 1;

	static Telemetry &instance();

	/** Opens the output; an empty path or "-" writes to stdout. */
	bool open(const std::string &path, const std::string &runId, bool trace);
	void close();

	/** Lifecycle and summary events: always emitted. */
	void emit(const char *category, const char *event, const Json &fields = Json());
	/** Per-object/per-frame events: emitted only when tracing is enabled. */
	void trace(const char *category, const char *event, const Json &fields = Json())
	{
		if (mTrace) emit(category, event, fields);
	}

	bool tracing() const { return mTrace; }
	const std::string &runId() const { return mRunId; }
	uint64_t dropped() const { return mDropped; }

	static std::string makeRunId();

private:
	Telemetry() = default;
	~Telemetry();
	void writerLoop();

	static constexpr size_t kMaxQueuedLines = 100000;

	std::mutex mMutex;
	std::condition_variable mCv;
	std::deque<std::string> mQueue;
	std::thread mWriter;
	std::ofstream mFile;
	bool mToStdout = false;
	bool mOpen = false;
	bool mStopping = false;
	bool mTrace = false;
	uint64_t mDropped = 0;
	std::string mRunId;
};

} // namespace nmqe
