/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#include "AampBufferControl.h"
#include "AampConfig.h"
#include "StreamAbstractionAAMP.h"

AampBufferControl::BufferControlExternalData::BufferControlExternalData(const AAMPGstPlayer* player,
																		const AampMediaType mediaType)
	: mRate(0), mTimeBasedBufferSeconds(0), mExtraDataCache(), mCacheValid(false)
{
	if (player && player->aamp && player->aamp->mConfig)
	{
		uint64_t x1_bufferSize =
			player->aamp->mConfig->GetConfigValue(eAAMPConfig_TimeBasedBufferSeconds);
		mRate = player->aamp->rate;
		float absRate = std::abs(mRate);
		if (absRate > 1)
		{
			// Increase buffer size during faster playback
			mTimeBasedBufferSeconds = x1_bufferSize * absRate;
		}
		else
		{
			mTimeBasedBufferSeconds = x1_bufferSize;
		}
	}
}

void AampBufferControl::BufferControlExternalData::actionDownloads(const AAMPGstPlayer* player, const AampMediaType mediaType, bool downloadsEnabled)
{
	if(player && player->aamp)
	{
		if(downloadsEnabled)
		{
			player->aamp->ResumeTrackDownloads(mediaType);
		}
		else
		{
			player->aamp->StopTrackDownloads(mediaType);
		}
	}
}

void AampBufferControl::BufferControlExternalData::cacheExtraData(const AAMPGstPlayer* player,
																  const AampMediaType mediaType)
{
	player->GetBufferControlData(mediaType, mExtraDataCache);
	mCacheValid = true;
}

BufferControlData AampBufferControl::BufferControlExternalData::getExtraDataCache() const
{
	if (!mCacheValid)
	{
		AAMPLOG_ERR("BufferControlExternalData invalid data.");
	}
	return mExtraDataCache;
}

AampBufferControl::BufferControlByteBased::BufferControlByteBased(BufferControlMaster& context):
BufferControlStrategyBase(context)
{
	AAMPLOG_WARN("BufferControlByteBased %s strategy activated", getThisMediaTypeName());
}


const char* AampBufferControl::BufferControlStrategyBase::getThisMediaTypeName() const
{
	return mContext.getThisMediaTypeName();
}

AampBufferControl::BufferControlByteBased::~BufferControlByteBased()
{
	AAMPLOG_TRACE("BufferControlByteBased strategy deactivated");
}


void AampBufferControl::BufferControlByteBased::enoughData()
{
	if(mState != eBUFFER_NEEDS_DATA_SIGNAL)
	{
		AAMPLOG_TRACE("BufferControlStrategyBase ENOUGH_DATA, %s, %s->eBUFFER_NEEDS_DATA_SIGNAL.", getThisMediaTypeName(), getStateName());
		mState = eBUFFER_NEEDS_DATA_SIGNAL;
		mContext.StopDownloads();
	}
}

void AampBufferControl::BufferControlByteBased::needData()
{
	if(mState == eBUFFER_NEEDS_DATA_SIGNAL)
	{
		AAMPLOG_WARN("%s eBUFFER_NEEDS_DATA_SIGNAL->eBUFFER_FILLING.", getThisMediaTypeName());
		mState = eBUFFER_FILLING;
		mContext.ResumeDownloads();
	}
}

void AampBufferControl::BufferControlTimeBased::updateInternal(const BufferControlExternalData& externalData)
{
	auto extraData = externalData.getExtraDataCache();
    if(extraData.StreamReady)
	{
		const auto originalState = mState;  //cache for later state change check
		const AampTime elapsedSecondsUnlimited = extraData.ElapsedSeconds;
		AampTime injectedSeconds=getInjectedSeconds();
		const AampTime elapsedSeconds = std::min(injectedSeconds, elapsedSecondsUnlimited);
		auto mediaType = mContext.getMediaType();
		if(((elapsedSeconds+1)<elapsedSecondsUnlimited) && ((mediaType==eMEDIATYPE_VIDEO)||(std::abs(externalData.getRate()))))
		{
			std::string msg = "BufferControlTimeBased ";
			msg+=getThisMediaTypeName();
			msg+=" limiting elapsedSeconds (";
			msg+=std::to_string(elapsedSecondsUnlimited.seconds());
			msg+=") to secondsInjected (";
			msg+=std::to_string(injectedSeconds.seconds());
			msg+=")";

			/* temporarily limiting all messages of this type to trace
			if((elapsedSeconds + mLastInjectedDuration + 1)<elapsedSecondsUnlimited)
			{
				AAMPLOG_WARN("%s", msg.c_str());
			}
			else*/
			{
				AAMPLOG_TRACE("%s", msg.c_str());
			}
		}

		const AampTime bufferedSeconds {injectedSeconds - elapsedSeconds};
		const float BufferTargetDurationSeconds = externalData.getTimeBasedBufferSeconds();

		switch(mState)
		{
			case eBUFFER_FULL:
				if(extraData.GstWaitingForData)
				{
					AAMPLOG_WARN("BufferControlTimeBased %s %s GStreamer state change pending.",
					getThisMediaTypeName(), getStateName());
					mState = eBUFFER_FILLING;
				}
				else if(0.5<(BufferTargetDurationSeconds-bufferedSeconds))
				{
					mState = eBUFFER_FILLING;
				}
				break;

			case eBUFFER_FILLING:
				if(bufferedSeconds >= BufferTargetDurationSeconds)
				{
					if(extraData.GstWaitingForData)
					{
						AAMPLOG_WARN("BufferControlTimeBased %s waiting for GStreamer state change before changing to eBUFFER_FULL.", getThisMediaTypeName());
					}
					else
					{
						mState = eBUFFER_FULL;
					}
				}
				break;

			case eBUFFER_NEEDS_DATA_SIGNAL:
				//no action required
				break;

			default:
				AAMPLOG_ERR("BufferControlTimeBased unknown state");
				break;
		}

		if(originalState != mState)
		{
			AAMPLOG_WARN("BufferControlTimeBased %s %s->%s %.2fs buffered. (buffer target = %.2fs, %.2fs injected, %.2fs elapsed).",
						getThisMediaTypeName(),
						getStateName(originalState),
						getStateName(),
						bufferedSeconds.inSeconds(),
						BufferTargetDurationSeconds,
						injectedSeconds.inSeconds(),
						elapsedSeconds.inSeconds()
						);
		}
	}

	switch(mState)
	{
		case eBUFFER_FILLING:
			mContext.ResumeDownloads();
		break;

		case eBUFFER_NEEDS_DATA_SIGNAL:
		case eBUFFER_FULL:
		default:
			mContext.StopDownloads();
		break;
	}
}

void AampBufferControl::BufferControlTimeBased::RestartInjectedSecondsCount()
{
	AAMPLOG_WARN("BufferControlTimeBased %s Injected Seconds Count restarted.", getThisMediaTypeName());
	mInjectedStartSet=false;
	mInjectedStart=0;
	mInjectedEnd=0;
}

AampBufferControl::BufferControlTimeBased::BufferControlTimeBased(BufferControlMaster& context):
BufferControlByteBased(context),
mInjectedStart(0),
mInjectedStartSet(false),
mInjectedEnd(0),
mLastInjectedDuration(0)
{
	AAMPLOG_WARN("BufferControlTimeBased %s strategy activated", getThisMediaTypeName());
}

AampBufferControl::BufferControlTimeBased::~BufferControlTimeBased()
{
	AAMPLOG_TRACE("BufferControlTimeBased strategy deactivated");
}

void AampBufferControl::BufferControlTimeBased::enoughData()
{
	BufferControlByteBased::enoughData();
	RestartInjectedSecondsCount();
}

void AampBufferControl::BufferControlTimeBased::underflow()
{
	AAMPLOG_WARN("BufferControlTimeBased underflow, %s, %s -> eBUFFER_FILLING.",
	getThisMediaTypeName(), getStateName());
	mState = eBUFFER_FILLING;
	RestartInjectedSecondsCount();
	mContext.ResumeDownloads();
}

void AampBufferControl::BufferControlTimeBased::update(const BufferControlExternalData& externalData)
{
	updateInternal(externalData);
}

void AampBufferControl::BufferControlTimeBased::notifyFragmentInject(const BufferControlExternalData& externalData, const double fpts, const double fdts, const double duration, const bool firstBuffer)
{
	/**
	* Deliberately not adding duration here so that injected duration is underestimated.
	* Underestimation has only a trivial impact (1 fragment is downloaded earlier).
	* Overestimation could interrupt playback and must be avoided.
	* In some edge cases (e.g. HLS, near period end) the value of the duration argument is too large.
	**/
	mInjectedEnd = fdts;

	if(firstBuffer|(!mInjectedStartSet))
	{
		if(!firstBuffer)
		{
			//Could be due to a preceding call to RestartInjectedSecondsCount().
			AAMPLOG_WARN("BufferControlTimeBased %s, %s firstBuffer is not being used as buffer start reference.",
			getThisMediaTypeName(),
			getStateName());
		}

		mInjectedStart = fdts;
		//mInjectedEnd set above
		mInjectedStartSet = true;
	}

	AAMPLOG_TRACE("BufferControlTimeBased %s, %s injected%s %.2fs= %.2fs - %.2fs, (fpts %.2fs, fdts %.2fs, duration %.2fs)",
	getThisMediaTypeName(),
	getStateName(),
	firstBuffer?" first buffer":"",
	getInjectedSeconds().inSeconds(),
	mInjectedEnd.inSeconds(),
	mInjectedStart.inSeconds(),
	fpts,
	fdts,
	duration);

	update(externalData);
}

AampBufferControl::BufferControlMaster::BufferControlMaster():
	mpBufferingStrategy(nullptr),
	mTeardownInProgress(false),
	mMutex(),
	mMediaType(eMEDIATYPE_DEFAULT),
	mDownloadShouldBeEnabled(true)
	{
		//empty
	}

void AampBufferControl::BufferControlMaster::createOrChangeStrategyIfRequired(BufferControlExternalData& externalData)
{
	if(externalData.ShouldBeTimeBased())
	{
		if(dynamic_cast<BufferControlTimeBased*>(mpBufferingStrategy.get())==nullptr)
		{
			mpBufferingStrategy.reset(new BufferControlTimeBased{*this});
		}
	}
	else if(dynamic_cast<BufferControlByteBased*>(mpBufferingStrategy.get())==nullptr)
	{
		mpBufferingStrategy.reset(new BufferControlByteBased{*this});
	}
}

bool  AampBufferControl::BufferControlMaster::isBufferFull(const AampMediaType mediaType)
{
	bool isBuffFull = !mDownloadShouldBeEnabled.load();
	AAMPLOG_DEBUG("BufferControlMaster %s Buffer full status : %d", getThisMediaTypeName(), isBuffFull);
	return isBuffFull;
}

void AampBufferControl::BufferControlMaster::needData(const AAMPGstPlayer *player, const AampMediaType mediaType)
{
	try
	{
		mMediaType=mediaType;

		std::lock_guard<std::mutex> lock(mMutex);
		if(mTeardownInProgress)
		{
			/*
			 * During teardown directly respond to needdata messages
			 * this prevents elements being starved of data during teardown
			 * handling this through mpBufferingStrategy was considered
			 * but handling this directly is simpler & should be more robust to change
			 * consider direct EOS injection in place of this*/
			bool downloadsJustEnabled = !mDownloadShouldBeEnabled.exchange(true);
			if(downloadsJustEnabled)
			{
				AAMPLOG_WARN("BufferControlMaster %s starting downloads during teardown.", getThisMediaTypeName());
			}
		}
		else
		{
			BufferControlExternalData externalData{player, mediaType};
			createOrChangeStrategyIfRequired(externalData);
			if(mpBufferingStrategy)
			{
				mpBufferingStrategy->needData();
			}
		}
	}
	catch(const std::exception& e)
	{
		//unexpected
		AAMPLOG_ERR("caught %s", e.what());
	}

	BufferControlExternalData::actionDownloads(player, mediaType, mDownloadShouldBeEnabled);
}

void AampBufferControl::BufferControlMaster::enoughData(const AAMPGstPlayer *player, const AampMediaType mediaType)
{
	try
	{
		mMediaType=mediaType;

		std::lock_guard<std::mutex> lock(mMutex);
		if(mTeardownInProgress)
		{
			/*
			 * During teardown directly respond to needdata messages
			 * also see comments in AampBufferControl::BufferControlMaster::needData()*/
			bool downloadsJustDisabled = mDownloadShouldBeEnabled.exchange(false);
			if(downloadsJustDisabled)
			{
				AAMPLOG_WARN("BufferControlMaster %s disabling downloads during teardown.", getThisMediaTypeName());
			}
		}
		else
		{
			BufferControlExternalData externalData{player, mediaType};
			createOrChangeStrategyIfRequired(externalData);
			if(mpBufferingStrategy)
			{
				mpBufferingStrategy->enoughData();
			}
		}
	}
	catch(const std::exception& e)
	{
		//unexpected
		AAMPLOG_ERR("caught %s", e.what());
	}

	BufferControlExternalData::actionDownloads(player, mediaType, mDownloadShouldBeEnabled);
}

void AampBufferControl::BufferControlMaster::update(const AAMPGstPlayer *player, const AampMediaType mediaType)
{
	try
	{
		mMediaType=mediaType;
		BufferControlExternalData externalData{player, mediaType};
		if(externalData.ShouldBeTimeBased())
		{
			externalData.cacheExtraData(player, mediaType);
		}

		std::lock_guard<std::mutex> lock(mMutex);
		if(!mTeardownInProgress)
		{
			createOrChangeStrategyIfRequired(externalData);
			if(mpBufferingStrategy)
			{
				mpBufferingStrategy->update(externalData);
			}
		}
	}
	catch(const std::exception& e)
	{
		//unexpected
		AAMPLOG_ERR("caught %s", e.what());
	}

	BufferControlExternalData::actionDownloads(player, mediaType, mDownloadShouldBeEnabled);
}

void AampBufferControl::BufferControlMaster::notifyFragmentInject(const AAMPGstPlayer* player,  const AampMediaType mediaType, double fpts, double fdts, double duration, bool firstBuffer)
{
	try
	{
		mMediaType=mediaType;

		BufferControlExternalData externalData{player, mediaType};
		if(externalData.ShouldBeTimeBased())
		{
			externalData.cacheExtraData(player, mediaType);
		}

		std::lock_guard<std::mutex> lock(mMutex);
		if((!mTeardownInProgress) && mpBufferingStrategy)
		{
			mpBufferingStrategy->notifyFragmentInject(externalData, fpts, fdts, duration, firstBuffer);
		}
	}
	catch(const std::exception& e)
	{
		//unexpected
		AAMPLOG_ERR("caught %s", e.what());
	}

	BufferControlExternalData::actionDownloads(player, mediaType, mDownloadShouldBeEnabled);
}

void AampBufferControl::BufferControlMaster::underflow(const AAMPGstPlayer *player, const AampMediaType mediaType)
{
	try
	{
		mMediaType=mediaType;
		std::lock_guard<std::mutex> lock(mMutex);
		if((!mTeardownInProgress) && mpBufferingStrategy)
		{
			mpBufferingStrategy->underflow();
		}
	}
	catch(const std::exception& e)
	{
		//unexpected
		AAMPLOG_ERR("caught %s", e.what());
	}

	BufferControlExternalData::actionDownloads(player, mediaType, mDownloadShouldBeEnabled);
}

void AampBufferControl::BufferControlMaster::teardownStart()
{
	std::lock_guard<std::mutex> lock(mMutex);
	mTeardownInProgress = true;
}

void AampBufferControl::BufferControlMaster::teardownEnd()
{
	std::lock_guard<std::mutex> lock(mMutex);
	StopDownloads();
	mpBufferingStrategy.reset();
	mTeardownInProgress=false;
}

void AampBufferControl::BufferControlMaster::flush()
{
	std::lock_guard<std::mutex> lock(mMutex);
	mpBufferingStrategy.reset();
}
