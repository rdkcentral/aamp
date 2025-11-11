/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#ifndef RIALTO_COMMON_LOGGING_H_
#define RIALTO_COMMON_LOGGING_H_

#include "RialtoLogging.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RIALTO_COMMON_LOG_FATAL(fmt, args...) RIALTO_LOG_FATAL(RIALTO_COMPONENT_COMMON, fmt, ##args)
#define RIALTO_COMMON_LOG_SYS_FATAL(err, fmt, args...) RIALTO_LOG_SYS_FATAL(RIALTO_COMPONENT_COMMON, err, fmt, ##args)
#define RIALTO_COMMON_LOG_ERROR(fmt, args...) RIALTO_LOG_ERROR(RIALTO_COMPONENT_COMMON, fmt, ##args)
#define RIALTO_COMMON_LOG_SYS_ERROR(err, fmt, args...) RIALTO_LOG_SYS_ERROR(RIALTO_COMPONENT_COMMON, err, fmt, ##args)
#define RIALTO_COMMON_LOG_WARN(fmt, args...) RIALTO_LOG_WARN(RIALTO_COMPONENT_COMMON, fmt, ##args)
#define RIALTO_COMMON_LOG_SYS_WARN(err, fmt, args...) RIALTO_LOG_SYS_WARN(RIALTO_COMPONENT_COMMON, err, fmt, ##args)
#define RIALTO_COMMON_LOG_MIL(fmt, args...) RIALTO_LOG_MIL(RIALTO_COMPONENT_COMMON, fmt, ##args)
#define RIALTO_COMMON_LOG_INFO(fmt, args...) RIALTO_LOG_INFO(RIALTO_COMPONENT_COMMON, fmt, ##args)
#define RIALTO_COMMON_LOG_DEBUG(fmt, args...) RIALTO_LOG_DEBUG(RIALTO_COMPONENT_COMMON, fmt, ##args)

#ifdef __cplusplus
}
#endif

#endif // RIALTO_COMMON_LOGGING_H_
