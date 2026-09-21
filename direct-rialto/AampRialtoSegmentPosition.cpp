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
 * @file AampRialtoSegmentPosition.cpp
 * @brief Implementation of the staged-position / segment-baseline holder.
 */

#include "AampRialtoSegmentPosition.h"

void AampRialtoSegmentPosition::StageRequested(
	int64_t positionNs, int rate, bool authoritative)
{
	m_stagedPositionNs.store(positionNs, std::memory_order_relaxed);
	m_stagedRate.store(rate, std::memory_order_relaxed);
	m_authoritative.store(authoritative, std::memory_order_relaxed);
}

int64_t AampRialtoSegmentPosition::StagedPositionNs() const
{
	return m_stagedPositionNs.load(std::memory_order_relaxed);
}

int AampRialtoSegmentPosition::StagedRate() const
{
	return m_stagedRate.load(std::memory_order_relaxed);
}

bool AampRialtoSegmentPosition::ConsumeAuthoritative()
{
	return m_authoritative.exchange(false, std::memory_order_relaxed);
}

void AampRialtoSegmentPosition::ResetFlushClaim()
{
	m_flushClaimed.store(false, std::memory_order_relaxed);
}

bool AampRialtoSegmentPosition::ClaimDeferredFlush()
{
	return !m_flushClaimed.exchange(true, std::memory_order_acq_rel);
}

void AampRialtoSegmentPosition::CommitBaseline(int64_t positionNs)
{
	m_baselineNs.store(positionNs, std::memory_order_relaxed);
}

int64_t AampRialtoSegmentPosition::BaselineNs() const
{
	return m_baselineNs.load(std::memory_order_relaxed);
}

void AampRialtoSegmentPosition::ClearForNewSession()
{
	m_stagedPositionNs.store(kNoStagedPosition, std::memory_order_relaxed);
	m_baselineNs.store(0, std::memory_order_relaxed);
}
