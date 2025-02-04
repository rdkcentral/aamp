/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#ifndef AAMP_TSB_METADATA_MANAGER_H
#define AAMP_TSB_METADATA_MANAGER_H

#include "AampTsbMetaData.h"
#include <list>
#include <vector>
#include <mutex>
#include <functional>
#include <map>
#include <unordered_map>
#include <cinttypes>
#include <memory>
#include "AampLogManager.h"

/**
 * @class AampTsbMetaDataManager
 * @brief Manages metadata for AAMP TSB.
 */
class AampTsbMetaDataManager {
public:
	/**
	 * @brief Constructor
	 */
	AampTsbMetaDataManager();

	/**
	 * @brief Destructor
	 */
	~AampTsbMetaDataManager();

	/**
	 * @brief Initialize the metadata manager.
	 */
	void Initialize();

	/**
	 * @brief Clean up the metadata manager.
	 */
	void Cleanup();

	/**
	 * @brief Register a metadata type to be used with the manager.
	 * @param[in] type The metadata type to register.
	 * @param[in] isTransient Whether metadata of this type is transient.
	 * @return True if registered successfully, false if the type already exists.
	 * @note The isTransient property is used to determine the persistence behavior of metadata when removing items at a specific position.
	 * @details
	 * Transient Metadata (isTransient = true):
	 *
	 * * Metadata that has a temporary or ephemeral nature
	 * * When removing metadata at position P, all transient metadata items at or before position P are removed
	 * * Designed for data that should not persist beyond the point where it was encountered
	 *
	 * Non-Transient Metadata (isTransient = false):
	 *
	 * * Metadata that has a persistent nature
	 * * When removing metadata at position P, all items before position P are removed, but the latest metadata item at or before position P is preserved
	 * * Designed for data that should continue to be applicable until a newer value supersedes it
	 *
	 * This behavior is particularly visible in the @ref RemoveMetaData(const AampTime &position) method,
	 * where the code handles transient and non-transient metadata differently by adjusting the position
	 * of latestIt before performing the erasure.
	 *
	 * The distinction allows the system to maintain certain important metadata across position
	 * boundaries while clearing out metadata that should only apply to specific points in time.
	 */
	bool RegisterMetaDataType(AampTsbMetaData::Type type, bool isTransient);

	/**
	 * @brief Check if a metadata type is registered and get its properties.
	 * @param[in] type The metadata type to check.
	 * @param[out] isTransient Output parameter set to the type's transience if registered.
	 * @param[out] metadataList Reference to store pointer to the metadata list if type is registered.
	 * @return True if the type is registered, false otherwise.
	 */
	bool IsRegisteredType(AampTsbMetaData::Type type, bool& isTransient,
						 std::list<std::shared_ptr<AampTsbMetaData>>** metadataList = nullptr) const;

	/**
	 * @brief Get metadata of specific type within a time range.
	 * @tparam T Type of metadata class to retrieve (must be derived from AampTsbMetaData)
	 * @param[in] type Type of metadata to filter (must be derived from AampTsbMetaData)
	 * @param[in] rangeStart Start of time range.
	 * @param[in] rangeEnd End of time range (has to be bigger or equal to rangeStart).
	 * @return List of metadata of the specified type within the range.
	 */
	template<typename T>
	std::list<std::shared_ptr<T>> GetMetaDataByType(AampTsbMetaData::Type type, const AampTime& rangeStart, const AampTime& rangeEnd);

	/**
	 * @brief Get metadata of specific type with optional filter.
	 * @tparam T Type of metadata class to retrieve (must be derived from AampTsbMetaData)
	 * @param[in] type Type of metadata to filter
	 * @param[in] filter Optional function that returns true for metadata to include
	 * @return List of matching metadata
	 */
	template<typename T>
	std::list<std::shared_ptr<T>> GetMetaDataByType(
		AampTsbMetaData::Type type,
		std::function<bool(const std::shared_ptr<T>&)> filter = nullptr);

	/**
	 * @brief Add metadata to the manager.
	 * @param[in] metaData Metadata to add.
	 * @return True if added successfully, false otherwise.
	 */
	bool AddMetaData(const std::shared_ptr<AampTsbMetaData> &metaData);

	/**
	 * @brief Remove specific metadata from the manager.
	 * @param[in] metaData Metadata to remove.
	 * @return True if removed successfully, false otherwise.
	 */
	bool RemoveMetaData(const std::shared_ptr<AampTsbMetaData> &metaData);

	/**
	 * @brief Remove metadata at or before a specific position.
	 * @param[in] position Position in milliseconds.
	 * @return Number of items removed
	 */
	size_t RemoveMetaData(const AampTime& position);

	/**
	 * @brief Remove metadata matching specific criteria.
	 * @param[in] filter Function that returns true for metadata to remove
	 * @return Number of items removed
	 */
	size_t RemoveMetaDataIf(std::function<bool(const std::shared_ptr<AampTsbMetaData>&)> filter);

	/**
	 * @brief Get count of items in the metadata list
	 * @return Number of metadata items
	 */
	size_t GetSize() const;

	/**
	 * @brief Dump all metadata information.
	 */
	void Dump();

	/**
	 * @brief Change the position of a list of metadata items
	 * @param[in] metaDataList List of metadata items to update
	 * @param[in] newPosition New position in milliseconds
	 * @return True if all positions were updated successfully
	 */
	bool ChangeMetaDataPosition(const std::list<std::shared_ptr<AampTsbMetaData>>& metaDataList, const AampTime& newPosition);

private:
	std::map<AampTsbMetaData::Type, std::list<std::shared_ptr<AampTsbMetaData>>> mMetaDataMap; /**< Map of metadata type to list of metadata */
	std::unordered_map<AampTsbMetaData::Type, bool> mTypeTransienceMap; /**< Map of metadata type to transience flag */
	mutable std::mutex mTsbMetaDataMutex; /**< Protects the lists of metadata in the mMetaDataMap */
	uint32_t mNextOrderAdded;   /**< Next order value to assign */

	/**
	 * @brief Calculate the total count of all metadata items across all types.
	 * @note This method assumes the caller holds mTsbMetaDataMutex.
	 * @return Total count of metadata items.
	 */
	size_t CalculateTotalCount() const;
};

// Template method implementations

template<typename T>
std::list<std::shared_ptr<T>> AampTsbMetaDataManager::GetMetaDataByType(AampTsbMetaData::Type type, const AampTime& rangeStart, const AampTime& rangeEnd) {
	static_assert(std::is_base_of<AampTsbMetaData, T>::value,
				  "T must be derived from AampTsbMetaData");

	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);
	std::list<std::shared_ptr<T>> result;

	AAMPLOG_INFO("GetMetaDataByType for type %u in range %" PRIu64 "ms to %" PRIu64 "ms",
		(uint32_t)type, rangeStart.milliseconds(), rangeEnd.milliseconds());

	// Process only metadata of the matching type
	std::list<std::shared_ptr<AampTsbMetaData>>* metaDataList = nullptr;
	bool isTransient = false;
	if (!IsRegisteredType(type, isTransient, &metaDataList))
	{
		AAMPLOG_WARN("Type %u not registered", (uint32_t)type);
	}
	else if (rangeStart > rangeEnd)
	{
		AAMPLOG_WARN("Invalid range, start %" PRIu64 "ms > end %" PRIu64 "ms", rangeStart.milliseconds(), rangeEnd.milliseconds());
	}
	else if (!metaDataList->empty())
	{
		// Iterate backwards through the list to find metadata
		// Add metadata that is in range
		// Also if not transient, add the metadata that is before the range start if
		// there is no metadata exactly at range start
		for (auto it = metaDataList->rbegin(); it != metaDataList->rend(); ++it)
		{
			AampTime metaPos = (*it)->GetPosition();
			auto castData = std::dynamic_pointer_cast<T>(*it);
			if (castData == nullptr)
			{
				AAMPLOG_WARN("Failed to cast metadata of type %u to requested type", (uint32_t)type);
				continue; // Skip if not of the requested type
			}
			if (metaPos < rangeStart)
			{
				// If the type is non-transient then also need to return the metadata 
				// before rangeStart if there isn't a metadata exactly at rangeStart.
				if (!isTransient &&
					(result.empty() || (result.front()->GetPosition() > rangeStart)))
				{
					result.push_front(castData); // Maintain chronological order
				}
				break; // Can stop since list is ordered and we're going backwards
			}
			// Take into account that rangeEnd may be same as rangeStart
			if ((metaPos == rangeStart) || (metaPos < rangeEnd))
			{
				result.push_front(castData); // Maintain chronological order
			}
		}
	}
	AAMPLOG_INFO("Found %zu metadata items in range", result.size());
	return result;
}

template<typename T>
std::list<std::shared_ptr<T>> AampTsbMetaDataManager::GetMetaDataByType(
	AampTsbMetaData::Type type,
	std::function<bool(const std::shared_ptr<T>&)> filter) {

	static_assert(std::is_base_of<AampTsbMetaData, T>::value,
				  "T must be derived from AampTsbMetaData");

	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);
	std::list<std::shared_ptr<T>> result;

	AAMPLOG_INFO("GetMetaDataByType with filter for type %u", (uint32_t)type);

	std::list<std::shared_ptr<AampTsbMetaData>>* metaDataList = nullptr;
	bool isTransient = false;
	if (!IsRegisteredType(type, isTransient, &metaDataList))
	{
		AAMPLOG_WARN("Type %u not registered", (uint32_t)type);
	}
	else
	{
		for (const auto& metaData : *metaDataList)
		{
			auto castData = std::dynamic_pointer_cast<T>(metaData);
			if (castData && (!filter || filter(castData)))
			{
				result.push_back(castData);
			}
		}
		AAMPLOG_INFO("Found %zu metadata items matching filter", result.size());
	}

	return result;
}

#endif // AAMP_TSB_METADATA_MANAGER_H