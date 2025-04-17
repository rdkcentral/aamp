/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#ifdef USE_WATERMARK_JSBINDINGS
#include "PersistentWatermarkPluginAccess.h"

PersistentWatermark::PluginAccess::PluginAccess(): mThunderAccess(PlayerThunderAccessPlugin::WATERMARK)
{
	if(mThunderAccess.ActivatePlugin())
	{
		LOG_TRACE("PersistentWatermark:Thunder activated");
	}
	else
	{
		LOG_ERROR_EX("PersistentWatermark:could not activate Thunder.");
	}
}

bool PersistentWatermark::PluginAccess::DeleteWatermark(int layerID)
{
	return mThunderAccess.DeleteWatermark(layerID);
}

bool PersistentWatermark::PluginAccess::CreateWatermark(int layerID)
{
	return mThunderAccess.CreateWatermark(layerID);
}

bool PersistentWatermark::PluginAccess::ShowWatermark(int opacity)
{
	return mThunderAccess.ShowWatermark(opacity);
}

bool PersistentWatermark::PluginAccess::HideWatermark()
{
	return mThunderAccess.HideWatermark();
}

bool PersistentWatermark::PluginAccess::UpdateWatermark(int layerID, int sharedMemoryKey, int size)
{
	return mThunderAccess.UpdateWatermark(layerID, sharedMemoryKey, size);
}

std::string PersistentWatermark::PluginAccess::GetMetaDataWatermark()
{
	return mThunderAccess.GetMetaDataWatermark();
}

bool PersistentWatermark::PluginAccess::PersistentStoreSaveWatermark(const char* base64Image, std::string metaData)
{
	return mThunderAccess.PersistentStoreSaveWatermark(base64Image, metaData);
}

bool PersistentWatermark::PluginAccess::PersistentStoreLoadWatermark(int layerID)
{
    return mThunderAccess.PersistentStoreLoadWatermark(layerID);
}

PersistentWatermark::PluginAccess& PersistentWatermark::PluginAccess::get()
{
	static PluginAccess instance;
	return instance;
}
#endif // USE_WATERMARK_JSBINDINGS

