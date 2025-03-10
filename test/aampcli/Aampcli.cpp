/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 * @file Aampcli.cpp
 * @brief Stand alone AAMP player with command line interface.
 */

#include "Aampcli.h"
#include "scte35/AampSCTE35.h"
#include "AampcliShader.h"


Aampcli mAampcli;
const char *gApplicationPath = NULL;
extern VirtualChannelMap mVirtualChannelMap;
extern void tsdemuxer_InduceRollover( bool enable );
extern std::vector<std::vector<AdvertInfo>> mAdvertList;
static int mAdvertIndex = 0;
static int mAdReservationIndex = 0;

Aampcli :: Aampcli():
	mInitialized(false),
	mEnableProgressLog(false),
	mbAutoPlay(true),
	mTuneFailureDescription(""),
	mSingleton(NULL),
	mEventListener(NULL),
	mAampGstPlayerMainLoop(NULL),
	mAampMainLoopThread(NULL),
	mPlayerInstances(),
	mPlayerSessionID()
{
};

Aampcli::Aampcli(const Aampcli& aampcli):
	mInitialized(false),
	mEnableProgressLog(false),
	mbAutoPlay(true),
	mTuneFailureDescription(""),
	mSingleton(NULL),
	mEventListener(NULL),
	mAampGstPlayerMainLoop(NULL),
	mAampMainLoopThread(NULL),
	mPlayerInstances(),
	mPlayerSessionID(aampcli.mPlayerSessionID)
{
	mSingleton = aampcli.mSingleton;
	mEventListener = aampcli.mEventListener;
	
};

Aampcli& Aampcli::operator=(const Aampcli& aampcli)
{
	return *this;
};

Aampcli :: ~Aampcli()
{
};

void Aampcli::doAutomation( int startChannel, int stopChannel, int maxTuneTimeS, int playTimeS, int betweenTimeS )
{

	std::string outPath = aamp_GetConfigPath("/opt/test-results.csv");
	const char *mod = "wb"; // initially clear file
	CommandHandler lCommandHandler;

	if (mVirtualChannelMap.next() == NULL)
	{
		printf("[AAMPCLI] Can not auto channels, empty virtual channel map.\n");
		return;
	}

	for( int chan=startChannel; chan<=stopChannel; chan++ )
	{
		VirtualChannelInfo *info = mVirtualChannelMap.find(chan);
		if( info )
		{
			if( strstr(info->name.c_str(),"ClearKey") ||
					strstr(info->name.c_str(),"MultiDRM") ||
					strstr(info->name.c_str(),"DTS Audio") ||
					strstr(info->name.c_str(),"AC-4") )
			{
#ifdef __APPLE__
				continue; // skip unsupported DRM AND Audio formats
#endif
			}
			printf( "%d,\"%s\",%s,%s\n",
					info->channelNumber, info->name.c_str(), info->uri.c_str(), "TUNING...");

			char cmd[32];
			snprintf( cmd, sizeof(cmd), "%d", chan );
			mTuneFailureDescription.clear();
			lCommandHandler.dispatchAampcliCommands(cmd,mSingleton);
			AAMPPlayerState state = eSTATE_IDLE;
			for(int i=0; i<maxTuneTimeS; i++ )
			{
				sleep(1);
				state = mSingleton->GetState();
				if( state == eSTATE_PLAYING || state == eSTATE_ERROR )
				{
					break;
				}
			}
			const char *stateName;
			switch( state )
			{
				case eSTATE_PLAYING:
					sleep(playTimeS); // let play for a bit longer, as visibility sanity check
					stateName = "OK";
					printf( "***STOP***\n" );
					mSingleton->Stop();
					sleep( betweenTimeS );
					printf( "***NEXT***\n" );
					break;
				case eSTATE_ERROR:
					stateName = "FAIL";
					break;
				default:
					stateName = "TIMEOUT";
					break;
			}
			printf( "***%s\n", stateName );
			FILE *f = fopen( outPath.c_str(), mod );
			assert( f );
			fprintf( f, "%d,\"%s\",%s,%s,%s\n",
					info->channelNumber, info->name.c_str(), info->uri.c_str(), stateName, mTuneFailureDescription.c_str() );
			mod = "a";
			fclose( f );
		}
	}
}

void Aampcli::runCommand( std::string args )
{
	std::vector<std::string> cmdVec;
	CommandHandler lCommandHandler;
	lCommandHandler.registerAampcliCommands();
	using_history();
	if( !args.empty() )
	{
		lCommandHandler.dispatchAampcliCommands( args.c_str(), mAampcli.mSingleton);
	}
	printf("[AAMPCLI] type 'help' for list of available commands\n");
	for(;;)
	{
		rl_attempted_completion_function = lCommandHandler.commandCompletion;
		char *buffer = readline("[AAMPCLI] Enter cmd: ");
		if(buffer == NULL)
		{
			break;
		}
		char *ptr = buffer;
		while( ptr )
		{
			char *next = strchr(ptr,'\n');
			char *fin = next;
			if( next )
			{
				next++;
			}
			else
			{
				fin = ptr+strlen(ptr);
			}
			while( *ptr == ' ' ) ptr++; // skip leading whitespace
			while( fin>ptr && fin[-1]==' ' ) fin--;
			*fin = 0x00;
			
			if( *ptr )
			{
				add_history(ptr);
				bool l_status = lCommandHandler.dispatchAampcliCommands(ptr,mAampcli.mSingleton);
				if( !l_status )
				{
					exit(0);
				}
			}
			ptr = next;
		}
		free(buffer);
	} // for(;;)
} // Aampcli::runCommand

FILE * Aampcli::getConfigFile(const std::string& cfgFile)
{
	if (cfgFile.empty())
	{
		return NULL;
	}
	std::string path = aamp_GetConfigPath(cfgFile);
	FILE *f = fopen(path.c_str(), "rb");

	return f;
}

/**
 * @brief Thread to run mainloop (for standalone mode)
 * @param[in] arg user_data
 * @retval void pointer
 */
gpointer Aampcli::aampGstPlayerStreamThread(gpointer arg)
{
	if (mAampcli.mAampGstPlayerMainLoop)
	{
		g_main_loop_run(mAampcli.mAampGstPlayerMainLoop); // blocks
		printf("[AAMPCLI] aampGstPlayerStreamThread: exited main event loop\n");
	}
	g_main_loop_unref(mAampcli.mAampGstPlayerMainLoop);
	mAampcli.mAampGstPlayerMainLoop = NULL;
	return NULL;
}

/**
 * @brief To initialize Gstreamer and start mainloop (for standalone mode)
 * @param[in] argc number of arguments
 * @param[in] argv array of arguments
 */
void Aampcli::initPlayerLoop(int argc, char **argv)
{
	if (!mInitialized)
	{
		mInitialized = true;
		PlayerCliGstInit(&argc, &argv);
		
		mAampGstPlayerMainLoop = g_main_loop_new(NULL, FALSE);
		mAampMainLoopThread = g_thread_new("AAMPGstPlayerLoop", &aampGstPlayerStreamThread, NULL );
	}
}

void Aampcli::newPlayerInstance( std::string appName)
{
	PlayerInstanceAAMP *player = new PlayerInstanceAAMP(NULL, gpUpdateYUV );

	if (!appName.empty())
	{
		printf(" Set player name %s\n", appName.c_str());
		player->SetAppName(appName);
	}

	if( !mEventListener )
	{ // for now, use common event listener (could be instance specific)
		printf( "allocating new MyAAMPEventListener\n");
		mEventListener = new MyAAMPEventListener();
	}
	player->RegisterEvents(mEventListener);
	int playerId = player->GetId();
	printf( "new playerInstance; id=%d\n", playerId );
	mPlayerInstances.push_back(player);
	mPlayerSessionID.push_back({});
	mSingleton = player; // select
	mSingleton->SetContentProtectionDataUpdateTimeout(0);
}

int Aampcli::getApplicationDir( char *buffer, uint32_t size )
{
	if ((buffer == NULL) || (size == 0))
	{
		return 0;
	}

	buffer[0] = '\0';

	if (gApplicationPath != NULL)
	{
		if (strlen(gApplicationPath) < size)
		{
			// Is it a relative path?
			if (gApplicationPath[0] != '/')
			{
				// append it to the current working dir
				if(getcwd(buffer, size))
				{
					strncat(buffer, "/", size);
					buffer[size-1] = '\0';
					strncat(buffer, gApplicationPath, size);
					buffer[size-1] = '\0';
				}
			}
			else
			{
				// it's a ful path so just copy it
				strncpy(buffer, gApplicationPath, size);
				buffer[size-1] = '\0';
			}

			// strip off the app name to leave the path
			char *lastDir = strrchr(buffer, '/');
			if (lastDir)
			{
				*lastDir = '\0';
			}
		}
	}

	return (int)strnlen(buffer, size);
}

bool Aampcli::SetSessionId(std::string sid)
{
	const auto playerId = mSingleton->GetId();

	if (mPlayerSessionID.size() >= playerId)
	{
		mPlayerSessionID[playerId] = std::move(sid);
		std::cout << "[AAMPCLI] SessionId - " << playerId << " # " << mPlayerSessionID[playerId] << std::endl;
	}

	return true;
}

std::string Aampcli::GetSessionId() const
{
	const auto playerId = mSingleton->GetId();

	if (mPlayerSessionID.size() >= playerId)
	{
		return mPlayerSessionID[playerId];
	}

	return {};
}


std::string Aampcli::GetSessionId(size_t index) const
{
	if (mPlayerSessionID.size() > index)
	{
		return mPlayerSessionID[index];
	}

	return {};
}

/**
 * @brief
 * @param argc
 * @param argv
 * @retval
 */
int main(int argc, char **argv)
{
	AampLogManager::disableLogRedirection = true;
	ABRManager mAbrManager;

	gApplicationPath = argv[0];

	printf("**************************************************************************\n");
	printf("** ADVANCED ADAPTIVE MEDIA PLAYER (AAMP) - COMMAND LINE INTERFACE (CLI) **\n");
	printf("**************************************************************************\n");

	mAampcli.initPlayerLoop(0,NULL);
	mAampcli.newPlayerInstance();

	// Read/create virtual channel map
	const std::string cfgCSV("/opt/aampcli.csv");
	const std::string cfgLegacy("/opt/aampcli.cfg");
	FILE *f;
	if ( (f = mAampcli.getConfigFile(cfgCSV)) != NULL)
	{ // open virtual map from csv file
		printf("[AAMPCLI] opened aampcli.csv\n");
		mVirtualChannelMap.loadVirtualChannelMapFromCSV( f );
		fclose( f );
		f = NULL;
	}
	else if ( (f = mAampcli.getConfigFile(cfgLegacy)) != NULL)
	{  // open virtual map from legacy cfg file
		printf("[AAMPCLI] opened aampcli.cfg\n");
		mVirtualChannelMap.loadVirtualChannelMapLegacyFormat(f);
		fclose(f);
		f = NULL;
	}

	std::string args;
	for(int i = 1; i < argc; i++)
	{
		if( i>1 )
		{
			args += ' ';
		}
		args += std::string(argv[i]);
	}
	std::thread cmdThreadId = std::thread(&mAampcli.runCommand, args);
	createAppWindow(argc,argv);
	cmdThreadId.join();
	printf( "[AAMPCLI] done\n" );
}

void Aampcli::getAdvertUrl( uint32_t reqDuration, uint32_t &adDuration, std::vector<AdvertInfo>& adList)
{
	bool loop = false;
	std::string defUrl = "";
	advertInfo ad;
	mAdvertIndex = (mAdvertIndex < mAdvertList.size()) ? mAdvertIndex : 0;
	while (mAdvertIndex < mAdvertList.size())
	{
		if(mAdvertList[mAdvertIndex].size() == 0)
		{
			++mAdvertIndex;
			continue;
		}
		//If required duration Ad present in advert list will select it
		if(reqDuration == mAdvertList[mAdvertIndex][0].duration)
		{
			ad.url = mAdvertList[mAdvertIndex][0].url;
			ad.duration = mAdvertList[mAdvertIndex][0].duration;
			adList.push_back(ad);
			mAdvertIndex = mAdvertIndex + 1;
			break;
		}
		if((defUrl.empty()) && (mAdvertList[mAdvertIndex][0].duration == 0))
		{
			defUrl = mAdvertList[mAdvertIndex][0].url;
		}

		if(( loop == false) && ( mAdvertIndex + 1 == mAdvertList.size()))
		{
			mAdvertIndex = 0;
			loop = true;
		}
		else
		{
			mAdvertIndex++;
		}
	}

	//Required duration is not found in advert list then will add the present ads in list
	if(!adList.size())
	{
		mAdvertIndex = 0;
		int curDuration = 0;
		while((mAdvertIndex < mAdvertList.size()) && (curDuration < reqDuration))
		{
			if(mAdvertList[mAdvertIndex].size() == 0)
			{
				++mAdvertIndex;
				continue;
			}
			if((reqDuration - curDuration) >= mAdvertList[mAdvertIndex][0].duration)
			{
				 ad.url = mAdvertList[mAdvertIndex][0].url;
				 ad.duration = mAdvertList[mAdvertIndex][0].duration;
				 adList.push_back(ad);
				 mAdvertIndex = mAdvertIndex + 1;
				 curDuration += ad.duration;
			}
			else
			{
				 mAdvertIndex = mAdvertIndex + 1;
			}
		}
	}

	if((!adList.size()) && (!defUrl.empty()))
	{
		ad.url = defUrl;
		ad.duration = 0;
		adList.push_back(ad);
	}
}

void Aampcli::getAdvertUrlIndexed( std::vector<AdvertInfo>& adList, int idx)
{
	if(idx < 0)
	{
		printf("[AAMPCLI] Invalid index\n");
		return;
	}
	if(mAdvertList.size()==0)
	{
		printf("[AAMPCLI] Advert list is empty\n");
		return;
	}
	advertInfo ad;
	for (AdvertInfo& advert : mAdvertList[idx])
	{
		ad.url = advert.url;
		ad.duration =advert.duration;
		adList.push_back(ad);
	}
}

const char *MyAAMPEventListener::stringifyPlayerState(AAMPPlayerState state)
{
	static const char *stateName[] =
	{
		"IDLE",
		"INITIALIZING",
		"INITIALIZED",
		"PREPARING",
		"PREPARED",
		"BUFFERING",
		"PAUSED",
		"SEEKING",
		"PLAYING",
		"STOPPING",
		"STOPPED",
		"COMPLETE",
		"ERROR",
		"RELEASED"
	};
	if( state>=eSTATE_IDLE && state<=eSTATE_RELEASED )
	{
		return stateName[state];
	}
	else
	{
		return "UNKNOWN";
	}
}

/**
 * @brief Implementation of event callback
 * @param e Event
 */
void MyAAMPEventListener::Event(const AAMPEventPtr& e)
{
	switch (e->getType())
	{
		case AAMP_EVENT_STATE_CHANGED:
			{
				StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_STATE_CHANGED: %s (%d)\n", mAampcli.mEventListener->stringifyPlayerState(ev->getState()), ev->getState());
				break;
			}
		case AAMP_EVENT_SEEKED:
			{
				SeekedEventPtr ev = std::dynamic_pointer_cast<SeekedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_SEEKED: new positionMs %f\n", ev->getPosition());
				break;
			}
		case AAMP_EVENT_MEDIA_METADATA:
			{
				MediaMetadataEventPtr ev = std::dynamic_pointer_cast<MediaMetadataEvent>(e);
				std::vector<std::string> languages = ev->getLanguages();
				int langCount = ev->getLanguagesCount();
				printf("[AAMPCLI] AAMP_EVENT_MEDIA_METADATA\n");
				for (int i = 0; i < langCount; i++)
				{
					printf("[AAMPCLI] language: %s\n", languages[i].c_str());
				}
				printf("[AAMPCLI] AAMP_EVENT_MEDIA_METADATA\n\tDuration=%ld\n\twidth=%d\n\tHeight=%d\n\tHasDRM=%d\n\tProgreamStartTime=%f\n\tTsbDepthMs=%d\n", ev->getDuration(), ev->getWidth(), ev->getHeight(), ev->hasDrm(), ev->getProgramStartTime(), ev->getTsbDepth());
				int bitrateCount = ev->getBitratesCount();
				std::vector<BitsPerSecond> bitrates = ev->getBitrates();
				printf("[AAMPCLI] Bitrates:\n");
				for(int i = 0; i < bitrateCount; i++)
				{
					printf("\t[AAMPCLI] bitrate(%d)=%ld\n", i, bitrates.at(i));
				}
				printf("[AAMPCLI] Supported Speeds:\n");
				const std::vector<float> &supportedSpeeds = ev->getSupportedSpeeds();
				for( int i=0; i<supportedSpeeds.size(); i++ )
				{
					printf( "\t[AAMPCLI] supportedSpeed(%d)=%f\n", i, supportedSpeeds[i] );
				}
				break;
			}
		case AAMP_EVENT_TUNED:
			{
				printf("[AAMPCLI] AAMP_EVENT_TUNED\n");
				break;
			}
		case AAMP_EVENT_TUNE_FAILED:
			{
				MediaErrorEventPtr ev = std::dynamic_pointer_cast<MediaErrorEvent>(e);
				mAampcli.mTuneFailureDescription = ev->getDescription();
				printf("[AAMPCLI] AAMP_EVENT_TUNE_FAILED reason=%s\n",mAampcli.mTuneFailureDescription.c_str());
				break;
			}
		case AAMP_EVENT_SPEED_CHANGED:
			{
				SpeedChangedEventPtr ev = std::dynamic_pointer_cast<SpeedChangedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_SPEED_CHANGED current rate=%f\n", ev->getRate());
				break;
			}
		case AAMP_EVENT_DRM_METADATA:
			{
				DrmMetaDataEventPtr ev = std::dynamic_pointer_cast<DrmMetaDataEvent>(e);
				printf("[AAMPCLI] AAMP_DRM_FAILED Tune failure:%d\t\naccess status str:%s\t\naccess status val:%d\t\nResponse code:%d\t\nIs SecClient error:%d\t\n",ev->getFailure(), ev->getAccessStatus().c_str(), ev->getAccessStatusValue(), ev->getResponseCode(), ev->getSecclientError());
				printf("[AAMPCLI] AAMP_DRM_FAILED Tune failure:networkmetrics:%s\n",ev->getNetworkMetricData().c_str());
				break;
			}
		case AAMP_EVENT_EOS:
			printf("[AAMPCLI] AAMP_EVENT_EOS\n");
			break;

		case AAMP_EVENT_PLAYLIST_INDEXED:
			printf("[AAMPCLI] AAMP_EVENT_PLAYLIST_INDEXED\n");
			break;
		case AAMP_EVENT_PROGRESS:
			{
				ProgressEventPtr ev = std::dynamic_pointer_cast<ProgressEvent>(e);
				if(mAampcli.mEnableProgressLog)
				{
					char seekableRange[32];
					auto start = ev->getStart();
					auto end = ev->getEnd();
					if( start<0 && end<0 )
					{
						snprintf( seekableRange, sizeof(seekableRange), "n/a" );
					}
					else
					{
						snprintf( seekableRange, sizeof(seekableRange), "[start=%.3fs end=%.3fs]", start/1000.0, end/1000.0 );
					}
					
					printf("[AAMPCLI] AAMP_EVENT_PROGRESS\n\tduration=%.3fs\n\tposition=%.3fs\n\tseekableRange%s\n\tcurrRate=%.3f\n\tbufferedDuration=%.3fs\n\tPTS=%lld\n\ttimecode='%s'\n\tlatency=%.3fs\n\tprofileBandwidth=%ld\n\tnetworkBandwidth=%ld\n\tcurrentPlayRate=%.3f\n\tsessionId='%s'\n", ev->getDuration()/1000.0, ev->getPosition()/1000.0, seekableRange, ev->getSpeed(), ev->getBufferedDuration()/1000.0, ev->getPTS(), ev->getSEITimeCode(), ev->getLiveLatency()/1000.0, ev->getProfileBandwidth(), ev->getNetworkBandwidth(), ev->getCurrentPlayRate(), ev->GetSessionId().c_str());
				}
			}
			break;
		case AAMP_EVENT_CC_HANDLE_RECEIVED:
			{
				CCHandleEventPtr ev = std::dynamic_pointer_cast<CCHandleEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_CC_HANDLE_RECEIVED CCHandle=%lu\n",ev->getCCHandle());
				break;
			}
		case AAMP_EVENT_BITRATE_CHANGED:
			{
				BitrateChangeEventPtr ev = std::dynamic_pointer_cast<BitrateChangeEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_BITRATE_CHANGED\n\tbitrate=%" BITSPERSECOND_FORMAT "\n\tdescription=\"%s\"\n\tresolution=%dx%d@%ffps\n\ttime=%d\n\tposition=%lf\n", ev->getBitrate(), ev->getDescription().c_str(), ev->getWidth(), ev->getHeight(), ev->getFrameRate(), ev->getTime(), ev->getPosition());
				break;
			}
		case AAMP_EVENT_AUDIO_TRACKS_CHANGED:
			printf("[AAMPCLI] AAMP_EVENT_AUDIO_TRACKS_CHANGED\n");
			break;
		case AAMP_EVENT_TEXT_TRACKS_CHANGED:
			printf("[AAMPCLI] AAMP_EVENT_TEXT_TRACKS_CHANGED\n");
			break;
		case AAMP_EVENT_ID3_METADATA:
			printf("[AAMPCLI] AAMP_EVENT_ID3_METADATA\n");
			{
				auto ev = std::dynamic_pointer_cast<ID3MetadataEvent>(e);
				printf("[AAMPCLI] :: presentation time: %" PRIu64 "\n\n", ev->getPresentationTime());
			}

			break;
		case AAMP_EVENT_BLOCKED :
			{
				BlockedEventPtr ev = std::dynamic_pointer_cast<BlockedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_BLOCKED Reason:%s\n" ,ev->getReason().c_str());
				break;
			}
		case AAMP_EVENT_CONTENT_GAP :
			{
				ContentGapEventPtr ev = std::dynamic_pointer_cast<ContentGapEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_CONTENT_GAP\n\tStart:%lf\n\tDuration:%lf\n", ev->getTime(), ev->getDuration());
				break;
			}
		case AAMP_EVENT_WATERMARK_SESSION_UPDATE:
			{
				WatermarkSessionUpdateEventPtr ev = std::dynamic_pointer_cast<WatermarkSessionUpdateEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_WATERMARK_SESSION_UPDATE SessionHandle:%d Status:%d System:%s\n" ,ev->getSessionHandle(), ev->getStatus(), ev->getSystem().c_str());
				break;
			}
		case AAMP_EVENT_BUFFERING_CHANGED:
			{
				BufferingChangedEventPtr ev = std::dynamic_pointer_cast<BufferingChangedEvent>(e);
				printf("[AAMPCLI] AAMP_EVENT_BUFFERING_CHANGED Sending Buffer Change event status (Buffering): %s", (ev->buffering() ? "End": "Start"));
				break;
			}
		case AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE:
			{
				ContentProtectionDataEventPtr ev =  std::dynamic_pointer_cast<ContentProtectionDataEvent>(e);
				printf("[AMPCLI] AAMP_EVENT_CONTENT_PROTECTION_UPDATE received stream type %s\n",ev->getStreamType().c_str());
				std::vector<uint8_t> key = ev->getKeyID();
				printf("[AMPCLI] AAMP_EVENT_CONTENT_PROTECTION_UPDATE received key is ");
				for(int i=0;i<key.size();i++)
					printf("%x",key.at(i)&0xff);
				printf("\n");
				cJSON *root = cJSON_CreateObject();
				cJSON *KeyId = cJSON_CreateArray();
				for(int i=0;i<key.size();i++)
					cJSON_AddItemToArray(KeyId, cJSON_CreateNumber(key.at(i)));
				cJSON_AddItemToObject(root,"keyID",KeyId);
				std::string json = cJSON_Print(root);
				mAampcli.mSingleton->ProcessContentProtectionDataConfig(json.c_str());
				break;
			}

		case AAMP_EVENT_TIMED_METADATA:
		{
			TimedMetadataEventPtr ev =  std::dynamic_pointer_cast<TimedMetadataEvent>(e);
			if ( ev->getName() == "SCTE35" )
			{
				printf("\n[AAMPCLI] AAMP_EVENT_TIMED_METADATA received\n");
				/* Decode any SCTE35 splice info event. */
				std::vector<SCTE35SpliceInfo::Summary> spliceInfoSummary;
				SCTE35SpliceInfo spliceInfo(ev->getContent());

				spliceInfo.getSummary(spliceInfoSummary);
				for (auto &splice : spliceInfoSummary)
				{
					AAMPLOG_WARN("[CDAI] splice info type %d, time %f, duration %f, id 0x%" PRIx32,
						(int)splice.type, splice.time, splice.duration, splice.event_id);

					if ((splice.type == SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_ADVERTISEMENT_START) ||
						(splice.type == SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_PLACEMENT_OPPORTUNITY_START))
					{
							/* A set of ads should be selected for insertion based
							 * on the splice info event type, id and duration.
							 */
							AAMPLOG_WARN("[CDAI] Dynamic ad start signalled mAdBrkIndex(%d)", mAampcli.mAdBrkIndex);

							uint32_t adDuration = 0;
							std::vector<AdvertInfo> adList;
							std::string adId = "adId";
							if(!mAampcli.mIndexedAds)
							{
								//If we have an advert list, use that
								if (mAdvertList.size())
								{
									mAampcli.getAdvertUrl(splice.duration, adDuration, adList);
								}
								else
								{
									// set default url
									advertInfo ad;
									ad.url = "https://example.com/AD/HD/manifest.mpd";
									ad.duration = 0;
									adList.push_back(ad);

								}
							}
							else
							{
								//If we have an advert list, use that
								if (mAdvertList.size())
								{
									mAampcli.getAdvertUrlIndexed(adList,mAampcli.mAdBrkIndex);
								}
							}
							for(int i=0; i<adList.size(); i++)
							{
								++mAdReservationIndex;
								adId = "adId" + std::to_string(mAdReservationIndex);
								if (adList[i].url == "file://skip")
								{
									printf("[AMPCLI] AAMP_EVENT_TIMED_METADATA skip advert placement breakId=%s adId=%s\n", ev->getId().c_str(), adId.c_str());
								}
								else
								{
									printf("[AMPCLI] AAMP_EVENT_TIMED_METADATA place advert breakId=%s adId=%s duration=%d url=%s\n", ev->getId().c_str(), adId.c_str(), adList[i].duration, adList[i].url.c_str());
									mAampcli.mSingleton->SetAlternateContents(ev->getId(), adId, adList[i].url);
								}
							}

							mAampcli.mAdBrkIndex++;
							if(mAampcli.mAdBrkIndex >= mAdvertList.size()) // shouldn't go hear ideally , just safety measure
							{
								mAampcli.mAdBrkIndex = 0;
							}
					}
					else
					{
						AAMPLOG_INFO("[CDAI] No dynamic ad start signalled");
					}
				}
			}
			break;
		}

		case AAMP_EVENT_MANIFEST_REFRESH_NOTIFY:
		{
			std::string manifest;
			ManifestRefreshEventPtr ev = std::dynamic_pointer_cast<ManifestRefreshEvent>(e);
			printf("\n[AAMPCLI] AAMP_EVENT_MANIFEST_REFRESH_NOTIFY received Dur[%u]:NoPeriods[%u]:PubTime[%u]\nmanifestType[%s]\n",ev->getManifestDuration(),ev->getNoOfPeriods(),ev->getManifestPublishedTime(),ev->getManifestType());
			manifest = mAampcli.mSingleton->GetManifest();
			printf("\n [AAMPCLI] Dash  Manifest length [%zu]\n",manifest.length());
			break;
		}
		case AAMP_EVENT_TUNE_TIME_METRICS:
		{
			TuneTimeMetricsEventPtr ev = std::dynamic_pointer_cast<TuneTimeMetricsEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_TUNE_TIME_METRICS\n\tData[%s]\n",ev->getTuneMetricsData().c_str());
			break;
		}

		case AAMP_EVENT_AD_RESOLVED:
		{
			AdResolvedEventPtr ev = std::dynamic_pointer_cast<AdResolvedEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_AD_RESOLVED\n\tresolveStatus=%d\n\tadId=%s\n\tstart=%" PRIu64 "\n\tduration=%" PRIu64 "\n", ev->getResolveStatus(), ev->getAdId().c_str(), ev->getStart(), ev->getDuration());
			break;
		}

		case AAMP_EVENT_AD_RESERVATION_START:
		{
			AdReservationEventPtr ev = std::dynamic_pointer_cast<AdReservationEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_AD_RESERVATION_START\n\tadBreakId=%s\n\tposition=%" PRIu64 "\n", ev->getAdBreakId().c_str(), ev->getPosition());
			break;
		}

		case AAMP_EVENT_AD_RESERVATION_END:
		{
			AdReservationEventPtr ev = std::dynamic_pointer_cast<AdReservationEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_AD_RESERVATION_END\n\tadBreakId=%s\n\tposition=%" PRIu64 "\n", ev->getAdBreakId().c_str(), ev->getPosition());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_START:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_START\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_END:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_END\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_ERROR:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_ERROR\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_PROGRESS:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			printf("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}
		case AAMP_EVENT_NEED_MANIFEST_DATA:
		{
			printf("[AAMPCLI]  AAMP_EVENT_NEED_MANIFEST_DATA received \n");
			std::string manifestData = PlaybackCommand::getManifestData(mAampcli.mManifestDataUrl);
			printf("[AAMPCLI] updateManifest\n");
			mAampcli.mSingleton->updateManifest(manifestData.c_str());
			break;
		}

		default:
			break;
	}
}
