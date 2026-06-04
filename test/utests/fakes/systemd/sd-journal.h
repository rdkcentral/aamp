/*
 * Stub systemd/sd-journal.h for macOS L1 test builds.
 * Only the declarations needed by FakeSdJournal.cpp are provided.
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
