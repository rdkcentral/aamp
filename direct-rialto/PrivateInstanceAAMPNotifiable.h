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
 * @file PrivateInstanceAAMPNotifiable.h
 * @brief Thin adapter that satisfies IStreamSinkNotifiable by forwarding
 *        each call to the wrapped PrivateInstanceAAMP instance.
 *
 * This adapter is an implementation detail of the production path inside
 * AampRialtoPlayer and is not part of the public direct-rialto API.
 * Keeping the include of priv_aamp.h confined to the adapter's .cpp file
 * means AampRialtoPlayer.h does not need to include the heavyweight
 * PrivateInstanceAAMP header.
 */

#ifndef PRIVATE_INSTANCE_AAMP_NOTIFIABLE_H
#define PRIVATE_INSTANCE_AAMP_NOTIFIABLE_H

#include "IStreamSinkNotifiable.h"

class PrivateInstanceAAMP;

/**
 * @class PrivateInstanceAAMPNotifiable
 * @brief Adapts PrivateInstanceAAMP to the IStreamSinkNotifiable interface.
 *
 * Owns a non-owning pointer to PrivateInstanceAAMP.  The caller is
 * responsible for ensuring the AAMP instance outlives this adapter.
 */
class PrivateInstanceAAMPNotifiable final : public IStreamSinkNotifiable
{
public:
	/**
	 * @brief Construct the adapter.
	 * @param[in] aamp  Non-null pointer to the owning PrivateInstanceAAMP.
	 *                  Must outlive this object.
	 */
	explicit PrivateInstanceAAMPNotifiable(PrivateInstanceAAMP *aamp) noexcept;

	~PrivateInstanceAAMPNotifiable() override = default;

	// Non-copyable, non-movable (wraps a raw pointer).
	PrivateInstanceAAMPNotifiable(const PrivateInstanceAAMPNotifiable &) = delete;
	PrivateInstanceAAMPNotifiable &operator=(const PrivateInstanceAAMPNotifiable &) = delete;

	void NotifyFirstFrameReceived(unsigned long ccDecoderHandle) override;
	void NotifyFirstBufferProcessed(const std::string &videoRectangle) override;
	void LogFirstFrame() override;
	void LogTuneComplete() override;
	void NotifyEOSReached() override;
	void MonitorProgress(bool sync, bool beginningOfStream) override;
	void NotifySpeedChanged(float rate, bool changeState) override;
	AAMPPlayerState GetState() override;
	void NotifyBufferUnderflow(AampMediaType type) override;

private:
	PrivateInstanceAAMP *m_aamp{nullptr};
};

#endif // PRIVATE_INSTANCE_AAMP_NOTIFIABLE_H
