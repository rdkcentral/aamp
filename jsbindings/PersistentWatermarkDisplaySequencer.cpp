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
#include "PersistentWatermarkDisplaySequencer.h"


PersistentWatermark::DisplaySequencer::DisplaySequencer():mCurrentLayerID(0), mMutex(), mThunderAccess{"org.rdk.Watermark.1"}
{
	if(mThunderAccess.ActivatePlugin())
	{
		LOG_TRACE("activated");
	}
	else
	{
		LOG_ERROR_EX("PersistentWatermark:could not activate.");
	}
}

void PersistentWatermark::DisplaySequencer::deleteLayer()
{
	if(mCurrentLayerID)
	{
		LOG_TRACE("delete watermark");
		JsonObject param, result;
		param["id"] = mCurrentLayerID;
		bool success = mThunderAccess.InvokeJSONRPC("deleteWatermark", param, result);
		if(success && result["success"].Boolean())
		{
			LOG_TRACE("Watermark deleted");
			mCurrentLayerID = 0;
		}
		else
		{
			LOG_ERROR_EX("PersistentWatermark:failed to delete");
		}
	}
}

void PersistentWatermark::DisplaySequencer::SendEvent(EventHandler::eventType event, std::string msg)
{
	EventHandler::getInstance().Send(event, msg);
}

PersistentWatermark::DisplaySequencer::~DisplaySequencer()
{
	deleteLayer();
}

static int nextLayerID()
{
	static int nextLayer =100;
	if(nextLayer==__INT_MAX__)
	{
		nextLayer = 1;
	}
	else
	{
		nextLayer++;
	}
	LOG_TRACE("nextLayer ID enter key=%d,", nextLayer);
	return nextLayer;
}

void PersistentWatermark::DisplaySequencer::Show(PersistentWatermark::Storage& storage)
{
	if(!storage.valid())
	{
		SendEvent(EventHandler::eventType::ShowFailed, "Stored image is invalid");
		return;
	}
	std::lock_guard<std::mutex>lock(mMutex);

	if(mCurrentLayerID == 0)
	{
		mCurrentLayerID  = nextLayerID();
		{
			LOG_TRACE("Watermark create");
			JsonObject param, result;
			param["id"] = mCurrentLayerID;
			param["zorder"] = 1;
			bool success = mThunderAccess.InvokeJSONRPC("createWatermark", param, result);
			if(success && result["success"].Boolean())
			{
				LOG_TRACE("Watermark Create successful");
			}
			else
			{
				SendEvent(EventHandler::eventType::ShowFailed, "createWatermark failed");
				LOG_TRACE("Watermark exit -createWatermark failed");
				mCurrentLayerID = 0;
				return;
			}
		}
	}

	{
		LOG_TRACE("Watermark show");
		JsonObject param, result;
		param["show"] = true;
		bool success = mThunderAccess.InvokeJSONRPC("showWatermark", param, result);
		if(success && result["success"].Boolean())
		{
			LOG_TRACE("Watermark Show successful");
		}
		else
		{
			SendEvent(EventHandler::eventType::ShowFailed, " showWatermark, failed");
			LOG_TRACE("Watermark exit - showWatermark, failed");
			return;
		}
	}

	{
		if(!storage.valid())
		{
			SendEvent(EventHandler::eventType::ShowFailed, "Stored image is invalid");
			return;
		}

		/* Calling show() & update() concurrently could generate a failure here
			* (e.g. update() is executed between getKey() and getSize()
			* or the shared memory is deleted before the plugin connects to it)
			* This would have a low consequence as a failure event should be generated.*/
		int ImageKey = storage.getKey();
		int ImageSize= storage.getSize();
		LOG_TRACE("Watermark update key=%d, size=%d", ImageKey, ImageSize);
		JsonObject param, result;
		param["id"] = mCurrentLayerID;
		param["key"] = ImageKey;
		param["size"] = ImageSize;
		bool success = mThunderAccess.InvokeJSONRPC("updateWatermark", param, result);
		if(success && result["success"].Boolean())
		{
			LOG_TRACE("Watermark Update successful");
		}
		else
		{
			SendEvent(EventHandler::eventType::ShowFailed, "updateWatermark failed");
			LOG_TRACE("Watermark exit - updateWatermark failed");
			return;
		}

	}
	SendEvent(EventHandler::eventType::ShowSuccess);
	LOG_TRACE("Watermark exit - success");
}

void PersistentWatermark::DisplaySequencer::Hide()
{
	LOG_TRACE("Watermark enter");
	std::lock_guard<std::mutex>lock(mMutex);
	LOG_TRACE("Watermark hide");
	deleteLayer();

	JsonObject param, result;
	param["show"] = false;
	bool success = mThunderAccess.InvokeJSONRPC("showWatermark", param, result);

	if(success && result["success"].Boolean())
	{
		SendEvent(EventHandler::eventType::HideSuccess);
	}
	else
	{
		SendEvent(EventHandler::eventType::HideFailed);
	}
	LOG_TRACE("Watermark exit");
}
#endif
