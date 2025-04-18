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

/**
 * @file FakeTsbMetaDataManager.cpp
 * @brief Fake implementation of TSB metadata manager for unit testing
 */

#include "MockTsbMetaDataManager.h"

MockAampTsbMetaDataManager* g_mockAampTsbMetaDataManager = nullptr;

// Add constructor and destructor implementations
AampTsbMetaDataManager::AampTsbMetaDataManager()
{
}

AampTsbMetaDataManager::~AampTsbMetaDataManager()
{
}

void AampTsbMetaDataManager::Initialize()
{
	if (g_mockAampTsbMetaDataManager)
	{
		g_mockAampTsbMetaDataManager->Initialize();
	}
}

void AampTsbMetaDataManager::Cleanup()
{
	if (g_mockAampTsbMetaDataManager)
	{
		g_mockAampTsbMetaDataManager->Cleanup();
	}
}

bool AampTsbMetaDataManager::RegisterMetaDataType(AampTsbMetaData::Type type, bool isTransient)
{
	bool result = false;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->RegisterMetaDataType(type, isTransient);
	}
	return result;
}

bool AampTsbMetaDataManager::IsRegisteredType(AampTsbMetaData::Type type, bool& isTransient,
	std::list<std::shared_ptr<AampTsbMetaData>>** metadataList) const
{
	bool result = false;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->IsRegisteredType(type, isTransient, metadataList);
	}
	return result;
}

bool AampTsbMetaDataManager::AddMetaData(const std::shared_ptr<AampTsbMetaData> &metaData)
{
	bool result = false;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->AddMetaData(metaData);
	}
	return result;
}

bool AampTsbMetaDataManager::RemoveMetaData(const std::shared_ptr<AampTsbMetaData> &metaData)
{
	bool result = false;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->RemoveMetaData(metaData);
	}
	return result;
}

size_t AampTsbMetaDataManager::RemoveMetaData(const AampTime& position)
{
	size_t result = 0;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->RemoveMetaData(position);
	}
	return result;
}

size_t AampTsbMetaDataManager::RemoveMetaDataIf(std::function<bool(const std::shared_ptr<AampTsbMetaData>&)> filter)
{
	size_t result = 0;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->RemoveMetaDataIf(filter);
	}
	return result;
}

size_t AampTsbMetaDataManager::GetSize() const
{
	size_t result = 0;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->GetSize();
	}
	return result;
}

void AampTsbMetaDataManager::Dump()
{
	if (g_mockAampTsbMetaDataManager)
	{
		g_mockAampTsbMetaDataManager->Dump();
	}
}

bool AampTsbMetaDataManager::ChangeMetaDataPosition(const std::list<std::shared_ptr<AampTsbMetaData>>& metaDataList, const AampTime& newPosition)
{
	bool result = false;
	if (g_mockAampTsbMetaDataManager)
	{
		result = g_mockAampTsbMetaDataManager->ChangeMetaDataPosition(metaDataList, newPosition);
	}
	return result;
}


