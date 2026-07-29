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
 * @file IsoBmffLog.h
 * @brief Logging facade for the isobmff parser sources.
 *
 * These sources are compiled into both libaamp (AAMP core) and
 * libplayergstinterface (middleware). The two libraries use different
 * logging frameworks (AAMPLOG_* vs MW_LOG_*) and must not depend on
 * each other. This header selects the active backend at compile time:
 *
 *   - Define ISOBMFF_LOG_BACKEND_MW   to route through PlayerLogManager.
 *   - Otherwise (default)             the AAMP backend is used.
 *
 * Call sites use the AAMPLOG_* names unchanged; in the MW build they
 * are remapped to MW_LOG_* equivalents.
 */

#ifndef __ISOBMFF_LOG_H__
#define __ISOBMFF_LOG_H__

#if defined(ISOBMFF_LOG_BACKEND_MW)

#include "PlayerLogManager.h"

#ifndef AAMPLOG_TRACE
#define AAMPLOG_TRACE MW_LOG_TRACE
#define AAMPLOG_DEBUG MW_LOG_DEBUG
#define AAMPLOG_INFO  MW_LOG_INFO
#define AAMPLOG_WARN  MW_LOG_WARN
#define AAMPLOG_MIL   MW_LOG_MIL
#define AAMPLOG_ERR   MW_LOG_ERR
#endif

#else

#include "AampLogManager.h"

#endif

#endif /* __ISOBMFF_LOG_H__ */
