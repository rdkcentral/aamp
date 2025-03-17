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
#include <mutex>
#include <thread>
#include <atomic>
#include <cerrno>
#include <cstring>

#include "TsbApi.h"
#include "TsbLog.h"
#include "TsbSem.h"
#include "TsbLocationLock.h"
#include "TsbFs.h"

#define TSB_BYTES_IN_MIB    (1024 * 1024)

using namespace TSB;
using namespace std::chrono_literals;

class TSB::StoreImpl
{
public:
	StoreImpl(const Store::Config& config, LogFunction logger, LogLevel level);

	~StoreImpl();

	Status Write(const std::string& url, const void* buffer, std::size_t size);

	Status Read(const std::string& url, void* buffer, std::size_t size) const;

	std::size_t GetSize(const std::string& url) const;

	void Delete(const std::string& url);

	void Flush();

private:
	const Logger mLogger;
	const FS::path mLocation;
	const uint32_t mMinFreePercent;
	const uintmax_t mMaxCapacity;         // Expressed in bytes
	uintmax_t mCapacity{0};               // Expressed in bytes
	uintmax_t mAvailable{0};              // Expressed in bytes
	std::atomic_uint32_t mFlushDirNum;	  // This variable is atomic because it can be accessed from multiple threads
	std::atomic_uint32_t mActiveDirNum;   // This variable is atomic because it can be accessed from multiple threads
	std::mutex mApiMutex{};
	std::thread mFlusherThread{};
	Sem mFlusherSem{};
	std::unique_ptr<LocationLock> mLocationLock{};

	static std::string SanitizePath(const std::string &);
	Status UrlToFileMapper(const std::string& url, FS::path& path) const;
	void Flusher(void);
	bool CreateDirectoriesWithPermissions(const std::filesystem::path& path);
	Status WriteBuffer(const FS::path& path, const void* buffer, std::size_t size, bool retry);
	uintmax_t GetCapacityMinFree(uintmax_t capacity) const;
	FS::space_info GetFilesystemSpace() const;
};

Store::Store(const Config& config, LogFunction logger, LogLevel level)
	: mPimpl(new StoreImpl(config, std::move(logger), level))
{
}

Store::~Store() = default;
Store::Store(Store&&) noexcept = default;
Store& Store::operator=(Store&&) noexcept = default;

Status Store::Write(const std::string& url, const void* buffer, std::size_t size)
{
	return Pimpl()->Write(url, buffer, size);
}

Status Store::Read(const std::string& url, void* buffer, std::size_t size) const
{
	return Pimpl()->Read(url, buffer, size);
}

std::size_t Store::GetSize(const std::string& url) const
{
	return Pimpl()->GetSize(url);
}

void Store::Delete(const std::string& url)
{
	Pimpl()->Delete(url);
}

void Store::Flush()
{
	Pimpl()->Flush();
}

/*	@fn		UrlToFileMapper
	@brief	Generates a path (directory and name of a file) from a url

	@param[in] url - 		The url that will be split into directory path and filename
	@param[out] path - 		The path (directory and file name) that corresponds to a url
	@retval		OK  		If the url was successfully split.
				FAILED 		If the input url string was erratic.
*/
Status StoreImpl::UrlToFileMapper(const std::string& url, FS::path& path) const
{
	auto returnStatus = Status::FAILED;

	if (url.empty())
	{
		TSB_LOG_WARN(mLogger, "Url string is empty");
	}
	else
	{
		std::string separator("://");

		size_t separatorPos = url.find(separator);
		size_t filePathPos = (separatorPos == std::string::npos) ? 0 : (separatorPos + separator.length());

		if (filePathPos > url.length())
		{
			TSB_LOG_WARN(mLogger, "Invalid file path position", "filePathPos", filePathPos, "urlLength", url.length());
		}
		else
		{
			FS::path p = url.substr(filePathPos);

			std::string dirPath = p.parent_path();
			std::string fileName = p.filename();

			if (dirPath[0] == '/')
			{
				TSB_LOG_WARN(mLogger, "Directory begins with invalid delimiter", "path", dirPath);
			}
			else if (dirPath.find("//") != std::string::npos)
			{
				TSB_LOG_WARN(mLogger, "Invalid delimiter in the dir path", "path", dirPath);
			}
			else if (dirPath.find("..") != std::string::npos)
			{
				TSB_LOG_WARN(mLogger, "Invalid storage access", "path", dirPath);
			}
			else if (url.find('\0') != std::string::npos)
			{
				TSB_LOG_WARN(mLogger, "Invalid UTF-8 character url", "url", url);
			}
			else if (fileName.empty())
			{
				TSB_LOG_WARN(mLogger, "No file is supplied in url", "url", url);
			}
			else
			{
				path = mLocation;
				auto activeDirNum = mActiveDirNum.load();
				path /= std::to_string(activeDirNum);
				path /= p;
				returnStatus = Status::OK;
			}
		}
	}
	return returnStatus;
}

/*	@fn		SanitizePath
	@brief	Sanitize the incoming path for correctness

	@param[in] path - The path to sanitize
	@retval			  corrected path

	Currently checks for and removes trailing slash.
*/
std::string StoreImpl::SanitizePath(const std::string& path)
{
	std::string	location = path;

	if (location.back() == '/')
	{
		location.pop_back() ;
	}
	return location;
}

void StoreImpl::Flusher(void)
{
	TSB_LOG_TRACE(mLogger, "Flusher thread running");
	bool exit{false};
	std::error_code ec;

	while(!exit)
	{
		mFlusherSem.Wait();

		FS::path flushDir{mLocation / std::to_string(mFlushDirNum.load())};
		TSB_LOG_MIL(mLogger, "Flush storage content", "flushDirectory", flushDir);
		std::uintmax_t numRemoved{FS::remove_all(flushDir, ec)};
		if (numRemoved == static_cast<std::uintmax_t>(-1))
		{
			TSB_LOG_ERROR(mLogger, "Failed to delete files", "flushDir", flushDir, "errorCode", ec);
		}
		else
		{
			TSB_LOG_MIL(mLogger, "Flush storage content complete", "numFileAndDirRemoved", numRemoved);
		}

		if (mFlushDirNum.load() == mActiveDirNum.load())
		{
			exit = true;
		}
		else
		{
			(void) mFlushDirNum.fetch_add(1);
		}
	}
	TSB_LOG_TRACE(mLogger, "Exit Flusher");
}

bool StoreImpl::CreateDirectoriesWithPermissions(const std::filesystem::path& path)
{
	std::error_code ec;
	std::filesystem::path currentPath;
	bool success = true;

	for (auto it = path.begin(); it != path.end(); ++it)
	{
		currentPath /= *it;
		if (!FS::exists(currentPath))
		{
			if (!FS::create_directory(currentPath, ec) && ec)
			{
				TSB_LOG_ERROR(mLogger, "Failed to create directory", "directory", currentPath, "errorCode", ec);
				success = false;
				break;
			}
			// Set all permissions on each directory created. This is necessary so other AAMP instances can
			// access the directory, in case the first instance does not exit cleanly.
			FS::permissions(currentPath, FS::perms::all, ec);
			if (ec)
			{
				TSB_LOG_ERROR(mLogger, "Failed to set permissions", "directory", currentPath, "errorCode", ec);
				success = false;
				break;
			}
		}
	}
	return success;
}

StoreImpl::StoreImpl(const Store::Config& config, LogFunction logger, LogLevel level)
					: mLogger{std::move(logger), level},
					mLocation(SanitizePath(config.location)),
					mMinFreePercent(config.minFreePercentage),
					mMaxCapacity(static_cast<uintmax_t>(config.maxCapacity) * TSB_BYTES_IN_MIB),
					mFlushDirNum(0),
					mActiveDirNum(1)
{
	std::error_code ec;

	TSB_LOG_TRACE(mLogger, "Construct Store", "location", config.location, "minFreePercentage",
				  mMinFreePercent, "maxCapacity", mMaxCapacity);

	if (!mLocation.is_absolute())
	{
		TSB_LOG_ERROR(mLogger, "Location is not a valid absolute path", "location", mLocation);
		throw std::invalid_argument("Location is not a valid absolute path");
	}

	// Create the base directory and the initial flush directory, if they don't exist
	FS::path flushDir = mLocation / std::to_string(mFlushDirNum.load());
	if (!CreateDirectoriesWithPermissions(flushDir))
	{
		TSB_LOG_ERROR(mLogger, "Failed to create", "flushDir", flushDir);
		throw std::invalid_argument("Failed to create flushDir");
	}

	mLocationLock = std::make_unique<LocationLock>(mLocation);
	if (mLocationLock->Lock() == Status::FAILED)
	{
		TSB_LOG_WARN(mLogger, "Another TSB Store instance is currently using the configured location",
					 "location", mLocation);
		throw std::invalid_argument("Another TSB Store instance is currently using the configured location");
	}

	if (mMinFreePercent > 100)
	{
		TSB_LOG_ERROR(mLogger, "Invalid minimum free space", "percentage", mMinFreePercent);
		throw std::invalid_argument("Invalid minimum free space percentage");
	}

	// Move any stale files / directories present in the storage due to a non clean shutdown.
	for (const auto& dir_entry : FS::directory_iterator{mLocation})
	{
		if (dir_entry.path() != flushDir)
		{
			FS::rename(dir_entry.path(), flushDir / dir_entry.path().filename(), ec);
			if (ec.default_error_condition())
			{
				TSB_LOG_ERROR(mLogger, "Failed to move stale directory", "path",
								dir_entry.path(), "errorCode", ec);
			}
		}
	}

	FS::space_info s = GetFilesystemSpace();
	if (s.capacity == static_cast<std::uintmax_t>(-1))
	{
		TSB_LOG_ERROR(mLogger, "Error getting filesystem capacity");
		throw std::invalid_argument("Error getting filesystem capacity");
	}
	else
	{
		mCapacity = std::min(mMaxCapacity, GetCapacityMinFree(s.capacity));
		mAvailable = mCapacity;
		mFlusherThread = std::thread(&StoreImpl::Flusher, this);
		mFlusherSem.Post();
		TSB_LOG_MIL(mLogger, "Store Constructed", "instance", this,
					"location", mLocation, "availableSpace", mAvailable,
					"activeDirNum", mActiveDirNum.load());
	}
}

StoreImpl::~StoreImpl()
{
	TSB_LOG_TRACE(mLogger, "Destroy Store");
	mFlusherSem.Post();
	if(mFlusherThread.joinable())
	{
		mFlusherThread.join();
	}
	TSB_LOG_MIL(mLogger, "Store Destructed", "instance", this);
}

Status StoreImpl::Read(const std::string &url, void *buffer, std::size_t size) const
{
	Status returnStatus = Status::FAILED;
	std::error_code ec;
	FS::path path;
	if (UrlToFileMapper(url, path) != Status::OK)
	{
		TSB_LOG_ERROR(mLogger, "Could not map URL to a file", "segmentUrl", url);
	}
	else if (!FS::exists(path))
	{
		TSB_LOG_WARN(mLogger, "File does not exist", "file", path);
	}
	else if (buffer == nullptr)
	{
		TSB_LOG_ERROR(mLogger, "Buffer is null");
	}
	else if (size == 0)
	{
		TSB_LOG_ERROR(mLogger, "Size is 0");
	}
	else
	{
		FS::ifstream stream;
		stream.open(path, FS::ifstream::binary);
		if (stream.fail())
		{
			TSB_LOG_ERROR(mLogger, "Failed to open the file", "file", path);
		}
		else
		{
			stream.read(static_cast<char *>(buffer), size);
			if (stream.fail())
			{
				TSB_LOG_ERROR(mLogger, "Failed to read file", "file", path, "size", size);
			}
			else
			{
				TSB_LOG_TRACE(mLogger, "File Read", "file", path, "size", size);
				returnStatus = Status::OK;
			}
			stream.close();
		}
	}
	return returnStatus;
}

Status StoreImpl::WriteBuffer(const FS::path& path, const void* buffer, std::size_t size, bool retry)
{
	auto status = Status::FAILED;
	auto timeWaited = 0ms;
	constexpr auto timeout = 5000ms;
	constexpr auto sleepTime = 2ms;

	do
	{
		FS::ofstream file;
		std::error_code ec;
		/* Set the buffer size to 0, so the data is not buffered.
		 * The client is writing one segment at a time. With that amount of data, using an
		 * intermediate buffer will only degrade performance and slow things down. */
		file.rdbuf()->pubsetbuf(nullptr, 0);
		file.open(path, FS::ofstream::binary);
		if (file.fail())
		{
			retry = false;
			TSB_LOG_ERROR(mLogger, "Failed to open the file", "file", path);
		}
		else
		{
			// Set read and write permissions for all. This is necessary so other AAMP instances can
			// delete the file, in case the first instance does not exit cleanly.
			FS::perms permissions = FS::perms::owner_read | FS::perms::owner_write |
									FS::perms::group_read | FS::perms::group_write |
									FS::perms::others_read | FS::perms::others_write;
			FS::permissions(path, permissions, ec);
			if (ec)
			{
				TSB_LOG_ERROR(mLogger, "Failed to set permissions", "file", path, "errorCode", ec);
			}
			else
			{
				file.write(static_cast<const char *>(buffer), size);
				// The ofstream's bad bit is set on writing errors raised by the underlying
				// platform-specific implementation, such as ENOSPC.  The ENOSPC checks below assume
				// that the implementation sets errno when such writing errors occur, which is
				// highly likely on Linux-based systems as they will be using the Linux C standard
				// library write() function.  This behavior may not be portable to other systems.
				bool errnoSetFollowingWriteError = file.bad();
				if (file.fail() == false)
				{
					mAvailable -= size;
					TSB_LOG_TRACE(mLogger, "File written", "file", path,
									"fileSize", size, "availableSpace", mAvailable);
					retry = false;
					status = Status::OK;
				}
				else if ((timeWaited >= timeout) && errnoSetFollowingWriteError && (errno == ENOSPC))
				{
					retry = false;
					TSB_LOG_ERROR(mLogger, "Not enough space to write - timed out", "file", path, "timeout", timeout.count());
					status = Status::NO_SPACE;
				}
				// Timeout, but the write failed for a reason other than ENOSPC
				else if (timeWaited >= timeout)
				{
					retry = false;
					TSB_LOG_ERROR(mLogger, "Failed to write - timed out", "file", path, "timeout", timeout.count(),
									"errno", std::strerror(errno));
				}
				else if (retry && errnoSetFollowingWriteError && (errno == ENOSPC))
				{
					TSB_LOG_TRACE(mLogger, "Not enough space to write - retrying...",
									"sleepTime", sleepTime.count(),
									"fileSize", size,
									"available", mAvailable,
									"errno", std::strerror(errno));
					FS::sleep_for(sleepTime);
					timeWaited += sleepTime;
				}
				else if (errnoSetFollowingWriteError && (errno == ENOSPC))
				{
					// This is a TRACE only, as the client can cull (delete files) and retry the Write
					TSB_LOG_TRACE(mLogger, "Not enough space to write", "file", path,
									"size", size);
					status = Status::NO_SPACE;
				}
				else // No timeout, but the write failed for a reason other than ENOSPC
				{
					retry = false;
					TSB_LOG_ERROR(mLogger, "Failed to write to file", "file", path,
									"size", size, "errno", std::strerror(errno));
				}
			}

			file.close();
			if (status != Status::OK)
			{
				std::error_code ec;

				// If the write failed, the file may have been created.
				// Therefore remove it to allow retries - either by TSB, or its client.
				if (!FS::remove(path, ec))
				{
					TSB_LOG_WARN(mLogger, "Error deleting file", "file", path, "errorCode", ec);
				}
			}
		}
	} while (retry);

	return status;
}

Status StoreImpl::Write(const std::string& url, const void* buffer, std::size_t size)
{
	std::unique_lock<std::mutex> lock(mApiMutex);
	auto returnStatus = Status::FAILED;
	std::error_code ec;
	FS::path path;
	if (UrlToFileMapper(url, path) != Status::OK)
	{
		TSB_LOG_ERROR(mLogger, "Could not map URL to a file", "segmentUrl", url);
	}
	else if (FS::exists(path))
	{
		TSB_LOG_TRACE(mLogger, "File already exists", "path", path);
		returnStatus = Status::ALREADY_EXISTS;
	}
	else if (buffer == nullptr)
	{
		TSB_LOG_ERROR(mLogger, "Buffer is null");
	}
	else if (size == 0)
	{
		TSB_LOG_ERROR(mLogger, "Size is 0");
	}
	else if (size > mAvailable)
	{
		// This is a TRACE only, as the client can cull (delete files) and retry the Write
		TSB_LOG_TRACE(mLogger, "Not Enough space to write", "file", path, "fileSize", size,
						"availableSpace", mAvailable);
		returnStatus = Status::NO_SPACE;
	}
	else if (!CreateDirectoriesWithPermissions(path.parent_path()))
	{
		TSB_LOG_ERROR(mLogger, "Failed to create directory", "directory", path.parent_path());
	}
	else
	{
		// Do not retry in normal Write, not happening during Flush
		bool retry = false;
		if (mFlushDirNum.load() != mActiveDirNum.load())
		{
			// Write during Flush
			retry = true;
		}
		returnStatus = WriteBuffer(path, buffer, size, retry);
	}

	return returnStatus;
}

std::size_t StoreImpl::GetSize(const std::string& url) const
{
	std::size_t returnSize = 0;
	FS::path path;
	if (UrlToFileMapper(url, path) != Status::OK)
	{
		TSB_LOG_ERROR(mLogger, "Could not map URL to a file", "segmentUrl", url);
	}
	else if (!FS::exists(path))
	{
		TSB_LOG_WARN(mLogger, "File does not exist", "path", path);
	}
	else
	{
		std::error_code ec;
		std::uintmax_t segmentSize = FS::file_size(path, ec);
		if (segmentSize == static_cast<std::uintmax_t>(-1))
		{
			TSB_LOG_WARN(mLogger, "Error getting file size", "file", path, "error", ec);
		}
		else
		{
			TSB_LOG_TRACE(mLogger, "Got size", "file", path, "segmentSize", segmentSize);
			returnSize = segmentSize;
		}
	}
	return returnSize;
}

void StoreImpl::Delete(const std::string& url)
{
	std::unique_lock<std::mutex> lock(mApiMutex);
	FS::path path;
	if (UrlToFileMapper(url, path) != Status::OK)
	{
		TSB_LOG_ERROR(mLogger, "Could not map URL to a file", "segmentUrl", url);
	}
	else if (!FS::exists(path))
	{
		// File probably deleted already, so TRACE log only
		TSB_LOG_TRACE(mLogger, "File does not exist", "path", path);
	}
	else
	{
		std::error_code ec;
		std::uintmax_t size = FS::file_size(path, ec);
		if (size == static_cast<std::uintmax_t>(-1))
		{
			TSB_LOG_WARN(mLogger, "Error getting file size", "file", path, "errorCode", ec);
		}
		else if (!FS::remove(path, ec))
		{
			TSB_LOG_WARN(mLogger, "Error deleting file", "file", path, "errorCode", ec);
		}
		else
		{
			mAvailable += size;
			TSB_LOG_TRACE(mLogger, "Deleted file", "file", path, "fileSize", size, "availableSpace", mAvailable);
		}
	}
}

void StoreImpl::Flush()
{
	TSB_LOG_TRACE(mLogger, "Do Flush");
	{
		std::unique_lock<std::mutex> lock(mApiMutex);
		uintmax_t oldAvailable = mAvailable;
		uint32_t oldActiveDirNum = mActiveDirNum.fetch_add(1);
		mAvailable = mCapacity;

		TSB_LOG_MIL(mLogger, "Flush triggered", "oldActiveDirNum", oldActiveDirNum,
					"oldAvailableSpace", oldAvailable, "activeDirNum", (oldActiveDirNum + 1),
					"availableSpace", mAvailable);
	}
	mFlusherSem.Post();
}

uintmax_t StoreImpl::GetCapacityMinFree(uintmax_t capacity) const
{
	return (capacity * (100 - mMinFreePercent)) / 100;
}

FS::space_info StoreImpl::GetFilesystemSpace() const
{
	FS::space_info spaceInfo{static_cast<std::uintmax_t>(-1), static_cast<std::uintmax_t>(-1),
							 static_cast<std::uintmax_t>(-1)};

	std::error_code ec;

	spaceInfo = FS::space(mLocation, ec);
	if (ec.default_error_condition())
	{
		TSB_LOG_WARN(mLogger, "Error getting space", "location", mLocation, "errorCode", ec);
	}

	TSB_LOG_TRACE(mLogger, "Filesystem space", "capacity", spaceInfo.capacity,
				  "available", spaceInfo.available);
	return spaceInfo;
}
