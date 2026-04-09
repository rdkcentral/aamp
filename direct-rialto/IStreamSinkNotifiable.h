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

/**
 * @file IStreamSinkNotifiable.h
 * @brief Narrow callback interface used by StreamSink implementations to
 *        notify the player core of playback lifecycle events.
 *
 * Only the push-notification methods are included here — methods that the
 * sink calls on the player when it observes a new state.  Query methods and
 * download-control methods that the sink pulls from the player remain on
 * PrivateInstanceAAMP directly.
 *
 * This interface exists so that direct-rialto/ code under test can be
 * exercised with a lightweight mock rather than a reinterpret_cast of
 * MockPrivateInstanceAAMP.
 */

#ifndef ISTREAM_SINK_NOTIFIABLE_H
#define ISTREAM_SINK_NOTIFIABLE_H

#include "AampEvent.h"   ///< AAMPPlayerState enum

#include <string>

/**
 * @interface IStreamSinkNotifiable
 * @brief Push-notification contract between a StreamSink and the AAMP core.
 *
 * Implementations forward each call to PrivateInstanceAAMP.  In tests a
 * lightweight mock is used instead so assertions can be made without
 * instantiating the full AAMP player.
 */
class IStreamSinkNotifiable
{
public:
	virtual ~IStreamSinkNotifiable() = default;

	// -----------------------------------------------------------------------
	// First-frame / tune-complete notifications
	// -----------------------------------------------------------------------

	/**
	 * @brief Notify that the first video frame has been received from the
	 *        sink.  Drives AAMP state to eSTATE_PLAYING on initial tune,
	 *        signals waitforplaystart, fires the tuned event, and
	 *        initialises the CC renderer.
	 *
	 * @param[in] ccDecoderHandle  Platform CC-decoder handle (0 when not
	 *                             applicable, e.g. direct-Rialto path).
	 */
	virtual void NotifyFirstFrameReceived(unsigned long ccDecoderHandle) = 0;

	/**
	 * @brief Notify that the first buffer has been pushed into the sink.
	 *        Drives AAMP state to eSTATE_PLAYING after a seek
	 *        (eSTATE_SEEKING → eSTATE_PLAYING), updates trickStartUTCMS,
	 *        and reports playback position to the DRM manager.
	 *
	 * @param[in] videoRectangle  Current video rectangle as "x,y,w,h" string.
	 *                            Pass an empty string when the rectangle is
	 *                            unknown.
	 */
	virtual void NotifyFirstBufferProcessed(const std::string &videoRectangle) = 0;

	/**
	 * @brief Record the first-frame profiler timestamp.
	 *        Must be called alongside NotifyFirstBufferProcessed on the
	 *        initial tune.
	 */
	virtual void LogFirstFrame() = 0;

	/**
	 * @brief Record tune-completion metrics and log the tune-end event.
	 *        Must be called once per successful initial tune.
	 */
	virtual void LogTuneComplete() = 0;

	// -----------------------------------------------------------------------
	// End-of-stream
	// -----------------------------------------------------------------------

	/**
	 * @brief Notify that end-of-stream has been reached.
	 *        Drives AAMP state to eSTATE_COMPLETE for VOD and fires
	 *        AAMP_EVENT_EOS.
	 */
	virtual void NotifyEOSReached() = 0;

	// -----------------------------------------------------------------------
	// Progress monitoring
	// -----------------------------------------------------------------------

	/**
	 * @brief Drive AAMP to report a progress event.
	 *        Should be called each time the Rialto server delivers a
	 *        position update.
	 *
	 * @param[in] sync              Reserved; pass false unless a synchronous
	 *                              progress report is required.
	 * @param[in] beginningOfStream True when rewind has reached BoS.
	 */
	virtual void MonitorProgress(bool sync = false,
	                             bool beginningOfStream = false) = 0;

	// -----------------------------------------------------------------------
	// Speed / state
	// -----------------------------------------------------------------------

	/**
	 * @brief Notify that the playback speed has changed.
	 *        When @p changeState is true AAMP updates its internal state
	 *        (e.g. eSTATE_PAUSED when rate == 0, eSTATE_PLAYING otherwise).
	 *
	 * @param[in] rate         New playback rate.
	 * @param[in] changeState  Whether AAMP should update its player state.
	 */
	virtual void NotifySpeedChanged(float rate, bool changeState) = 0;

	/**
	 * @brief Return the current AAMP player state.
	 *
	 * Used by the sink to determine which notification variant is needed
	 * (e.g. seek-recovery vs. resume-from-pause).
	 */
	virtual AAMPPlayerState GetState() = 0;
};

#endif // ISTREAM_SINK_NOTIFIABLE_H
