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

/**
 * @file AampBufferControl.h
 * @brief Provides Buffer control
 */

#ifndef __AAMP_BUFFER_CONTROL_H__
#define __AAMP_BUFFER_CONTROL_H__

#include "aampgstplayer.h"
#include "AampTime.h"
#include "AampUtils.h"

class AAMPGstPlayerPriv;
struct media_stream;

namespace AampBufferControl
{
	/* A standardized way of accessing data from other modules */
	class BufferControlExternalData
	{
	private:
		float mRate;
		float mTimeBasedBufferSeconds;
		BufferControlData mExtraDataCache;
		bool mCacheValid;


	public:
		BufferControlExternalData(const AAMPGstPlayer* player, const AampMediaType mediaType);

		float getRate() const {return mRate;}
		float getTimeBasedBufferSeconds() const {return mTimeBasedBufferSeconds;}
		bool ShouldBeTimeBased()const {return (mTimeBasedBufferSeconds>0);}


		/* Fetch data that is only required for BufferControlTimeBased::updateInternal
		** i.e. when ShouldBeTimeBased() == true*/
		void cacheExtraData(const AAMPGstPlayer* player, const AampMediaType mediaType);

		//returned value is only valid when cacheExtraData() has been called
		BufferControlData getExtraDataCache() const;

		static void	actionDownloads(const AAMPGstPlayer* player, const AampMediaType mediaType, const bool downloadsEnabled);
	};

	class BufferControlMaster;

	/**
	 * @brief
	 * Abstract base strategy implementing common functionality for implemented strategies
	 **/
	class BufferControlStrategyBase
	{
	public:
		typedef enum
		{                              // new state for time based buffering
			eBUFFER_NEEDS_DATA_SIGNAL, // waiting for needs_data
			eBUFFER_FILLING,           // building up buffers to USE_TIME_BASED_BUFFERING(s)
			eBUFFER_FULL               // waiting for buffering to fall under USE_TIME_BASED_BUFFERING(s)
		} BufferingState;

	protected:
		BufferControlMaster& mContext;
		BufferingState mState = eBUFFER_NEEDS_DATA_SIGNAL;

		const char* getThisMediaTypeName() const ;

		/**
		 * @brief Return a string representation of the supplied state
		 */
		static const char* getStateName(const BufferingState inputState)
		{
			switch(inputState)
			{
				case eBUFFER_NEEDS_DATA_SIGNAL:
				return "eBUFFER_NEEDS_DATA_SIGNAL";

				case eBUFFER_FILLING:
				return "eBUFFER_FILLING";

				case eBUFFER_FULL:
				return "eBUFFER_FULL";

				default:
				return "??";
			}
		}

	public:
		BufferControlStrategyBase(BufferControlMaster& context):
		mContext(context),
		mState(eBUFFER_NEEDS_DATA_SIGNAL)
		{/*empty*/};
		virtual ~BufferControlStrategyBase(){/*empty*/};

		/**
		 * @brief get a string representation of the current state
		 */
		const char* getStateName() const {return getStateName(mState);}

		/**
		 * @brief call on GStreamer need_data signal
		 */
		virtual void needData()=0;

		/**
		 * @brief call on GStreamer enough_data signal
		 */
		virtual void enoughData()=0;

		/**
		 * @brief call on GStreamer underflow signal
		 */
		virtual void underflow(){};

		/**
		 * @brief Generic update
		 */
		virtual void update( const BufferControlExternalData& externalData){/*empty*/};
		virtual void notifyFragmentInject( const BufferControlExternalData& externalData,  const double fpts,  const double fdts,  const double duration,  const bool firstBuffer)
		{
			update(externalData);
		}
	};

	/**
	 * @brief
	 * Byte based strategy using GStreamer need_Data & enough_data
	 * moves between eBUFFER_NEEDS_DATA_SIGNAL & eBUFFER_FILLING states
	 * encapsulated in this class
	 * this strategy was always used
	 **/
	class BufferControlByteBased : public BufferControlStrategyBase
	{
	public:
		BufferControlByteBased(BufferControlMaster& context);
		virtual ~BufferControlByteBased();

		void needData() override;
		void enoughData() override;
	};

	/**
	 * @brief
	 * Time based buffer strategy
	 * Builds on Byte based strategy using common handling of needData & enoughData
	 * Adds support for the  eBUFFER_FULL state
	 * eBUFFER_FULL can be reached from eBUFFER_FILLING when the buffer contains a sufficient duration of content
	 * eBUFFER_FILLING is resumed if the buffered content falls below a the target level
	 * Normally this limits the buffer size ~the target duration
	 * (i.e. x seconds after current playback position)
	 * Normally, only the first GStreamer need_Data signal is significant.
	 * enough_data acts as a backup e.g.position error or buffer too small.
	 **/
	class BufferControlTimeBased : public BufferControlByteBased
	{
		AampTime mInjectedEnd;
		AampTime mInjectedStart;
		bool mInjectedStartSet;
		AampTime mLastInjectedDuration;

		void updateInternal( const BufferControlExternalData& externalData);
		AampTime getInjectedSeconds() const
		{
			if(mInjectedStartSet)
			{
				//This deliberately does not include the duration of the last fragment
				//it's safer to underestimate the duration slightly (than to stop downloading prematurely)
				return abs(mInjectedEnd-mInjectedStart);
			}
			else
			{
				return 0;
			}
		}


		/**
		 * @brief
		 * Call when something unexpected occurs to prevent a potential injectedSeconds() overestimation
		 * Benefit: This is defensive, eBUFFER_FULL shouldn't be reached prematurely
		 * Drawback: injectedSeconds() is likely to underestimate & therefore ENOUGH_DATA is more likely to be reached
		 **/
		void RestartInjectedSecondsCount();

	public:
		BufferControlTimeBased(BufferControlMaster& context);
		virtual ~BufferControlTimeBased();

		//BufferControlInterface implementation
		void enoughData() override;
		void underflow()override;
		void update( const BufferControlExternalData& externalData) override;
		void notifyFragmentInject( const BufferControlExternalData& externalData,  const double fpts,  const double fdts,  const double duration,  const bool firstBuffer) override;
	};


	/**
	 * @brief
	 * Handle multiple buffering strategies
	 * Select strategy
	 * delegate to the current strategy
	 */
	class BufferControlMaster
	{

		std::unique_ptr<BufferControlStrategyBase> mpBufferingStrategy; // current buffering strategy
		std::atomic<bool> mTeardownInProgress;   // true during stream teardown, control access with mMutex
		std::mutex mMutex;          // Objects will be used by both periodic calls & gst callbacks

		std::atomic<AampMediaType> mMediaType;
		std::atomic<bool> mDownloadShouldBeEnabled;

		/**
		 * @brief create/replace strategies at runtime as needed
		 */
		void createOrChangeStrategyIfRequired(BufferControlExternalData& externalData);

	public:
		void ResumeDownloads(){mDownloadShouldBeEnabled=true;};
		void StopDownloads(){mDownloadShouldBeEnabled=false;};

		AampMediaType getMediaType() const {return mMediaType;}
		const char* getThisMediaTypeName() const {return GetMediaTypeName(getMediaType());}

		BufferControlMaster();

		/**
		 * @brief Indicate that teardown is in progress
		 * needData/enoughData/underflow should not be handled during teardown*/
		void teardownStart();

		/**
		 * @brief Indicate that teardown has completed
		 * reset internal state
		 * needData/enoughData/underflow can be handled*/
		void teardownEnd();


		/**
		 * @brief call on flush to reset internal states
		 */
		void flush();

		/**
		 * @brief Call from corresponding GStreamer events
		 * change state &/or strategy as required
		 * starts/stops downloads as required
		**/
		void needData(const AAMPGstPlayer* player, const AampMediaType mediaType);
		void enoughData(const AAMPGstPlayer* player, const AampMediaType mediaType);
		void underflow(const AAMPGstPlayer* player, const AampMediaType mediaType);
		/** 
		* @brief get the status of buffer enough data signal
		*/
		bool isBufferFull(const AampMediaType mediaType);

		/**
		 * @brief Generic update
		 * change state &/or strategy as required
		 * starts/stops downloads as required
		*/
		void update(const AAMPGstPlayer* player, const AampMediaType mediaType);

		void notifyFragmentInject(const AAMPGstPlayer* player,  const AampMediaType mediaType,  const double fpts,  const double fdts,  const double duration,  const bool firstBuffer);
	};
};
#endif
