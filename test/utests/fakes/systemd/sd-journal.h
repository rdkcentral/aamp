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
 * @file sd-journal.h
 * @brief Stub systemd/sd-journal.h for macOS L1 test builds.
 *
 * Only the declarations needed by the unit-test fakes are provided.
 */
#ifndef STUB_SD_JOURNAL_H
#define STUB_SD_JOURNAL_H

#include <stdarg.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

int sd_journal_printv_with_location(int priority, const char *file,
                                    const char *line, const char *func,
                                    const char *format, va_list arg);

int sd_journal_printv(int priority, const char *format, va_list arg);

#ifdef __cplusplus
}
#endif

#endif /* STUB_SD_JOURNAL_H */
