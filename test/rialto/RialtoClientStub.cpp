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
 * @file RialtoClientStub.cpp
 * @brief Stub implementations of Rialto client library symbols.
 *
 * This file provides no-op / nullptr-returning implementations of the Rialto
 * factory singletons and non-inline MediaSegment copy methods so that AAMP can
 * link without the real libRialtoClient.so.
 *
 * These stubs are used when building AAMP without the "rialto" option
 * (i.e. when install-aamp.sh is run without "rialto"). When the real Rialto
 * library IS built, it provides these symbols and this stub is not linked.
 */

#include "IMediaPipeline.h"
#include "IClientLogControl.h"
#include "IControl.h"

namespace firebolt::rialto
{

// ---------------------------------------------------------------------------
// Factory stubs — return nullptr so direct-rialto code handles gracefully
// ---------------------------------------------------------------------------

std::shared_ptr<IMediaPipelineFactory> IMediaPipelineFactory::createFactory()
{
	return nullptr;
}

std::shared_ptr<IClientLogControlFactory> IClientLogControlFactory::createFactory()
{
	return nullptr;
}

std::shared_ptr<IControlFactory> IControlFactory::createFactory()
{
	return nullptr;
}

// ---------------------------------------------------------------------------
// MediaSegment::copy — non-inline member declared in IMediaPipeline.h
// ---------------------------------------------------------------------------

void IMediaPipeline::MediaSegment::copy(const MediaSegment &other)
{
	m_sourceId = other.m_sourceId;
	m_type = other.m_type;
	m_data = other.m_data;
	m_dataLength = other.m_dataLength;
	m_timeStamp = other.m_timeStamp;
	m_duration = other.m_duration;
	m_codecData = other.m_codecData;
	m_extraData = other.m_extraData;
	m_encrypted = other.m_encrypted;
	m_mediaKeySessionId = other.m_mediaKeySessionId;
	m_keyId = other.m_keyId;
	m_initVector = other.m_initVector;
	m_subSamples = other.m_subSamples;
	m_initWithLast15 = other.m_initWithLast15;
	m_alignment = other.m_alignment;
	m_cipherMode = other.m_cipherMode;
	m_crypt = other.m_crypt;
	m_skip = other.m_skip;
	m_encryptionPatternSet = other.m_encryptionPatternSet;
	m_displayOffset = other.m_displayOffset;
}

void IMediaPipeline::MediaSegmentAudio::copy(const MediaSegmentAudio &other)
{
	MediaSegment::copy(other);
	m_sampleRate = other.m_sampleRate;
	m_numberOfChannels = other.m_numberOfChannels;
	m_clippingStart = other.m_clippingStart;
	m_clippingEnd = other.m_clippingEnd;
}

void IMediaPipeline::MediaSegmentVideo::copy(const MediaSegmentVideo &other)
{
	MediaSegment::copy(other);
	m_width = other.m_width;
	m_height = other.m_height;
	m_frameRate = other.m_frameRate;
}

} // namespace firebolt::rialto
