/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file jseventlistener.cpp
 * @brief Event Listener impl for PrivAAMPStruct_JS object
 */


#include "jseventlistener.h"
#include "jsevent.h"
#include "jsutils.h"
#include "vttCue.h"


/**
 * @class AAMP_Listener_PlaybackStateChanged
 * @brief Event listener impl for AAMP_EVENT_STATE_CHANGED event.
 */
class AAMP_Listener_PlaybackStateChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_PlaybackStateChanged Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_PlaybackStateChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		StateChangedEventPtr evt = std::dynamic_pointer_cast<StateChangedEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("state");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getState()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_ProgressUpdate
 * @brief Event listener impl for AAMP_EVENT_PROGRESS event.
 */
class AAMP_Listener_ProgressUpdate : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_ProgressUpdate Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_ProgressUpdate(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		ProgressEventPtr evt = std::dynamic_pointer_cast<ProgressEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("durationMiliseconds"); // FIXME
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("positionMiliseconds"); // FIXME
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("playbackSpeed");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getSpeed()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("startMiliseconds"); // FIXME
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getStart()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("endMiliseconds"); // FIXME
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getEnd()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("currentPTS");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPTS()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("videoBufferedMiliseconds");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getVideoBufferedDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("audioBufferedMiliseconds");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getAudioBufferedDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("timecode");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getSEITimeCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("liveLatency");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getLiveLatency()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("targetLatency");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getTargetLatency()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("profileBandwidth");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getProfileBandwidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("networkBandwidth");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getNetworkBandwidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("currentPlayRate");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getCurrentPlayRate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	

	}
};


/**
 * @class AAMP_Listener_SpeedChanged
 * @brief Event listener impl for AAMP_EVENT_SPEED_CHANGED event.
 */
class AAMP_Listener_SpeedChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_SpeedChanged Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_SpeedChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		SpeedChangedEventPtr evt = std::dynamic_pointer_cast<SpeedChangedEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("speed");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getRate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("reason");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, "unknown"), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_BufferingChanged
 * @brief Event listener impl for AAMP_EVENT_BUFFERING_CHANGED event.
 */
class AAMP_Listener_BufferingChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_BufferingChanged Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_BufferingChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		BufferingChangedEventPtr evt = std::dynamic_pointer_cast<BufferingChangedEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("buffering");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeBoolean(mContext, evt->buffering()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_PlaybackFailed
 * @brief Event listener impl for AAMP_EVENT_TUNE_FAILED event.
 */
class AAMP_Listener_PlaybackFailed : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_PlaybackFailed Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_PlaybackFailed(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		MediaErrorEventPtr evt = std::dynamic_pointer_cast<MediaErrorEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("code");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("subCode");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getSubCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getDescription().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("shouldRetry");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeBoolean(mContext, evt->shouldRetry()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		if(-1 != evt->getClass()) //Only send verbose error for secclient/secmanager DRM failures
		{
			prop = JSStringCreateWithUTF8CString("class");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getClass()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
			
			prop = JSStringCreateWithUTF8CString("reason");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getReason()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
			
			prop = JSStringCreateWithUTF8CString("businessStatus");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getBusinessStatus()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}
		if(!evt->getResponseData().empty())
		{
			prop = JSStringCreateWithUTF8CString("responseData");
			JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getResponseData().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

	}
};


/**
 * @class AAMP_Listener_MediaMetadata
 * @brief Event listener impl for AAMP_EVENT_MEDIA_METADATA event.
 */
class AAMP_Listener_MediaMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_MediaMetadata Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_MediaMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		MediaMetadataEventPtr evt = std::dynamic_pointer_cast<MediaMetadataEvent>(ev);

		JSStringRef prop;
		prop = JSStringCreateWithUTF8CString("durationMiliseconds"); // FIXME
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		int count = evt->getLanguagesCount();
		const std::vector<std::string> &langVect = evt->getLanguages();
		JSValueRef* array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			JSValueRef lang = aamp_CStringToJSValue(mContext, langVect[i].c_str());
			array[i] = lang;
		}
		JSValueRef propValue = JSObjectMakeArray(mContext, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("languages");
		JSObjectSetProperty(mContext, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		count = evt->getBitratesCount();
		const std::vector<long> &bitrateVect = evt->getBitrates();
		array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(mContext, bitrateVect[i]);
		}
		propValue = JSObjectMakeArray(mContext, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("bitrates");
		JSObjectSetProperty(mContext, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		count = evt->getSupportedSpeedCount();
		const std::vector<float> &speedVect = evt->getSupportedSpeeds();
		array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(mContext, speedVect[i]);
		}
		propValue = JSObjectMakeArray(mContext, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("playbackSpeeds");
		JSObjectSetProperty(mContext, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("programStartTime");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getProgramStartTime()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("producerReferenceClockOffset");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getProducerReferenceClockOffset()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getWidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getHeight()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("hasDrm");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeBoolean(mContext, evt->hasDrm()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("isLive");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeBoolean(mContext, evt->isLive()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("DRM");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getDrmType().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("url");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getUrl().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		//ratings
		if(!evt->getRatings().empty())
		{
			prop = JSStringCreateWithUTF8CString("ratings");
			JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getRatings().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//ssi
		if(evt->getSsi() >= 0 )
		{
			prop = JSStringCreateWithUTF8CString("ssi");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getSsi()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//framerate
		if(evt->getFrameRate() > 0 )
		{
			prop = JSStringCreateWithUTF8CString("framerate");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getFrameRate()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		if(eVIDEOSCAN_UNKNOWN != evt->getVideoScanType())
		{
			prop = JSStringCreateWithUTF8CString("progressive");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeBoolean(mContext, ((eVIDEOSCAN_PROGRESSIVE == evt->getVideoScanType())?true:false)), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}
		//aspect ratio
		if((0 != evt->getAspectRatioWidth()) && (0 != evt->getAspectRatioHeight()))
		{
			prop = JSStringCreateWithUTF8CString("aspectRatioWidth");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getAspectRatioWidth()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);

			prop = JSStringCreateWithUTF8CString("aspectRatioHeight");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getAspectRatioHeight()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//VideoCodec
		if(!evt->getVideoCodec().empty())
		{
			prop = JSStringCreateWithUTF8CString("videoCodec");
			JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getVideoCodec().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//HdrType
		if(!evt->getHdrType().empty())
		{
			prop = JSStringCreateWithUTF8CString("hdrType");
			JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getHdrType().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//AudioBitrate
		const std::vector<long> &audioBitrateVect = evt->getAudioBitrates();
		count = (int)audioBitrateVect.size();
		if(count > 0 )
		{
			array = new JSValueRef[count];
			for (int32_t i = 0; i < count; i++)
			{
				array[i] = JSValueMakeNumber(mContext, audioBitrateVect[i]);
			}
			propValue = JSObjectMakeArray(mContext, count, array, NULL);
			delete [] array;

			prop = JSStringCreateWithUTF8CString("audioBitrates");
			JSObjectSetProperty(mContext, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}


		//AudioCodec
		if(!evt->getAudioCodec().empty())
		{
			prop = JSStringCreateWithUTF8CString("audioCodec");
			JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAudioCodec().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//AudioMixType
		if(!evt->getAudioMixType().empty())
		{
			prop = JSStringCreateWithUTF8CString("audioMixType");
			JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAudioMixType().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//AtmosInfo
		prop = JSStringCreateWithUTF8CString("isAtmos");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeBoolean(mContext,evt->getAtmosInfo()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		//MediaFormat type
		prop = JSStringCreateWithUTF8CString("mediaFormat");
                JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getMediaFormat().c_str()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);
        
		//tsbdepth
		prop = JSStringCreateWithUTF8CString("tsbDepthMs");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getTsbDepth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

	}
};


/**
 * @class AAMP_Listener_SpeedsChanged
 * @brief Event listener impl for AAMP_EVENT_SPEEDS_CHANGED event.
 */
class AAMP_Listener_SpeedsChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_SpeedsChanged Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_SpeedsChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		SupportedSpeedsChangedEventPtr evt = std::dynamic_pointer_cast<SupportedSpeedsChangedEvent>(ev);

		int count = evt->getSupportedSpeedCount();
		const std::vector<float> &speedVect = evt->getSupportedSpeeds();
		JSValueRef* array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(mContext, speedVect[i]);
		}
		JSValueRef propValue = JSObjectMakeArray(mContext, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		JSStringRef prop = JSStringCreateWithUTF8CString("playbackSpeeds");
		JSObjectSetProperty(mContext, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_Listener_Seeked
 * @brief Event listener impl for AAMP_EVENT_SEEKED event.
 */
class AAMP_Listener_Seeked : public AAMP_JSEventListener
{
public:
	/**
	* @brief AAMP_Listener_Seeked Constructor
	* @param[in] obj aamp instance of PrivAAMPStruct_JS
	* @param[in] type event type
	* @param[in] jsCallback callback to be registered as listener
	*/
	AAMP_Listener_Seeked(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		SeekedEventPtr evt = std::dynamic_pointer_cast<SeekedEvent>(ev);
		JSStringRef prop = JSStringCreateWithUTF8CString("position");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_TuneProfiling
 * @brief Event listener impl for AAMP_EVENT_TUNE_PROFILING event.
 */
class AAMP_Listener_TuneProfiling : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_TuneProfiling Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_TuneProfiling(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		TuneProfilingEventPtr evt = std::dynamic_pointer_cast<TuneProfilingEvent>(ev);
                JSStringRef prop;
                const char* microData = evt->getProfilingData().c_str();

                LOG_TRACE("AAMP_Listener_TuneProfiling microData %s", microData);
                prop = JSStringCreateWithUTF8CString("microData");
                JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, microData), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);
	}

};

/**
 * @class AAMP_Listener_CCHandleAvailable
 * @brief Event listener impl for AAMP_EVENT_CC_HANDLE_RECEIVED, event.
 */
class AAMP_Listener_CCHandleAvailable : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_CCHandleAvailable Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_CCHandleAvailable(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		CCHandleEventPtr evt = std::dynamic_pointer_cast<CCHandleEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("decoderHandle");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getCCHandle()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_DRMMetadata
 * @brief Event listener impl for AAMP_EVENT_DRM_METADATA event.
 */
class AAMP_Listener_DRMMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_DRMMetadata Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_DRMMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		DrmMetaDataEventPtr evt = std::dynamic_pointer_cast<DrmMetaDataEvent>(ev);
		JSStringRef prop;

		int code = evt->getAccessStatusValue();
		const char* description = evt->getAccessStatus().c_str();
		const char* networkMetric = evt->getNetworkMetricData().c_str();

		LOG_WARN_EX("AAMP_Listener_DRMMetadata code %d Description %s", code, description);
		LOG_TRACE("AAMP_Listener_DRMMetadata NetworkMetric %s", networkMetric);
		prop = JSStringCreateWithUTF8CString("code");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, code), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, description), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("networkMetrics");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, networkMetric), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		const std::vector<std::string> &headerVec = evt->getHeaderResponses();
		if(!headerVec.empty())
		{
			int count = (int)headerVec.size();
			JSValueRef* array = new JSValueRef[count];
			for(int i = 0; i < count; i++)
			{
				JSValueRef respHeader = aamp_CStringToJSValue(mContext, headerVec[i].c_str());
				array[i] = respHeader;
			}
			JSValueRef propValue = JSObjectMakeArray(mContext, count, array, NULL);
			SAFE_DELETE_ARRAY(array);
			prop = JSStringCreateWithUTF8CString("headers");
			JSObjectSetProperty(mContext, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		const char *pBodyResponse = evt->getBodyResponse().c_str();
		if( NULL != pBodyResponse )
		{
			JSObjectRef bodyResponseObj = aamp_CreateBodyResponseJSObject(mContext, pBodyResponse);
			if( bodyResponseObj )
			{
				JSValueProtect(mContext, bodyResponseObj);
				prop = JSStringCreateWithUTF8CString("body");
				JSObjectSetProperty(mContext, jsEventObj, prop, bodyResponseObj, kJSPropertyAttributeReadOnly, NULL);
				JSStringRelease(prop);
				JSValueUnprotect(mContext, bodyResponseObj);	
			}
		}
	}

};


/**
 * @class AAMP_Listener_AnomalyReport
 * @brief Event listener impl for AAMP_EVENT_REPORT_ANOMALY event.
 */
class AAMP_Listener_AnomalyReport : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AnomalyReport Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AnomalyReport(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AnomalyReportEventPtr evt = std::dynamic_pointer_cast<AnomalyReportEvent>(ev);
		JSStringRef prop;

		int severity = evt->getSeverity();
		const char* description = evt->getMessage().c_str();

        	LOG_WARN_EX("AAMP_Listener_AnomalyReport severity %d Description %s", severity, description);
		prop = JSStringCreateWithUTF8CString("severity");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, severity), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, description), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_VTTCueData
 * @brief Event listener impl for AAMP_EVENT_WEBVTT_CUE_DATA event.
 */
class AAMP_Listener_VTTCueData : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_VTTCueData Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_VTTCueData(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		WebVttCueEventPtr evt = std::dynamic_pointer_cast<WebVttCueEvent>(ev);

		JSStringRef prop;
		VTTCue *cue = evt->getCueData();

		prop = JSStringCreateWithUTF8CString("start");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, cue->mStart), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("duration");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, cue->mDuration), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("text");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, cue->mText.c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_TimedMetadata
 * @brief Event listener impl for AAMP_EVENT_TIMED_METADATA event.
 */
class AAMP_Listener_TimedMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_TimedMetadata Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_TimedMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		TimedMetadataEventPtr evt = std::dynamic_pointer_cast<TimedMetadataEvent>(ev);

		JSObjectRef timedMetadata = aamp_CreateTimedMetadataJSObject(mContext, evt->getTime(), evt->getName().c_str(), evt->getContent().c_str(), evt->getId().c_str(), evt->getDuration());
		if (timedMetadata)
		{
			JSValueProtect(mContext, timedMetadata);
			JSStringRef prop = JSStringCreateWithUTF8CString("timedMetadata");
			JSObjectSetProperty(mContext, jsEventObj, prop, timedMetadata, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
			JSValueUnprotect(mContext, timedMetadata);
		}
	}
};

/**
 * @class AAMP_Listener_BulkTimedMetadata
 * @brief Event listener impl for BULK_TIMED_METADATA AAMP event
 */
class AAMP_Listener_BulkTimedMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_BulkTimedMetadata Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
        AAMP_Listener_BulkTimedMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
        {
        }


        /**
         * @brief Set JS event properties
         */
        void SetEventProperties(const AAMPEventPtr& ev,  JSObjectRef eventObj)
        {
		BulkTimedMetadataEventPtr evt = std::dynamic_pointer_cast<BulkTimedMetadataEvent>(ev);
		JSStringRef name = JSStringCreateWithUTF8CString("timedMetadatas");
		JSObjectSetProperty(mContext, eventObj, name, aamp_CStringToJSValue(mContext, evt->getContent().c_str()),  kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
        }
};


/**
 * @class AAMP_Listener_AdResolved
 *
 * @brief Event listener impl for AAMP_EVENT_AD_RESOLVED event
 */
class AAMP_Listener_AdResolved : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdResolved Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdResolved(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdResolvedEventPtr evt = std::dynamic_pointer_cast<AdResolvedEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("resolvedStatus");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeBoolean(mContext, evt->getResolveStatus()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementStartTime");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getStart()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementDuration");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("errorCode");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getErrorCode().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("errorDescription");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getErrorDescription().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdReservationStart
 *
 * @brief Event listener impl for AAMP_EVENT_AD_RESERVATION_START event
 */
class AAMP_Listener_AdReservationStart : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdReservationStart Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdReservationStart(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdReservationEventPtr evt = std::dynamic_pointer_cast<AdReservationEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adbreakId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAdBreakId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdReservationEnd
 *
 * @brief Event listener impl for AAMP_EVENT_AD_RESERVATION_END event
 */
class AAMP_Listener_AdReservationEnd : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdReservationEnd Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdReservationEnd(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdReservationEventPtr evt = std::dynamic_pointer_cast<AdReservationEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adbreakId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAdBreakId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("reason");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getReason().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdPlacementStart
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_START event
 */
class AAMP_Listener_AdPlacementStart : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdPlacementStart Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdPlacementStart(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdPlacementEnd
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_END event
 */
class AAMP_Listener_AdPlacementEnd : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdPlacementEnd Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdPlacementEnd(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdProgress
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_PROGRESS event
 */
class AAMP_Listener_AdProgress : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdProgress Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdProgress(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdPlacementError
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_ERROR event
 */
class AAMP_Listener_AdPlacementError : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdPlacementError Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdPlacementError(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("error");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getErrorCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_BitrateChanged
 * @brief Event listener impl for AAMP_EVENT_BITRATE_CHANGED event.
 */
class AAMP_Listener_BitrateChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_BitrateChanged Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_BitrateChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		BitrateChangeEventPtr evt = std::dynamic_pointer_cast<BitrateChangeEvent>(ev);
		JSStringRef name;
		name = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getTime()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("bitRate");
		JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getBitrate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(mContext, jsEventObj, name, aamp_CStringToJSValue(mContext, evt->getDescription().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getWidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getHeight()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("framerate");
		JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getFrameRate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("position");
		JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("cappedProfile");
		JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getCappedProfileStatus()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("displayWidth");
                JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getDisplayWidth()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("displayHeight");
                JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getDisplayHeight()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(name);

		if(eVIDEOSCAN_UNKNOWN != evt->getScanType())
		{
			name = JSStringCreateWithUTF8CString("progressive");
			JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeBoolean(mContext, ((eVIDEOSCAN_PROGRESSIVE == evt->getScanType())?true:false)), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}

		if((0 != evt->getAspectRatioWidth()) && (0 != evt->getAspectRatioHeight()))
		{
			name = JSStringCreateWithUTF8CString("aspectRatioWidth");
			JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getAspectRatioWidth()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);

			name = JSStringCreateWithUTF8CString("aspectRatioHeight");
			JSObjectSetProperty(mContext, jsEventObj, name, JSValueMakeNumber(mContext, evt->getAspectRatioHeight()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}

	}
};

/**
 * @class AAMP_Listener_Id3Metadata
 * @brief Event listener impl for AAMP_EVENT_ID3_METADATA event.
 */
class AAMP_Listener_Id3Metadata: public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_Id3Metadata Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_Id3Metadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		ID3MetadataEventPtr evt = std::dynamic_pointer_cast<ID3MetadataEvent>(ev);
		std::vector<uint8_t> data = evt->getMetadata();
		int len = evt->getMetadataSize();
		JSStringRef prop;

		JSValueRef* array = new JSValueRef[len];
		for (int32_t i = 0; i < len; i++)
		{
			array[i] = JSValueMakeNumber(mContext, data[i]);
		}
		prop = JSStringCreateWithUTF8CString("schemeIdUri");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getSchemeIdUri().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("value");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getValue().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("timeScale");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getTimeScale()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("presentationTime");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getPresentationTime()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("eventDuration");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getEventDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("id");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getId()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("timestampOffset");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getTimestampOffset()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("data");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSObjectMakeArray(mContext, len, array, NULL), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("length");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, len), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_Listener_Blocked
 * @brief Event listener impl for AAMP_EVENT_BLOCKED event.
 */
class AAMP_Listener_Blocked: public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_Blocked Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_Blocked(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		BlockedEventPtr evt = std::dynamic_pointer_cast<BlockedEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("reason");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getReason().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
		prop = JSStringCreateWithUTF8CString("locator");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getLocator().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_Listener_ContentGap
 * @brief Event listener impl for AAMP_EVENT_CONTENT_GAP event.
 */
class AAMP_Listener_ContentGap : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_ContentGap Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_ContentGap(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		ContentGapEventPtr evt = std::dynamic_pointer_cast<ContentGapEvent>(ev);
		JSStringRef prop;

		double time = evt->getTime();
		double durationMs = evt->getDuration();

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, std::round(time)), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		if (durationMs >= 0)
		{
			prop = JSStringCreateWithUTF8CString("duration");
			JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, (int)durationMs), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}
	}
};


/**
 * @class AAMP_Listener_HTTPResponseHeader
 * @brief Event listener impl for AAMP_EVENT_HTTP_RESPONSE_HEADER event.
 */
class AAMP_Listener_HTTPResponseHeader : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_HTTPResponseHeader Constructor
	 * @param[in] obj aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_HTTPResponseHeader(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		HTTPResponseHeaderEventPtr evt = std::dynamic_pointer_cast<HTTPResponseHeaderEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("header");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getHeader().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("response");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getResponse().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_Listener_WatermarkSessionUpdate
 * @brief Event listener impl for AAMP_EVENT_WATERMARK_SESSION_UPDATE event.
 */
class AAMP_Listener_WatermarkSessionUpdate: public AAMP_JSEventListener
{
public:
        /**
         * @brief AAMP_Listener_WatermarkSessionUpdate Constructor
         * @param[in] obj aamp instance of PrivAAMPStruct_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
        AAMP_Listener_WatermarkSessionUpdate(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
                : AAMP_JSEventListener(obj, type, jsCallback)
        {
        }

        /**
         * @brief Set JS event properties
         * @param[in] ev AAMP event object
         * @param[out] jsEventObj JS event object
         */
        void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
        {
                WatermarkSessionUpdateEventPtr evt = std::dynamic_pointer_cast<WatermarkSessionUpdateEvent>(ev);
                JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("sessionHandle");
                JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getSessionHandle()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("status");
                JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getStatus()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);

                prop = JSStringCreateWithUTF8CString("system");
                JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getSystem().c_str()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);
        }
};

/**
 * @class AAMP_JSListener_ContentProtectionData
 * @brief Event listener impl for (AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE) AAMP event
 */

class AAMP_Listener_ContentProtectionData : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_ContentProtectionData Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_ContentProtectionData(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		ContentProtectionDataEventPtr evt = std::dynamic_pointer_cast<ContentProtectionDataEvent>(ev);
		std::vector<uint8_t> keyId = evt->getKeyID();
		int len = (int)keyId.size();
		JSStringRef prop;
		JSValueRef* array = new JSValueRef[len];
		for (int32_t i = 0; i < len; i++)
		{
			array[i] = JSValueMakeNumber(mContext, keyId[i]);
		}

		prop = JSStringCreateWithUTF8CString("keyID");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSObjectMakeArray(mContext, len, array, NULL), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("streamType");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getStreamType().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};

/**
 *  @class AAMP_EVENT_MANIFEST_REFRESH_NOTIFY
 * @brief Event listener impl for AAMP_EVENT_MANIFEST_REFRESH_NOTIFY event.
 */

class AAMP_Listener_DashManifestRefreshNotify : public AAMP_JSEventListener
{
public:
    /**
     * @brief AAMP_EVENT_MANIFEST_REFRESH_NOTIFY Constructor
     * @param[in] type event type
     * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_DashManifestRefreshNotify(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
    {
    }

    /**
     * @brief Set properties to JS event object
     * @param[in] ev AAMP event object
     * @param[out] jsEventObj JS event object
     */
    void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
    {
        ManifestRefreshEventPtr evt = std::dynamic_pointer_cast<ManifestRefreshEvent>(ev);
		JSStringRef prop;
		
		prop = JSStringCreateWithUTF8CString("manifestDuration");
        JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getManifestDuration()), kJSPropertyAttributeReadOnly, NULL);
        JSStringRelease(prop);
		
		prop = JSStringCreateWithUTF8CString("manifestPublishedTime");
        JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getManifestPublishedTime()), kJSPropertyAttributeReadOnly, NULL);
        JSStringRelease(prop);
		
		prop = JSStringCreateWithUTF8CString("noOfPeriods");
        JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getNoOfPeriods()), kJSPropertyAttributeReadOnly, NULL);
        JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("manifestType");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->getManifestType().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};

/**
 *  @class AAMP_JSListener_TimeMetricData
 * @brief Event listener impl for AAMP_Listener_TuneMetricData event.
 */

class AAMP_Listener_TuneMetricData : public AAMP_JSEventListener
{
public:
    /**
     * @brief AAMP_EVENT_MANIFEST_REFRESH_NOTIFY Constructor
     * @param[in] type event type
     * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_TuneMetricData(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		TuneTimeMetricsEventPtr evt = std::dynamic_pointer_cast<TuneTimeMetricsEvent>(ev);
		JSStringRef prop;
		const char* tuneMetricData = evt->getTuneMetricsData().c_str();

		prop = JSStringCreateWithUTF8CString("tuneMetricsData");
		LOG_TRACE("AAMP_Listener_TuneMetricData Tunemetric data %s", tuneMetricData);
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, tuneMetricData), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};

class AAMP_Listener_MonitorAVStatus : public AAMP_JSEventListener
{
public:
	AAMP_Listener_MonitorAVStatus(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		MonitorAVStatusEventPtr evt = std::dynamic_pointer_cast<MonitorAVStatusEvent>(ev);
		JSStringRef prop;
		const char* monitorAVStatus = evt->getMonitorAVStatus().c_str();

		prop = JSStringCreateWithUTF8CString("currentState");
		LOG_TRACE("AAMP_Listener_MonitorAVStatus MonitorAVStatus data %s", monitorAVStatus);
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, monitorAVStatus), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("videoPosMs");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getVideoPositionMS()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("audioPosMs");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getAudioPositionMS()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("timeInStateMs");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getTimeInStateMS()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("droppedFrames");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, evt->getDroppedFrames()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/// -----------------------------------------------------------------------------------------

class AAMP_Listener_VodAdBreakOpportunity : public AAMP_JSEventListener
{
public:
	AAMP_Listener_VodAdBreakOpportunity(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	void SetEventProperties(const AAMPEventPtr& evt, JSObjectRef jsEventObj)
	{
		VodAdBreakOpportunityEventPtr e = std::dynamic_pointer_cast<VodAdBreakOpportunityEvent>(evt);
		if (!e) return;

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("breakId");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, e->getBreakId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("insertionPointSec");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, e->getInsertionPointSec()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("breakDurationSec");
		JSObjectSetProperty(mContext, jsEventObj, prop, JSValueMakeNumber(mContext, e->getBreakDurationSec()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("breakType");
		JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, e->getBreakType().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/// -----------------------------------------------------------------------------------------

class AAMP_Listener_DefaultEvent : public AAMP_JSEventListener
{
public:
    /**
     * @brief AAMP_EVENT_MANIFEST_REFRESH_NOTIFY Constructor
     * @param[in] type event type
     * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_DefaultEvent(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] evt AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& evt, JSObjectRef jsEventObj)
	{
	}

};

/// -----------------------------------------------------------------------------------------

/**
 * @brief AAMP_JSEventListener Constructor
 */
AAMP_JSEventListener::AAMP_JSEventListener(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
	: p_obj(obj)
	, p_type(type)
	, p_jsCallback(jsCallback)
	, mContext(obj != NULL ? obj->_ctx : NULL)
	, mDispatchCount(0)
	, mDetached(false)
	, mCallbackProtected(jsCallback != NULL)
	, mProtectedCallback(jsCallback)
{
	if (p_jsCallback != NULL)
	{
		JSValueProtect(mContext, p_jsCallback);
	}
}

/**
 * @brief AAMP_JSEventListener Destructor
 */
AAMP_JSEventListener::~AAMP_JSEventListener()
{
	std::lock_guard<std::mutex> lock(mStateMutex);
	if (mCallbackProtected)
	{
		JSValueUnprotect(mContext, mProtectedCallback);
		mCallbackProtected = false;
	}
}

/**
 * @brief Callback invoked for dispatching event
 */
void AAMP_JSEventListener::Event(const AAMPEventPtr& e)
{
	AAMPEventType evtType = e->getType();

	if (evtType < 0 || evtType >= AAMP_MAX_NUM_EVENTS)
	{
		return;
	}

	JSGlobalContextRef ctx = mContext;
	JSObjectRef callback = NULL;
	JSObjectRef promiseCallback = NULL;

	// Register the dispatch so teardown keeps the callback protected until it completes.
	// No JavaScriptCore allocation may happen under mStateMutex: it can run GC, and a
	// finalized player re-enters RemoveAllEventListener(), which takes this lock.
	{
		std::lock_guard<std::mutex> lock(mStateMutex);
		if (mDetached || p_obj == NULL)
		{
			return;
		}
		++mDispatchCount;
		callback = p_jsCallback;
	}

	// Event construction uses only mContext, never p_obj, as the owner may be finalized
	// by GC during these allocations.
	JSObjectRef event = createNewAAMPJSEvent(ctx, aampPlayer_getNameFromEventType(evtType), false, false);
	if (event == NULL)
	{
		FinishEventDispatch();
		return;
	}
	JSValueProtect(ctx, event);
	AAMP_JSEventListener::SetEventProperties(e, event);
	SetEventProperties(e, event);

	// Take the single-use promise from the owner, protecting it ourselves before the
	// owner releases its protection, so it stays alive after a callback releases the JS player.
	if (p_type == AAMP_EVENT_AD_RESOLVED && callback == NULL)
	{
		AdResolvedEventPtr evt = std::dynamic_pointer_cast<AdResolvedEvent>(e);
		std::string adId = evt->getAdId();
		std::lock_guard<std::mutex> lock(mStateMutex);
		if (!mDetached && p_obj != NULL)
		{
			promiseCallback = p_obj->getCallbackForAdId(adId);
			if (promiseCallback != NULL)
			{
				JSValueProtect(ctx, promiseCallback);
				p_obj->removeCallbackForAdId(adId);
			}
		}
	}

	if (p_type == AAMP_EVENT_AD_RESOLVED && callback == NULL)
	{
		if (promiseCallback != NULL)
		{
			aamp_dispatchEventToJS(ctx, promiseCallback, event);
			JSValueUnprotect(ctx, promiseCallback);
		}
		else
		{
			LOG_ERROR_EX("No promise callback registered ctx=%p, jsCallback=%p", ctx, promiseCallback);
		}
	}
	else if (callback != NULL)
	{
		aamp_dispatchEventToJS(ctx, callback, event);
	}
	else
	{
		LOG_WARN_EX("Callback registered is (%p) for event=%d", callback, p_type);
	}

	JSValueUnprotect(ctx, event);
	FinishEventDispatch();
}

void AAMP_JSEventListener::FinishEventDispatch()
{
	std::lock_guard<std::mutex> lock(mStateMutex);
	if (mDispatchCount > 0)
	{
		--mDispatchCount;
	}
	if (mDispatchCount == 0 && mDetached && mCallbackProtected)
	{
		JSValueUnprotect(mContext, mProtectedCallback);
		mCallbackProtected = false;
	}
}

void AAMP_JSEventListener::SetEventProperties(const AAMPEventPtr& evt, JSObjectRef jsEventObj)
{
	JSStringRef prop;

	prop = JSStringCreateWithUTF8CString("sessionId");
	JSObjectSetProperty(mContext, jsEventObj, prop, aamp_CStringToJSValue(mContext, evt->GetSessionId().c_str()), kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(prop);
}

/**
 * @brief Adds a JS function as listener for a particular event
 */
void AAMP_JSEventListener::AddEventListener(PrivAAMPStruct_JS* obj, AAMPEventType type, JSObjectRef jsCallback)
{
	LOG_TRACE("(%p, %d, %p)", obj, type, jsCallback);

    // Check for duplicate: see if jsCallback is already registered for this event type
	if (obj->_listeners.count(type) > 0)
	{
		auto range = obj->_listeners.equal_range(type);
		for (auto iter = range.first; iter != range.second; ++iter)
		{
			auto listener = std::static_pointer_cast<AAMP_JSEventListener>(iter->second);
			std::lock_guard<std::mutex> lock(listener->mStateMutex);
			if (listener->p_jsCallback == jsCallback)
			{
				// Listener already registered for this type and callback, ignore registration
				LOG_WARN_EX("Duplicate event listener registration ignored for type %d and callback %p", type, jsCallback);
				return;
			}
		}
	}
	std::shared_ptr<AAMP_JSEventListener> pListener = NULL;

	switch(type)
	{
		case AAMP_EVENT_STATE_CHANGED:
			pListener = std::make_shared<AAMP_Listener_PlaybackStateChanged>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_PROGRESS:
			pListener = std::make_shared<AAMP_Listener_ProgressUpdate>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_SPEED_CHANGED:
			pListener = std::make_shared<AAMP_Listener_SpeedChanged>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BUFFERING_CHANGED:
			pListener = std::make_shared<AAMP_Listener_BufferingChanged>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_TUNE_FAILED:
			pListener = std::make_shared<AAMP_Listener_PlaybackFailed>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_MEDIA_METADATA:
			pListener = std::make_shared<AAMP_Listener_MediaMetadata>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_SPEEDS_CHANGED:
			pListener = std::make_shared<AAMP_Listener_SpeedsChanged>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_SEEKED:
			pListener = std::make_shared<AAMP_Listener_Seeked>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_TUNE_PROFILING:
			pListener = std::make_shared<AAMP_Listener_TuneProfiling>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_CC_HANDLE_RECEIVED:
			pListener = std::make_shared<AAMP_Listener_CCHandleAvailable>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_DRM_METADATA:
			pListener = std::make_shared<AAMP_Listener_DRMMetadata>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_REPORT_ANOMALY:
			pListener = std::make_shared<AAMP_Listener_AnomalyReport>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_WEBVTT_CUE_DATA:
			pListener = std::make_shared<AAMP_Listener_VTTCueData>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BULK_TIMED_METADATA:
			pListener = std::make_shared<AAMP_Listener_BulkTimedMetadata>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_TIMED_METADATA:
			pListener = std::make_shared<AAMP_Listener_TimedMetadata>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_HTTP_RESPONSE_HEADER:
			pListener = std::make_shared<AAMP_Listener_HTTPResponseHeader>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BITRATE_CHANGED:
			pListener = std::make_shared<AAMP_Listener_BitrateChanged>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_RESOLVED:
			pListener = std::make_shared<AAMP_Listener_AdResolved>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_RESERVATION_START:
			pListener = std::make_shared<AAMP_Listener_AdReservationStart>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_RESERVATION_END:
			pListener = std::make_shared<AAMP_Listener_AdReservationEnd>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_START:
			pListener = std::make_shared<AAMP_Listener_AdPlacementStart>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_END:
			pListener = std::make_shared<AAMP_Listener_AdPlacementEnd>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_PROGRESS:
			pListener = std::make_shared<AAMP_Listener_AdProgress>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_ERROR:
			pListener = std::make_shared<AAMP_Listener_AdPlacementError>(obj, type, jsCallback);				break;
		case AAMP_EVENT_ID3_METADATA:
			pListener = std::make_shared<AAMP_Listener_Id3Metadata>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BLOCKED:
			pListener = std::make_shared<AAMP_Listener_Blocked>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_CONTENT_GAP:
			pListener = std::make_shared<AAMP_Listener_ContentGap>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_WATERMARK_SESSION_UPDATE:
			pListener = std::make_shared<AAMP_Listener_WatermarkSessionUpdate>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE:
			pListener = std::make_shared<AAMP_Listener_ContentProtectionData>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_MANIFEST_REFRESH_NOTIFY:
			pListener = std::make_shared<AAMP_Listener_DashManifestRefreshNotify>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_TUNE_TIME_METRICS:
			pListener = std::make_shared<AAMP_Listener_TuneMetricData>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_MONITORAV_STATUS:
			pListener = std::make_shared<AAMP_Listener_MonitorAVStatus>(obj, type, jsCallback);
			break;
		case AAMP_EVENT_VOD_ADBREAK_OPPORTUNITY:
			pListener = std::make_shared<AAMP_Listener_VodAdBreakOpportunity>(obj, type, jsCallback);
			break;
		// Following events are not having payload and hence falls under default case
		// AAMP_EVENT_EOS, AAMP_EVENT_TUNED, AAMP_EVENT_ENTERING_LIVE,
		// AAMP_EVENT_AUDIO_TRACKS_CHANGED, AAMP_EVENT_TEXT_TRACKS_CHANGED, AAMP_EVENT_NEED_MANIFEST_DATA
		default:
			// pListener = new AAMP_JSEventListener(obj, type, jsCallback);
			pListener = std::make_shared<AAMP_Listener_DefaultEvent>(obj, type, jsCallback);
			break;
	}

	if (obj->_aamp != NULL)
	{
		obj->_aamp->AddEventListener(type, pListener);
	}

	obj->_listeners.insert({type,(pListener)});
}


/**
 * @brief Removes a JS listener for a particular event
 */
void AAMP_JSEventListener::RemoveEventListener(PrivAAMPStruct_JS* obj, AAMPEventType type, JSObjectRef jsCallback)
{
        LOG_TRACE("(%p, %d, %p)", obj, type, jsCallback);
	if (obj->_listeners.count(type) > 0)
	{

		typedef std::multimap<AAMPEventType, std::shared_ptr<void>>::iterator listenerIter_t;
		std::pair<listenerIter_t, listenerIter_t> range = obj->_listeners.equal_range(type);
		for(listenerIter_t iter = range.first; iter != range.second; )
		{
			auto listener = std::static_pointer_cast<AAMP_JSEventListener>((iter->second));
			std::lock_guard<std::mutex> lock(listener->mStateMutex);
			if (listener->p_jsCallback == jsCallback)
			{
				if (obj->_aamp != NULL)
				{
					obj->_aamp->RemoveEventListener(iter->first, listener);
				}
				listener->mDetached = true;
				listener->p_obj = NULL;
				if (listener->mDispatchCount == 0 && listener->mCallbackProtected)
				{
					JSValueUnprotect(listener->mContext, listener->mProtectedCallback);
					listener->mCallbackProtected = false;
				}
				listener->p_jsCallback = NULL;

				iter = obj->_listeners.erase(iter);
			}
			else
			{
				iter++;
			}
		}
	}
}


/**
 * @brief Remove all JS listeners registered
 */
void AAMP_JSEventListener::RemoveAllEventListener(PrivAAMPStruct_JS * obj)
{
	
        LOG_TRACE("obj(%p) listeners remaining(%d)", obj, obj->_listeners.size());

	for (auto listenerIter = obj->_listeners.begin(); listenerIter != obj->_listeners.end();)
	{
		auto listener = std::static_pointer_cast<AAMP_JSEventListener>((listenerIter->second));
		if (obj->_aamp != NULL)
		{
			obj->_aamp->RemoveEventListener(listenerIter->first, listener);
		}

		{
			std::lock_guard<std::mutex> lock(listener->mStateMutex);
			listener->mDetached = true;
			listener->p_obj = NULL;
			if (listener->mDispatchCount == 0 && listener->mCallbackProtected)
			{
				JSValueUnprotect(listener->mContext, listener->mProtectedCallback);
				listener->mCallbackProtected = false;
			}
			listener->p_jsCallback = NULL;
		}

		listenerIter = obj->_listeners.erase(listenerIter);
	}

	obj->_listeners.clear();

}
