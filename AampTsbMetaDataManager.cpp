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

#include <inttypes.h>
#include <memory>
#include "AampTsbMetaDataManager.h"
#include "AampLogManager.h"

// AampTsbMetaDataManager implementation
AampTsbMetaDataManager::AampTsbMetaDataManager()
	: mNextOrderAdded(1)
{
	// Constructor implementation
}

AampTsbMetaDataManager::~AampTsbMetaDataManager()
{
	Cleanup();
}

void AampTsbMetaDataManager::Initialize()
{
	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);
	mMetaDataMap.clear();
	mTypeTransienceMap.clear();
}

void AampTsbMetaDataManager::Cleanup()
{
	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);
	mMetaDataMap.clear();
	mTypeTransienceMap.clear();
}

/**
 * @brief Register a metadata type to be used with the manager.
 * @param type The metadata type to register.
 * @param isTransient Whether metadata of this type is transient.
 * @return True if registered successfully, false if the type already exists.
 */
bool AampTsbMetaDataManager::RegisterMetaDataType(AampTsbMetaData::Type type, bool isTransient)
{
	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);

	bool ret = false;
	// Check if type already exists
	if (mMetaDataMap.find(type) != mMetaDataMap.end())
	{
		AAMPLOG_ERR("Metadata type %u already registered", (uint32_t)type);
	}
	else
	{
		// Add type with empty list
		mMetaDataMap[type] = std::list<std::shared_ptr<AampTsbMetaData>>();

		// Store the transience property for this type
		mTypeTransienceMap[type] = isTransient;

		AAMPLOG_INFO("Registered metadata type %u, isTransient=%d", (uint32_t)type, isTransient);
		ret = true;
	}
	return ret;
}

/**
 * @brief Check if a metadata type is registered and get its transience property.
 * @param type The metadata type to check.
 * @param[out] isTransient If type is registered, set to the transience property.
 * @param[out] metadataList If type is registered, set to the list of metadata for this type.
 * @return True if the metadata type is registered, false otherwise.
 */
bool AampTsbMetaDataManager::IsRegisteredType(AampTsbMetaData::Type type, bool& isTransient,
											std::list<std::shared_ptr<AampTsbMetaData>>** metadataList) const
{
	auto transIt = mTypeTransienceMap.find(type);
	auto dataIt = mMetaDataMap.find(type);
	bool registered = (transIt != mTypeTransienceMap.end() && dataIt != mMetaDataMap.end());

	if (registered)
	{
		isTransient = transIt->second;
		if (metadataList)
		{
			*metadataList = const_cast<std::list<std::shared_ptr<AampTsbMetaData>>*>(&dataIt->second);
		}
	}

	AAMPLOG_INFO("Metadata type %u%s registered", (uint32_t)type, registered ? "" : "not ");
	return registered;
}

/**
 * @brief Calculate the total count of all metadata items across all types.
 * @note This method assumes the caller holds mTsbMetaDataMutex.
 * @return Total count of metadata items.
 */
size_t AampTsbMetaDataManager::CalculateTotalCount() const
{
	size_t totalCount = 0;
	for (const auto& pair : mMetaDataMap)
	{
		totalCount += pair.second.size();
	}
	return totalCount;
}

bool AampTsbMetaDataManager::AddMetaData(const std::shared_ptr<AampTsbMetaData> &metaData)
{
	bool success = false;

	if (!metaData)
	{
		AAMPLOG_WARN("Attempt to add null metadata");
	}
	else
	{
		std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);

		AampTsbMetaData::Type type = metaData->GetType();
		bool isTransient = false;
		std::list<std::shared_ptr<AampTsbMetaData>>* metaDataList = nullptr;

		if (!IsRegisteredType(type, isTransient, &metaDataList))
		{
			AAMPLOG_ERR("Cannot add metadata of type %u - type not registered", (uint32_t)type);
		}
		else
		{
			// Check if this exact metadata object already exists in the list
			bool alreadyExists = false;
			for (const auto& existingMetaData : *metaDataList)
			{
				if (existingMetaData == metaData)
				{
					AAMPLOG_WARN("Attempt to add duplicate metadata object of type %u", (uint32_t)type);
					alreadyExists = true;
					break;
				}
			}

			if (!alreadyExists)
			{
				// Set incrementing order value to ensure deterministic ordering when positions are equal
				metaData->SetOrderAdded(mNextOrderAdded++);
				if (mNextOrderAdded == 0)
				{
					AAMPLOG_WARN("mNextOrderAdded overflow");
					mNextOrderAdded = 1;
				}

				metaData->Dump("Add ");

				// Custom comparison function for lower_bound considering both position and orderAdded
				auto compareMetaData = [](const std::shared_ptr<AampTsbMetaData>& a, const std::shared_ptr<AampTsbMetaData>& b)
				{
					bool ret;
					if (a->GetPosition() != b->GetPosition())
					{
						ret = a->GetPosition() < b->GetPosition();
					}
					else
					{
						ret = a->GetOrderAdded() < b->GetOrderAdded();
					}
					return ret;
				};

				// Find the correct insertion point using the custom comparison function
				auto it = std::lower_bound(metaDataList->begin(), metaDataList->end(), metaData, compareMetaData);
				metaDataList->insert(it, metaData);

				AAMPLOG_TRACE("Added metadata of type %u at position %" PRIu64 "ms with order %u, total count: %zu",
							 (uint32_t)type, metaData->GetPosition().milliseconds(), metaData->GetOrderAdded(), CalculateTotalCount());
				success = true;
			}
		}
	}
	return success;
}

bool AampTsbMetaDataManager::RemoveMetaData(const std::shared_ptr<AampTsbMetaData> &metaData)
{
	bool removed = false;

	if (!metaData)
	{
		AAMPLOG_WARN("Attempt to remove null metadata");
	}
	else
	{
		std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);
		AampTsbMetaData::Type type = metaData->GetType();
		bool isTransient = false;
		std::list<std::shared_ptr<AampTsbMetaData>>* list = nullptr;

		if (IsRegisteredType(type, isTransient, &list))
		{
			for (auto it = list->begin(); it != list->end(); )
			{
				if (*it == metaData)
				{
					(*it)->Dump("Erase ");
					it = list->erase(it);
					removed = true;
					break;
				}
				else
				{
					++it;
				}
			}
			if (removed)
			{
				AAMPLOG_TRACE("Removed metadata of type %u, remaining count: %zu",
							(uint32_t)type, CalculateTotalCount());
			}
		}
	}
	return removed;
}

/**
 * @brief Remove metadata at or before a specific position based on transience.
 * @param positionMs Position in milliseconds.
 * @return Number of items removed
 */
size_t AampTsbMetaDataManager::RemoveMetaData(const AampTime &position)
{
	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);

	AAMPLOG_INFO("Removing metadata at or before position = %" PRIu64 "ms", position.milliseconds());

	size_t totalRemoved = 0;

	for (auto& mapEntry : mMetaDataMap)
	{
		AampTsbMetaData::Type type = mapEntry.first;

		// Process only metadata of the matching type
		std::list<std::shared_ptr<AampTsbMetaData>>* list = nullptr;
		bool isTransient = false;
		if (!IsRegisteredType(type, isTransient, &list))
		{
			continue;
		}

		if (list->empty())
		{
			continue;
		}

		// For non-transient metadata: keep latest up to position, remove rest
		const size_t sizeBefore = list->size();
		auto latestIt = list->end();
		auto it = list->begin();

		// Find the latest metadata at or before position
		while (it != list->end())
		{
			if ((*it)->GetPosition() > position)
			{
				break;
			}
			latestIt = it;
			++it;
		}

		if (latestIt == list->end())
		{
			AAMPLOG_INFO("No metadata found at or before position %" PRIu64 "ms", position.milliseconds());
			continue;
		}

		// If transient we want to remove everything up to and including the metadata at "position"
		// If non-transient we want to remove everything up to the metadata at "position", or if
		// there is no metadata at "position" we want to remove everything up to the previous metadata
		// before "position"
		if (isTransient)
		{
			++latestIt;
		}

		const char* typeStr = isTransient ? "(transient) " : "(non-transient) ";
		std::string prefix = std::string("Erase ") + typeStr + std::to_string(position.milliseconds()) + "ms ";

		// If we found one, keep it and remove everything before it
		it = list->begin();
		// Remove up to latestIt
		while (it != latestIt)
		{
			(*it)->Dump(prefix);
			it = list->erase(it);
		}

		const size_t sizeAfter = list->size();
		size_t removedFromType = sizeBefore - sizeAfter;
		totalRemoved += removedFromType;

		AAMPLOG_INFO("Type %u: removed %zu items, remaining %zu items",
				(uint32_t)type, removedFromType, sizeAfter);
	}

	AAMPLOG_INFO("Metadata removal complete, removed %zu items, remaining: %zu",
				totalRemoved, CalculateTotalCount());

	return totalRemoved;
}

size_t AampTsbMetaDataManager::RemoveMetaDataIf(std::function<bool(const std::shared_ptr<AampTsbMetaData>&)> filter)
{
	size_t totalRemoved = 0;

	if (!filter)
	{
		AAMPLOG_WARN("Null filter provided to RemoveMetaDataIf");
	}
	else
	{
		std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);

		// Iterate through all types in the map
		for (auto& mapEntry : mMetaDataMap) {
			auto& list = mapEntry.second;
			size_t sizeBefore = list.size();

			for (auto it = list.begin(); it != list.end(); )
			{
				if (filter(*it))
				{
					(*it)->Dump("Erase ");
					it = list.erase(it);
				}
				else
				{
					++it;
				}
			}

			size_t removedFromType = sizeBefore - list.size();
			totalRemoved += removedFromType;
		}

		if (totalRemoved > 0)
		{
			AAMPLOG_TRACE("Removed %zu metadata items via filter, remaining count: %zu",
						 totalRemoved, CalculateTotalCount());
		}
	}
	return totalRemoved;
}

size_t AampTsbMetaDataManager::GetSize() const
{
	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);
	return CalculateTotalCount();
}

void AampTsbMetaDataManager::Dump()
{
	std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);
	size_t totalCount = CalculateTotalCount();

	AAMPLOG_INFO("Dumping TSB Metadata - Start (%zu items)", totalCount);

	for (const auto& pair : mMetaDataMap)
	{
		AAMPLOG_INFO("Metadata Type: %u, Count: %zu", (uint32_t)pair.first, pair.second.size());
		for (const auto& metaData : pair.second)
		{
			metaData->Dump("Dump ");
		}
	}

	AAMPLOG_INFO("Dumping TSB Metadata - End");
}

bool AampTsbMetaDataManager::ChangeMetaDataPosition(
	const std::list<std::shared_ptr<AampTsbMetaData>>& metaDataList, const AampTime& newPosition)
{
	 bool allUpdated = true;

	if (metaDataList.empty())
	{
		AAMPLOG_WARN("Empty metadata list provided to ChangeMetaDataPosition");
		allUpdated = false;
	}
	else
	{
		std::lock_guard<std::mutex> lock(mTsbMetaDataMutex);

		for (const auto& metaData : metaDataList)
		{
			if (!metaData)
			{
				AAMPLOG_WARN("Null metadata found in list");
				allUpdated = false;
				continue;
			}

			AampTsbMetaData::Type type = metaData->GetType();
			bool isTransient = false;
			std::list<std::shared_ptr<AampTsbMetaData>>* list = nullptr;

			// Verify the metadata type is registered
			if (!IsRegisteredType(type, isTransient, &list))
			{
				AAMPLOG_ERR("Metadata type %u not registered", (uint32_t)type);
				allUpdated = false;
				continue;
			}

			// Find the metadata in our list
			auto it = std::find(list->begin(), list->end(), metaData);
			if (it == list->end())
			{
				AAMPLOG_ERR("Metadata not found in manager");
				allUpdated = false;
				continue;
			}

			(*it)->Dump("Change Pos ");

			// Remove and reinsert to maintain ordering
			list->erase(it);
			metaData->SetPosition(newPosition);

			// Find new insertion point - considering both position and orderAdded
			auto rpos = list->rbegin();
			while (rpos != list->rend())
			{
				if ((*rpos)->GetPosition() < newPosition)
				{
					break;
				}
				if ((*rpos)->GetPosition() == newPosition &&
					(*rpos)->GetOrderAdded() <= metaData->GetOrderAdded())
				{
					break;
				}
				++rpos;
			}

			list->insert(rpos.base(), metaData);

			AAMPLOG_TRACE("Updated metadata of type %u to position %" PRIu64 "ms with order %u",
					(uint32_t)type, newPosition.milliseconds(), metaData->GetOrderAdded());
		}
	}
	return allUpdated;
}

