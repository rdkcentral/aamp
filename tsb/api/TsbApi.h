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

#ifndef __TSB_API_H__
#define __TSB_API_H__

#include <cstdint>
#include <cstddef>
#include <memory>
#include <functional>
#include <string>

namespace TSB
{

/**
 * @brief API statuses returned by the TSB library
 */
enum class Status
{
	OK,             /**< Operation completed successfully. */
	FAILED,         /**< Operation failed due to an error condition. */
	NO_SPACE,       /**< There is insufficient space to complete the operation. */
	ALREADY_EXISTS  /**< File already exists when not expected. */
};

/**
 * @brief Log levels supported by the TSB library
 */
enum class LogLevel
{
	TRACE, /**< Debug logs for investigating issues. Should not appear in production. */
	WARN,  /**< Operation continues without error but through an unusual path. */
	MIL,   /**< Milestone. A significant operation completed, e.g. Flush, end of constructor. */
	ERROR  /**< Operation cannot continue, e.g. invalid arguments or filesystem error. */
};

/**
 *  @fn Logging callback
 *
 *  @brief Function that allows the TSB library to send log messages to the client for output.
 *         The function supplied by the client must not throw exceptions.
 *
 *  @param[in] message - message string to print. The client takes ownership of this.
 *
 *  NOTE: The message will contain human-readable message text, and may contain related
 *        context-dependent data. The message will also contain some logging metadata.
 *
 *        Log message metadata may include (but is not limited to):
 *          - Log level
 *          - Thread ID
 *          - Function name
 *          - Source file name
 *          - Line number
 *
 *        The client must not make assumptions regarding format or content of TSB log messages.
 */
using LogFunction = std::function<void(std::string&& message)>;

/**
 * @brief Time-Shifted Buffer (TSB) Store API for storing segment data.
 *
 *        The API is thread-safe.
 */
class StoreImpl;

class Store
{
public:
	struct Config
	{
		/**
		 * @brief Location of the Store, as an absolute filesystem path.
		 *        The path may or may not have a trailing slash (/).
		 *
		 *        The location must be unique to the Store - it must not be shared with other
		 *        parts of the system, or other Store instances.
		 *
		 *        Example: "/tmp/data/tsb_location/main_asset/"
		 */
		std::string location;

		/**
		 * @brief Minimum storage percentage to keep free in the mounted file system
		 *        used by 'location'.
		 *
		 *        Example: 5
		 */
		uint32_t minFreePercentage{0};

		/**
		 * @brief Maximum capacity to allocate in the mounted file system
		 *        used by 'location', in mebibytes (MiB = 1024 * 1024 bytes).
		 *
		 *        Example: 10240
		 */
		uint32_t maxCapacity{0};
	};

	/**
	 *  @fn Store (constructor)
	 *
	 *  @brief Creates and initialises a new, empty Store instance for segment data.
	 *         This includes doing a Flush to delete any stale files at the Store's location.
	 *
	 *         In normal usage, the Store's location should be empty on construction,
	 *         but if the previous TSB Store instance did not exit cleanly (for example
	 *         the process it was running in crashed, or the user power-cycled the STB),
	 *         then there may be stale segment data present.
	 *
	 *         NOTE: The Flush on construction is asynchronous - the client can write segments
	 *         to the store while the deletion of any stale files takes place in the background.
	 *
	 *  @param[in] config - configuration parameters. See Config struct above for details.
	 *  @param[in] logger - logging function that the Store can use to send log messages to client
	 *  @param[in] level - lowest log level to output, for example LogLevel::WARN will output WARN,
	 *                     MIL and ERROR
	 *
	 *  @throw std::invalid_argument if the config is invalid.
	 * 		   The exception message will detail the exact failure.
	 */
	explicit Store(const Config& config, LogFunction logger, LogLevel level);

	/**
	 *  @fn ~Store (destructor)
	 *
	 *  @brief Destroys the Store instance.
	 *         This includes doing a Flush to clear all data currently in the Store.
	 *
	 *         NOTE: The Flush on destruction is synchronous, and may therefore take a significant
	 *         amount of time to complete, depending on how much data is currently in the Store.
	 */
	~Store();

	Store(Store&&) noexcept;
	Store& operator=(Store&&) noexcept;

	// No copying as it's not possible to have two stores at the same filesystem location
	Store(const Store& other) = delete;
	Store& operator=(const Store& other) = delete;

	/**
	 *  @fn Write
	 *
	 *  @brief Writes the given segment data buffer to the Store, at a location associated with
	 *         the given URL.  The client owns the buffer - it is responsible for allocating and
	 *         deallocating it.
	 *
	 *         While the user remains tuned to a given channel, it's expected that the client
	 *         will Write data to the Store until the API returns NO_SPACE.  This signifies that
	 *         the time-shifted buffer has reached its maximum depth.  The client can then cull
	 *         the oldest segment(s) using the Delete API, and retry a Write of the latest segment.
	 *
	 *         NOTE: This API is synchronous and may take significant time to return.
	 *         It will, however, return quickly without attempting to write to the filesystem
	 *         if the NO_SPACE condition is detected.
	 *         Writing and deleting segments from different threads is thread-safe.
	 *
	 *  @param[in] url - segment URL, can be fully qualified with "https://"
	 *  @param[in] buffer - buffer containing segment data to store
	 *  @param[in] size - number of bytes to write from the buffer to the store
	 *
	 *  @retval Status::OK on success
	 *  @retval Status::NO_SPACE if there is insufficient free space to store the segment data
	 *  @retval Status::ALREADY_EXISTS if the segment already exists in the store
	 *  @retval Status::FAILED on invalid argument, filesystem or other failure
	 */
	Status Write(const std::string& url, const void* buffer, std::size_t size);

	/**
	 *  @fn Read
	 *
	 *  @brief Reads the segment data associated with the given URL from the Store
	 *         to the given segment buffer.
	 *
	 *         The client owns the buffer - it is responsible for allocating and deallocating it.
	 *         Use the GetSize API to get a minimum size for the buffer.
	 *
	 *         NOTE: This API is synchronous and may take a significant time to return.
	 *
	 *  @param[in] url - segment URL, can be fully qualified with "https://"
	 *  @param[out] buffer - segment buffer to fill
	 *  @param[in] size - number of bytes to read from the store to the buffer
	 *
	 *  @retval Status::OK on success
	 *  @retval Status::FAILED on invalid argument, filesystem or other failure
	 */
	Status Read(const std::string& url, void* buffer, std::size_t size) const;

	/**
	 *  @fn GetSize
	 *
	 *  @brief Gets the size in bytes of the segment data in the Store
	 *         that is associated with the given URL.
	 *
	 *         The client can use this size to ensure that a suitable buffer
	 *         is passed to the Read API.
	 *
	 *         NOTE: This API is synchronous.
	 *
	 *  @param[in] url - segment URL, can be fully qualified with "https://"
	 *
	 *  @return Segment size in bytes, or 0 on error
	 */
	std::size_t GetSize(const std::string& url) const;

	/**
	 *  @fn Delete
	 *
	 *  @brief Deletes the segment data from the Store that is associated with the given URL.
	 *
	 *         NOTE: This API is synchronous.
	 *         Writing and deleting segments from different threads is thread-safe.
	 *
	 *  @param[in] url - segment URL, can be fully qualified with "https://"
	 */
	void Delete(const std::string& url);

	/**
	 *  @fn Flush
	 *
	 *  @brief Deletes all segment data from the Store and assigns a new place within the
	 *         configured location where the Store will start writing new segment data.
	 *
	 *         It's expected that the client will call Flush on a channel change.
	 *
	 *         NOTE: This API is asynchronous - the client can write new segments to the store
	 *         for a new channel while old segments are deleted in the background.
	 */
	void Flush();

private:
	const StoreImpl* Pimpl() const
	{
		return mPimpl.get();
	}

	StoreImpl* Pimpl()
	{
		return mPimpl.get();
	}

	std::unique_ptr<StoreImpl> mPimpl;
};

} // namespace TSB

#endif /* __TSB_API_H__ */
