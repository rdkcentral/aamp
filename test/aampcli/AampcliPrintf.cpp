/*
 *   Copyright 2025 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
*/

/***************************************************
 * @file AampcliPrintf.cpp
 * @brief printf()-like API for aamp-cli logging.
 ***************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <alloca.h>
#include <sys/time.h>

#define TIMESTAMP_FMT "%lu.%03lu: "

static int _local_printf(int buf_size, const char *format, va_list ap)
{
	char *buf = (char *)alloca(buf_size);
	int required_size = vsnprintf(buf, (size_t)buf_size, format, ap);

	// Note: vsnprintf() does not include the terminating nul byte in
	//       its return value. So 0 is valid for the empty string.
	if (required_size >= 0)
	{
		++required_size;	/* Include the nul terminator. */

		if (buf_size)
		{
			/* We have a buffer - print it. */
			struct timeval tv;
			(void) gettimeofday(&tv, NULL);

			if (required_size <= buf_size)
			{
				/* Successfully formatted the buffer - output it. */
				printf(TIMESTAMP_FMT "%s", tv.tv_sec, tv.tv_usec / 1000, buf);
			}
			else
			{
				printf(
					TIMESTAMP_FMT
					"ERROR: vsnprintf() required %d bytes, but only had %d\n",
					tv.tv_sec, tv.tv_usec / 1000, required_size, buf_size);
			}
		}
	}

	return required_size;
}

int AAMPCLI_PRINTF(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	int required_size = _local_printf(0, format, ap);
	va_end(ap);

	if (required_size > 0)
	{
		va_start(ap, format);
		(void) _local_printf(required_size, format, ap);
		va_end(ap);

		// Be consistent with PRINTF(3) in case it's checked ...
		--required_size;
	}
	else
	{
		printf("ERROR: AAMPCLI_PRINTF() failed to determine the buffer size required"
		       " (Format string was '%s')\n", format);

		required_size = -1;
	}

	return required_size;
}
