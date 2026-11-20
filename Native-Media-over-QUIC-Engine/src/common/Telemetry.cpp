#include "Telemetry.h"

#include "Clock.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>

namespace nmqe {

void Json::key(const char *name)
{
	if (!mBody.empty()) mBody += ',';
	mBody += '"';
	mBody += name;
	mBody += "\":";
}

Json &Json::add(const char *name, int64_t value)
{
	key(name);
	mBody += std::to_string(value);
	return *this;
}

Json &Json::add(const char *name, uint64_t value)
{
	key(name);
	mBody += std::to_string(value);
	return *this;
}

Json &Json::add(const char *name, double value)
{
	key(name);
	if (!std::isfinite(value))
	{
		mBody += "null";
		return *this;
	}
	char text[32];
	snprintf(text, sizeof(text), "%.6g", value);
	mBody += text;
	return *this;
}

Json &Json::add(const char *name, bool value)
{
	key(name);
	mBody += value ? "true" : "false";
	return *this;
}

Json &Json::add(const char *name, const std::string &value)
{
	key(name);
	mBody += '"';
	mBody += escape(value);
	mBody += '"';
	return *this;
}

Json &Json::add(const char *name, const char *value)
{
	return add(name, std::string(value ? value : ""));
}

Json &Json::raw(const char *name, const std::string &json)
{
	key(name);
	mBody += json;
	return *this;
}

std::string Json::escape(const std::string &text)
{
	std::string out;
	out.reserve(text.size());
	for (unsigned char c : text)
	{
		switch (c)
		{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char hex[8];
					snprintf(hex, sizeof(hex), "\\u%04x", c);
					out += hex;
				}
				else
				{
					out += static_cast<char>(c);
				}
		}
	}
	return out;
}

Telemetry &Telemetry::instance()
{
	static Telemetry telemetry;
	return telemetry;
}

Telemetry::~Telemetry()
{
	close();
}

std::string Telemetry::makeRunId()
{
	std::random_device device;
	std::mt19937_64 generator(device() ^ static_cast<uint64_t>(monoNs()));
	char text[32];
	snprintf(text, sizeof(text), "%" PRId64 "-%04x", wallMs(), static_cast<unsigned>(generator() & 0xffff));
	return text;
}

bool Telemetry::open(const std::string &path, const std::string &runId, bool traceEnabled)
{
	close();
	std::lock_guard<std::mutex> lock(mMutex);
	mRunId = runId;
	mTrace = traceEnabled;
	mToStdout = path.empty() || path == "-";
	if (!mToStdout)
	{
		mFile.open(path, std::ios::out | std::ios::app);
		if (!mFile) return false;
	}
	mStopping = false;
	mOpen = true;
	mWriter = std::thread(&Telemetry::writerLoop, this);
	return true;
}

void Telemetry::close()
{
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (!mOpen) return;
		mStopping = true;
	}
	mCv.notify_all();
	if (mWriter.joinable()) mWriter.join();
	std::lock_guard<std::mutex> lock(mMutex);
	if (mFile.is_open()) mFile.close();
	mOpen = false;
}

void Telemetry::emit(const char *category, const char *event, const Json &fields)
{
	std::string line;
	line.reserve(160 + fields.body().size());
	line += "{\"schema\":";
	line += std::to_string(kSchemaVersion);
	line += ",\"run\":\"";
	line += mRunId;
	line += "\",\"mono_ns\":";
	line += std::to_string(monoNs());
	line += ",\"wall_ms\":";
	line += std::to_string(wallMs());
	line += ",\"cat\":\"";
	line += category;
	line += "\",\"ev\":\"";
	line += event;
	line += '"';
	if (!fields.body().empty())
	{
		line += ',';
		line += fields.body();
	}
	line += "}\n";

	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (!mOpen) return;
		if (mQueue.size() >= kMaxQueuedLines)
		{
			++mDropped;
			return;
		}
		mQueue.push_back(std::move(line));
	}
	mCv.notify_one();
}

void Telemetry::writerLoop()
{
	std::unique_lock<std::mutex> lock(mMutex);
	for (;;)
	{
		mCv.wait(lock, [this] { return mStopping || !mQueue.empty(); });
		std::deque<std::string> batch;
		batch.swap(mQueue);
		const bool stopping = mStopping;
		lock.unlock();
		for (const auto &line : batch)
		{
			if (mToStdout) std::cout << line;
			else mFile << line;
		}
		if (mToStdout) std::cout.flush();
		else mFile.flush();
		lock.lock();
		if (stopping && mQueue.empty()) return;
	}
}

} // namespace nmqe
