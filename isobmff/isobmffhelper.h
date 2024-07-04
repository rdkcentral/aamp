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

#ifndef __ISOBMFFHELPER_H__
#define __ISOBMFFHELPER_H__

#include <cstdlib>
#include <string>
#include "AampGrowableBuffer.h"
/**
 * @brief Convert an ISOBMFF segment to a single key frame
 * @param[in,out] buffer ISOBMFF segment, contains a single key frame when the function returns
 * @retval false in case of failure, true otherwise
 */
bool IsoBmffConvertToKeyFrame(AampGrowableBuffer &buffer);

/**
 * @fn IsoBmffRestampPts
 *
 * @brief Restamp the PTS in the ISO BMFF boxes in the buffer, by adding an offset
 *
 * @param[in] buffer - Pointer to the AampGrowableBuffer
 * @param[in] ptsOffset - Offset to be added to PTS values
 * @param[in] fragmentUrl - Fragment URL, used in logging
 *
 * @retval true  - PTS values were restamped
 * @retval false - There was a problem restamping PTS values
 */
bool IsoBmffRestampPts(AampGrowableBuffer &buffer, int64_t ptsOffset, std::string const &fragmentUrl);

#endif /* __ISOBMFFHELPER_H__ */
