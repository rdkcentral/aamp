/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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

#include "MockAampBufferControl.h"
#include "AampBufferControl.h"

MockAampBufferControl *g_mockAampBufferControl = nullptr;

AampBufferControl::BufferControlMaster::BufferControlMaster()
{
}

AampBufferControl::BufferControlExternalData::BufferControlExternalData(const AAMPGstPlayer *player,
																		const AampMediaType mediaType)
{
}

void AampBufferControl::BufferControlExternalData::cacheExtraData(const AAMPGstPlayer *player,
																  const AampMediaType mediaType)
{
}

bool AampBufferControl::BufferControlMaster::isBufferFull(const AampMediaType mediaType)
{
	return false;
}

void AampBufferControl::BufferControlMaster::needData(const AAMPGstPlayer *player,
													  AampMediaType mediaType)
{
}

void AampBufferControl::BufferControlMaster::enoughData(const AAMPGstPlayer *player,
														AampMediaType mediaType)
{
}

void AampBufferControl::BufferControlMaster::underflow(const AAMPGstPlayer *player,
													   const AampMediaType mediaType)
{
}

void AampBufferControl::BufferControlMaster::teardownStart()
{
}

void AampBufferControl::BufferControlMaster::teardownEnd()
{
}

void AampBufferControl::BufferControlMaster::notifyFragmentInject(const AAMPGstPlayer *player,
																  const AampMediaType mediaType,
																  double fpts, double fdts,
																  double duration, bool firstBuffer)
{
}

void AampBufferControl::BufferControlMaster::update(const AAMPGstPlayer *player, const AampMediaType mediaType)
{
}

void AampBufferControl::BufferControlMaster::flush()
{
}
