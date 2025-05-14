/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include "TsbFs.h"
#include "TsbSem.h"

#include "TsbMockFilesystem.h"
#include "TsbMockOfstream.h"
#include "TsbMockIfstream.h"
#include "TsbMockDirectoryIterator.h"
#include "TsbMockLibc.h"

TsbMockFilesystem* g_mockFilesystem = nullptr;
TsbMockOfstream* g_mockOfstream = nullptr;
TsbMockIfstream* g_mockIfstream = nullptr;
TsbMockDirectorIterator* g_mockDirectoryIterator = nullptr;
TsbMockLibc* g_mockLibc = nullptr;

#ifdef ENABLE_TSB_FAKES_LOG
#define LOG(msg)                                                                                   \
	do                                                                                             \
	{                                                                                              \
		std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << "() " << (msg)              \
				  << std::endl;                                                                    \
	} while (0)
#else
#define LOG(msg)                                                                                   \
	do                                                                                             \
	{                                                                                              \
	} while (0)
#endif

namespace TSB
{

namespace FS
{

directory_entry directory_iterator::operator*()
{
	directory_entry rv{};
	LOG("Derefencing mock iterator to get directory entry");
	if (g_mockDirectoryIterator)
	{
		rv = g_mockDirectoryIterator->DeRef();
	}
	return rv;
}

bool directory_iterator::operator!=(const directory_iterator& __iter)
{
	bool rv = false;
	LOG("Checking mock iterator for inequality with end");
	if (g_mockDirectoryIterator)
	{
		rv = g_mockDirectoryIterator->NotEq(__iter);
	}
	return rv;
}

directory_iterator begin(directory_iterator __iter)
{
	LOG("Mock iterator begin");
	return __iter;
}

directory_iterator end(directory_iterator)
{
	LOG("Mock iterator end");
	// Return a past-the-end iterator
	return directory_iterator();
}

basic_filebuf* ofstream::rdbuf() const
{
	basic_filebuf* rv = nullptr;
	if (g_mockOfstream)
	{
		rv = g_mockOfstream->rdbuf();
	}
	LOG(rv);
	return rv;
}

void ofstream::open(const std::string& __s, std::ios_base::openmode __mode)
{
	LOG(__s);
	if (g_mockOfstream)
	{
		g_mockOfstream->open(__s, __mode);
	}
}

bool ofstream::fail() const
{
	bool rv = false;
	if (g_mockOfstream)
	{
		rv = g_mockOfstream->fail();
	}
	LOG(rv);
	return rv;
}

bool ofstream::bad() const
{
	bool rv = false;
	if (g_mockOfstream)
	{
		rv = g_mockOfstream->bad();
	}
	LOG(rv);
	return rv;
}

void ofstream::clear (std::ios_base::iostate state)
{
	LOG(state);
	if (g_mockOfstream)
	{
		g_mockOfstream->clear(state);
	}
}

void ofstream::write(const char* __s, std::streamsize __n)
{
	LOG(__n);
	if (g_mockOfstream)
	{
		g_mockOfstream->write(__s, __n);
	}
}

void ofstream::close()
{
	LOG("Closing");
	if (g_mockOfstream)
	{
		g_mockOfstream->close();
	}
}

void ifstream::open(const std::string& __s, std::ios_base::openmode __mode)
{
	LOG(__s);
	if (g_mockIfstream)
	{
		g_mockIfstream->open(__s, __mode);
	}
}

bool ifstream::fail() const
{
	bool rv = false;
	if (g_mockIfstream)
	{
		rv = g_mockIfstream->fail();
	}
	LOG(rv);
	return rv;
}

void ifstream::read(char* __s, std::streamsize __n)
{
	LOG(__n);
	if (g_mockIfstream)
	{
		g_mockIfstream->read(__s, __n);
	}
}

void ifstream::close()
{
	LOG("Closing");
	if (g_mockIfstream)
	{
		g_mockIfstream->close();
	}
}

bool create_directory(const path& __p, std::error_code& __ec)
{
	bool rv = true;
	LOG(__p);
	if (g_mockFilesystem)
	{
		rv = g_mockFilesystem->create_directory(__p, __ec);
	}
	LOG(rv);
	return rv;
}

bool exists(const path& __p)
{
	bool rv = true;
	LOG(__p);
	if (g_mockFilesystem)
	{
		rv = g_mockFilesystem->exists(__p);
	}
	LOG(rv);
	return rv;
}

uintmax_t file_size(const path& __p, std::error_code& __ec)
{
	uintmax_t rv = 1;
	LOG(__p);
	if (g_mockFilesystem)
	{
		rv = g_mockFilesystem->file_size(__p, __ec);
	}
	LOG(rv);
	return rv;
}

void permissions(const path &__p, perms __prms, std::error_code &__ec)
{
	LOG(__p);
	if (g_mockFilesystem)
	{
		g_mockFilesystem->permissions(__p, __prms, __ec);
	}
}

bool remove(const path& __p, std::error_code& __ec)
{
	bool rv = true;
	LOG(__p);
	if (g_mockFilesystem)
	{
		rv = g_mockFilesystem->remove(__p, __ec);
	}
	LOG(rv);
	return rv;
}

uintmax_t remove_all(const path& __p, std::error_code& __ec)
{
	uintmax_t rv = 1;
	LOG(__p);
	if (g_mockFilesystem)
	{
		rv = g_mockFilesystem->remove_all(__p, __ec);
		if (g_mockFilesystem->mockRemoveAllSem)
		{
			LOG("Starting wait on sem");
			g_mockFilesystem->mockRemoveAllSem->Wait();
			LOG("Finished wait on sem");
		}
		g_mockFilesystem->mockRemoveAllCompleted = true;
	}
	LOG(rv);
	return rv;
}

void rename(const path& __from, const path& __to, std::error_code& __ec)
{
	LOG(__from);
	LOG(__to);
	if (g_mockFilesystem)
	{
		g_mockFilesystem->rename(__from, __to, __ec);
	}
}

space_info space(const path& __p, std::error_code& __ec)
{
	space_info rv{};
	LOG(__p);
	if (g_mockFilesystem)
	{
		rv = g_mockFilesystem->space(__p, __ec);
	}
	LOG(rv.capacity);
	LOG(rv.available);
	return rv;
}

int open(const char *pathname, int flags)
{
	int rv = 0;
	LOG(pathname);
	LOG(flags);
	if (g_mockLibc)
	{
		rv = g_mockLibc->open(pathname, flags);
	}
	LOG(rv);
	return rv;
}

int close(int fd)
{
	int rv = 0;
	LOG(fd);
	if (g_mockLibc)
	{
		rv = g_mockLibc->close(fd);
	}
	LOG(rv);
	return rv;
}

int flock(int fd, int op)
{
	int rv = 0;
	LOG(fd);
	LOG(op);
	if (g_mockLibc)
	{
		rv = g_mockLibc->flock(fd, op);
	}
	LOG(rv);
	return rv;
}

} // namespace FS

} // namespace TSB
