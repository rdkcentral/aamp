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
#include  "PersistentWatermarkPluginAccess.h"


PersistentWatermark::DisplaySequencer::DisplaySequencer():mCurrentLayerID(0)
{
	//empty
}

void PersistentWatermark::DisplaySequencer::deleteLayer()
{
	if(mCurrentLayerID)
	{
		LOG_TRACE("delete watermark");
		if(PluginAccess::get().DeleteWatermark(mCurrentLayerID))
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

void PersistentWatermark::DisplaySequencer::Show(PersistentWatermark::Storage& storage, int opacity)
{
	if(mCurrentLayerID == 0)
	{
		mCurrentLayerID  = nextLayerID();
		{
			LOG_TRACE("PersistentWatermark::DisplaySequencer::Show() Watermark create");
			if(PluginAccess::get().CreateWatermark(mCurrentLayerID))
			{
				LOG_TRACE("Watermark Create successful");
			}
			else
			{
				SendEvent(EventHandler::eventType::ShowFailed, "createWatermark failed");
				LOG_WARN_EX("PersistentWatermark::DisplaySequencer::Show() exit -createWatermark failed");
				mCurrentLayerID = 0;
				return;
			}
		}
	}

	{
		LOG_TRACE("Watermark show");
		if(PluginAccess::get().ShowWatermark(opacity))
		{
			LOG_TRACE("PersistentWatermark::DisplaySequencer::Show() Watermark Show successful");
		}
		else
		{
			SendEvent(EventHandler::eventType::ShowFailed, " showWatermark, failed");
			LOG_WARN_EX("PersistentWatermark::DisplaySequencer::Show()  exit - showWatermark, failed");
			return;
		}
	}


	if(storage.UpdatePlugin(mCurrentLayerID))
	{
		SendEvent(EventHandler::eventType::ShowSuccess);
		LOG_TRACE("PersistentWatermark::DisplaySequencer::Show() exit - success");
	}
	else
	{
		SendEvent(EventHandler::eventType::ShowFailed, "updateWatermark failed");
		LOG_WARN_EX("PersistentWatermark::DisplaySequencer::Show() updateWatermark failed");
		return;
	}
}

void PersistentWatermark::DisplaySequencer::Hide()
{
	LOG_TRACE("Watermark hide");
	deleteLayer();

	if(PluginAccess::get().HideWatermark())
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
