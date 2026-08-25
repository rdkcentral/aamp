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

#include "CachedFragment.h"

CachedFragment::CachedFragment()
	: fragment()
	, position(0.0)
	, duration(0.0)
	, initFragment(false)
	, discontinuity(false)
	, profileIndex(0)
	, cacheFragStreamInfo(StreamInfo())
	, type(eMEDIATYPE_DEFAULT)
	, downloadStartTime(0)
	, timeScale(0)
	, PTSOffsetSec(0)
	, absPosition(0.0)
	, discontinuityIndex(0)
{
}

void CachedFragment::Copy(const CachedFragment& other)
{
}

void CachedFragment::Clear()
{
}
