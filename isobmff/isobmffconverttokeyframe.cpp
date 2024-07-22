/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 Synamedia Ltd.
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

/* AAMP config header file is needed for the log level configuration.
 * It also includes AAMP log manager header file. */
#include "AampConfig.h"
#include "isobmffbuffer.h"
#include "isobmffconverttokeyframe.h"


bool IsoBmffConvertToKeyFrame(AampGrowableBuffer &buffer)
{
	AAMPLOG_TRACE("Function called with len = %lu", buffer.GetLen());

	bool retval{true};
	IsoBmffBuffer isoBmffBuffer{};

	isoBmffBuffer.setBuffer(reinterpret_cast<uint8_t*>(buffer.GetPtr()), buffer.GetLen() );

	if(isoBmffBuffer.parseBuffer())
	{
		isoBmffBuffer.truncate();
		buffer.ReduceLen(isoBmffBuffer.getSize());
	}
	else
	{
		retval = false;
	}

	return retval;
}
