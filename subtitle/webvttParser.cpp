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
 * @file webvttParser.cpp
 *
 * @brief Parser impl for WebVTT subtitle fragments
 *
 */

#include <string.h>
#include <assert.h>
#include <cctype>
#include <algorithm>
#include "webvttParser.h"
#include "AampLogManager.h"
#include "AampUtils.h"

//Macros
#define CHAR_CARRIAGE_RETURN    '\r'
#define CHAR_LINE_FEED          '\n'
#define CHAR_SPACE              ' '

#define VTT_QUEUE_TIMER_INTERVAL 250 //milliseconds

/// Variable initialization for supported cue line alignment values
std::vector<std::string> allowedCueLineAligns = { "start", "center", "end" };

/// Variable initialization for supported cue position alignment values
std::vector<std::string> allowedCuePosAligns = { "line-left", "center", "line-right" };

/// Variable initialization for supported cue text alignment values
std::vector<std::string> allowedCueTextAligns = { "start", "center", "end", "left", "right" };


/***************************************************************************
* @fn parsePercentageValueSetting
* @brief Function to parse the value in percentage format
* 
* @param settingValue[in] value to be parsed
* @return char* pointer to the parsed value
***************************************************************************/
static char * parsePercentageValueSetting(char *settingValue)
{
	char *ret = NULL;
	char *percentageSym = strchr(settingValue, '%');
	if ((std::isdigit( static_cast<unsigned char>(settingValue[0]) ) != 0) && (percentageSym != NULL))
	{
		*percentageSym = '\0';
		ret = settingValue;
	}
	return ret;
}


/***************************************************************************
* @fn findWebVTTLineBreak
* @brief Function to extract a line from a VTT fragment
* 
* @param buffer[in] VTT data to extract from
* @return char* pointer to extracted line
***************************************************************************/
static char * findWebVTTLineBreak(char *buffer)
{
	//VTT has CR and LF as line terminators or both
	char *lineBreak = strpbrk(buffer, "\r\n");
	char *next = NULL;
	if (lineBreak)
	{
		next = lineBreak + 1;
		//For CR, LF pair cases
		if (*lineBreak == CHAR_CARRIAGE_RETURN && *next == CHAR_LINE_FEED)
		{
			next += 1;
		}
		*lineBreak = '\0';
	}
	return next;
}

/***************************************************************************
 * @fn SendVttCueToExt
 * @brief Timer's callback to send WebVTT cues to external app
 *
 * @param[in] user_data pointer to WebVTTParser instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
***************************************************************************/
static gboolean SendVttCueToExt(gpointer user_data)
{
	WebVTTParser *parser = (WebVTTParser *) user_data;
	parser->sendCueData();
	return G_SOURCE_CONTINUE;
}


/***************************************************************************
* @fn WebVTTParser
* @brief Constructor function
* 
* @param aamp[in] PrivateInstanceAAMP pointer
* @param type[in] VTT data type
* @return void
***************************************************************************/
WebVTTParser::WebVTTParser(SubtitleMimeType type, int width, int height) : SubtitleParser(type, width, height),
	mStartPTS(0), mCurrentPos(0), mStartPos(0), mPtsOffset(0),
	mReset(true), mVttQueue(), mVttQueueIdleTaskId(0), mVttQueueMutex(), lastCue(),
	mProgressOffset(0)
{
	lastCue = { 0, 0 };
}


/***************************************************************************
* @fn ~WebVTTParser
* @brief Destructor function
* 
* @return void
***************************************************************************/
WebVTTParser::~WebVTTParser()
{
	close();
}


/***************************************************************************
* @fn init
* @brief Initializes the parser instance
* 
* @param startPos[in] playlist start position in milliseconds
* @param basePTS[in] base PTS value
* @return bool true if successful, false otherwise
***************************************************************************/
bool WebVTTParser::init(double startPosSeconds, unsigned long long basePTS)
{
	bool ret = true;
	mStartPTS = basePTS;

	if (mVttQueueIdleTaskId == 0)
	{
		mVttQueueIdleTaskId = g_timeout_add(VTT_QUEUE_TIMER_INTERVAL, SendVttCueToExt, this);
	}

	AAMPLOG_WARN("WebVTTParser::startPos:%.3f and mStartPTS:%lld", startPosSeconds, mStartPTS);
	//We are ready to receive data, unblock in PrivateInstanceAAMP
	if(playerResumeTrackDownloads_CB)
	{
		playerResumeTrackDownloads_CB();
	}
	return ret;
}


/***************************************************************************
* @fn processData
* @brief Parse incoming VTT data
* 
* @param buffer[in] input VTT data
* @param bufferLen[in] data length
* @param position[in] position of buffer
* @param duration[in] duration of buffer
* @return bool true if successful, false otherwise
***************************************************************************/
bool WebVTTParser::processData(const char* cBuffer, size_t bufferLen, double position, double duration)
{
	char *buffer = (char *)cBuffer;
	bool ret = false;

	AAMPLOG_TRACE("WebVTTParser::Enter with position:%.3f and duration:%.3f ", position, duration);

	if (mReset)
	{
		mStartPos = (position * 1000) - mProgressOffset;
		mReset = false;
		AAMPLOG_WARN("WebVTTParser::Received first buffer after reset with mStartPos:%.3f",  mStartPos);
	}

	//Check for VTT signature at the start of buffer
	if (bufferLen > 6)
	{
		char *next = findWebVTTLineBreak(buffer);
		if (next && strlen(buffer) >= 6)
		{
			char *token = strtok(buffer, " \t\n\r");
			if(token != NULL)
			{
				//VTT is UTF-8 encoded and BOM is 0xEF,0xBB,0xBF
				if ((unsigned char) token[0] == 0xEF && (unsigned char) token[1] == 0xBB && (unsigned char) token[2] == 0xBF)
				{
					//skip BOM
					token += 3;
				}
				if (strlen(token) == 6 && strncmp(token, "WEBVTT", 6) == 0)
				{
					buffer = next;
					ret = true;
				}
			}
			else
			{
				AAMPLOG_WARN("token  is null");  //CID:85449,85515 - Null Returns
			}
		}
	}

	if (ret)
	{
		while (buffer)
		{
			char *nextLine = findWebVTTLineBreak(buffer);
			//TODO: Parse CUE ID

			if (strstr(buffer, "X-TIMESTAMP-MAP") != NULL)
			{
				unsigned long long mpegTime = 0;
				unsigned long long localTime = 0;
				//Found X-TIMESTAMP-MAP=LOCAL:<cue time>,MPEGTS:<MPEG-2 time>
				char* token = strtok(buffer, "=,");
				while (token)
				{
					if (token[0] == 'L')
					{
						localTime = convertHHMMSSToTime(token + 6);
					}
					else if (token[0] == 'M')
					{
						mpegTime = atoll(token + 7);
					}
					token = strtok(NULL, "=,");
				}
				mPtsOffset = (mpegTime / 90) - localTime; //in milliseconds
				AAMPLOG_INFO("Parsed local time:%lld and PTS:%lld and cuePTSOffset:%lld", localTime, mpegTime, mPtsOffset);
			}
			else if (strstr(buffer, " --> ") != NULL)
			{
				AAMPLOG_TRACE("Found cue:%s", buffer);
				long long start = -1;
				long long end = -1;
				char *text = NULL;
				//cue settings
				char *cueLine = NULL; //default "auto"
				char *cueLineAlign = NULL; //default "start"
				char *cuePosition = NULL; //default "auto"
				char *cuePosAlign = NULL; //default "auto"
				char *cueSize = NULL; //default 100
				char *cueTextAlign = NULL; //default "center"

				char *token = strtok(buffer, " -->\t");
				while (token != NULL)
				{
					AAMPLOG_TRACE("Inside tokenizer, token:%s", token);
					if (std::isdigit( static_cast<unsigned char>(token[0]) ) != 0)
					{
						if (start == -1)
						{
							start = convertHHMMSSToTime(token);
						}
						else if (end == -1)
						{
							end = convertHHMMSSToTime(token);
						}
					}
					//TODO:parse cue settings
					else
					{
						//for setting ':' is the delimiter
						char *key = token;
						char *value = strchr(token, ':');
						if (value != NULL)
						{
							*value = '\0';
							value++;
							if (strncmp(key, "line", 4) == 0)
							{
								char *lineAlign = strchr(value, ',');
								if (lineAlign != NULL)
								{
									*lineAlign = '\0';
									lineAlign++;
									if (std::find(allowedCueLineAligns.begin(), allowedCueLineAligns.end(), lineAlign) != allowedCueLineAligns.end())
									{
										cueLineAlign = lineAlign;
									}
								}
								cueLine = parsePercentageValueSetting(value);
							}
							else if (strncmp(key, "position", 8) == 0)
							{
								char *posAlign = strchr(value, ',');
								if (posAlign != NULL)
								{
									*posAlign = '\0';
									posAlign++;
									if (std::find(allowedCuePosAligns.begin(), allowedCuePosAligns.end(), posAlign) != allowedCuePosAligns.end())
									{
										cuePosAlign = posAlign;
									}
								}
								cuePosition = parsePercentageValueSetting(value);
							}
							else if (strncmp(key, "size", 4) == 0)
							{
								cueSize = parsePercentageValueSetting(value);
							}
							else if (strncmp(key, "align", 5) == 0)
							{
								if (std::find(allowedCueTextAligns.begin(), allowedCueTextAligns.end(), value) != allowedCueTextAligns.end())
								{
									cueTextAlign = value;
								}
							}
						}
					}
					token = strtok(NULL, " -->\t");
				}

				/* Avoid set but not used warnings as these are for future use. */
				(void)cueLine;
				(void)cueLineAlign;
				(void)cuePosition;
				(void)cuePosAlign;
				(void)cueSize;
				(void)cueTextAlign;

				text = nextLine;
				nextLine = findWebVTTLineBreak(nextLine);
				while(nextLine && (*nextLine != CHAR_LINE_FEED && *nextLine != CHAR_CARRIAGE_RETURN && *nextLine != '\0'))
				{
					AAMPLOG_TRACE("Found nextLine:%s", nextLine);
					if (nextLine[-1] == '\0')
					{
						nextLine[-1] = '\n';
					}
					if (nextLine[-2] == '\0')
					{
						nextLine[-2] = '\r';
					}
					nextLine = findWebVTTLineBreak(nextLine);
				}
				double cueStartInMpegTime = (start + mPtsOffset);
				double duration = (end - start);
				double mpegTimeOffset = cueStartInMpegTime - mStartPTS;
				double relativeStartPos = mStartPos + mpegTimeOffset; //w.r.t to position in reportProgress
				AAMPLOG_TRACE("So found cue with startPTS:%.3f and duration:%.3f, and mpegTimeOffset:%.3f and relative time being:%.3f", cueStartInMpegTime/1000.0, duration/1000.0, mpegTimeOffset/1000.0, relativeStartPos/1000.0);
				addCueData(new VTTCue(relativeStartPos, duration, std::string(text), std::string()));
			}

			buffer = nextLine;
		}
	}
	mCurrentPos = (position + duration) * 1000.0;
	AAMPLOG_TRACE("################# Exit sub PTS:%.3f", mCurrentPos);
	return ret;
}


/***************************************************************************
* @fn close
* @brief Close and release all resources
* 
 @return bool true if successful, false otherwise
***************************************************************************/
bool WebVTTParser::close()
{
	bool ret = true;
	if (mVttQueueIdleTaskId != 0)
	{
                AAMPLOG_INFO("WebVTTParser:: Remove mVttQueueIdleTaskId %d", mVttQueueIdleTaskId);
                g_source_remove(mVttQueueIdleTaskId);
                mVttQueueIdleTaskId = 0;
	}

	std::lock_guard<std::mutex> guard(mVttQueueMutex);
	if (!mVttQueue.empty())
	{
		while(mVttQueue.size() > 0)
		{
			VTTCue *cue = mVttQueue.front();
			mVttQueue.pop();
			SAFE_DELETE(cue);
		}
	}

	lastCue.mStart = 0;
	lastCue.mDuration = 0;
	mProgressOffset = 0;

	return ret;
}


/***************************************************************************
* @fn reset
* @brief Reset the parser
* 
* @return void
***************************************************************************/
void WebVTTParser::reset()
{
	//Called on discontinuity, blocks further VTT processing
	//Blocked until we get new basePTS
	AAMPLOG_WARN("WebVTTParser::Reset subtitle parser at position:%.3f", mCurrentPos);
	//Avoid calling stop injection if the first buffer is discontinuous
	if (!mReset)
	{
		if(playerStopTrackDownloads_CB)
		{
			playerStopTrackDownloads_CB();
		}
	}
	mPtsOffset = 0;
	mStartPTS = 0;
	mStartPos = 0;
	mReset = true;
}


/***************************************************************************
* @fn addCueData
* @brief Add cue to queue
* 
* @param cue[in] pointer to cue to store
* @return void
***************************************************************************/
void WebVTTParser::addCueData(VTTCue *cue)
{
	if (lastCue.mStart != cue->mStart || lastCue.mDuration != cue->mDuration)
	{
		std::lock_guard<std::mutex> guard(mVttQueueMutex);
		mVttQueue.push(cue);
		lastCue.mStart = cue->mStart;
		lastCue.mDuration = cue->mDuration;
	}
	else
	{
		delete cue;
	}
}


/***************************************************************************
* @fn sendCueData
* @brief Send cues stored in queue to AAMP
* 
* @return void
***************************************************************************/
void WebVTTParser::sendCueData()
{
	std::lock_guard<std::mutex> guard(mVttQueueMutex);
	if (!mVttQueue.empty())
	{
		while(mVttQueue.size() > 0)
		{
			VTTCue *cue = mVttQueue.front();
			mVttQueue.pop();
			if (cue->mStart > 0)
			{
				if(playerSendVTTCueData_CB)
				{
					playerSendVTTCueData_CB(cue);
				}
			}
			else
			{
				AAMPLOG_WARN("Discarding cue with start:%.3f and text:%s", cue->mStart/1000.0, cue->mText.c_str());
			}
			SAFE_DELETE(cue);
		}
	}
}

/**
 * @}
 */
