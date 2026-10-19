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

#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

#include "simnet/net_persona_fitter.h"

namespace aamptrace {

static inline double now_monotonic_s()
{
	using Clock = std::chrono::steady_clock;
	return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

struct Burst
{
	double duration = 0.0;
	std::size_t bytes = 0;
	double gapBefore = 0.0;
};

class NetTrace
{
public:
	static constexpr double kMinBurstDurS = 0.001;

	explicit NetTrace(uint64_t reqId, double gapThresholdS)
		: mReqId(reqId), mGapThresholdS(gapThresholdS)
	{
	}

	bool OnWrite(std::size_t numBytes, double nowS)
	{
		bool newBurst = false;
		if (!mInBurst)
		{
			OpenBurst(nowS, mLastEndTimeS > 0.0 ? std::max(0.0, nowS - mLastEndTimeS) : 0.0);
			newBurst = true;
		}
		else
		{
			double idle = mLastCbTimeS > 0.0 ? std::max(0.0, nowS - mLastCbTimeS) : 0.0;
			if (idle > mGapThresholdS)
			{
				CloseBurst(nowS);
				OpenBurst(nowS, idle);
				newBurst = true;
			}
		}
		if (!mBursts.empty())
		{
			mBursts.back().bytes += numBytes;
		}
		mLastCbTimeS = nowS;
		return newBurst;
	}

	void OnCompleteBytes()
	{
		if (mInBurst)
		{
			CloseBurst(mLastCbTimeS > 0.0 ? mLastCbTimeS : now_monotonic_s());
		}
	}

	void SetCurlTimings(double startXferS, bool connReused)
	{
		mStartXferS = startXferS;
		mConnReused = connReused ? 1 : 0;
	}

	void FlushPersona()
	{
		auto& fitter = NetPersonaFitter::GetInstance();
		fitter.AddRequest(mStartXferS, mConnReused, false);
		double bytesSum = 0.0;
		double bytesSumSq = 0.0;
		for (std::size_t index = 0; index < mBursts.size(); ++index)
		{
			const auto& burst = mBursts[index];
			fitter.AddBurst(mReqId, static_cast<int>(index), burst.duration,
				burst.bytes, burst.gapBefore, false);
			double bytes = static_cast<double>(burst.bytes);
			bytesSum += bytes;
			bytesSumSq += bytes * bytes;
		}
		if (!mBursts.empty())
		{
			fitter.AddRequestBurstSummary(mBursts.size(), bytesSum, bytesSumSq);
		}
	}

private:
	void OpenBurst(double startS, double gapBeforeS)
	{
		mInBurst = true;
		mBurstStartTimeS = startS;
		mBursts.push_back({0.0, 0, gapBeforeS});
	}

	void CloseBurst(double endS)
	{
		if (!mInBurst || mBursts.empty())
		{
			return;
		}
		double durationS = endS - mBurstStartTimeS;
		mBursts.back().duration = std::max(kMinBurstDurS, durationS);
		mLastEndTimeS = endS;
		mInBurst = false;
	}

	uint64_t mReqId;
	bool mInBurst = false;
	double mLastCbTimeS = 0.0;
	double mLastEndTimeS = 0.0;
	double mBurstStartTimeS = 0.0;
	double mGapThresholdS;
	std::vector<Burst> mBursts;
	int mConnReused = 0;
	double mStartXferS = 0.0;
};

} // namespace aamptrace
