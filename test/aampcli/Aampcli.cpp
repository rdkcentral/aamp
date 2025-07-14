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
//#include "AampcliShader.h"

Aampcli mAampcli;
const char *gApplicationPath = NULL;
extern VirtualChannelMap mVirtualChannelMap;

extern std::vector<AdvertInfo> mAdvertList;
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
		AAMPCLI_PRINTF("[AAMPCLI] Can not auto channels, empty virtual channel map.\n");
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
			AAMPCLI_PRINTF( "%d,\"%s\",%s,%s\n",
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
					AAMPCLI_PRINTF( "***STOP***\n" );
					mSingleton->Stop();
					sleep( betweenTimeS );
					AAMPCLI_PRINTF( "***NEXT***\n" );
					break;
				case eSTATE_ERROR:
					stateName = "FAIL";
					break;
				default:
					stateName = "TIMEOUT";
					break;
			}
			AAMPCLI_PRINTF( "***%s\n", stateName );
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
	AAMPCLI_PRINTF("[AAMPCLI] type 'help' for list of available commands\n");
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
		AAMPCLI_PRINTF("[AAMPCLI] aampGstPlayerStreamThread: exited main event loop\n");
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
		AAMPCLI_PRINTF(" Set player name %s\n", appName.c_str());
		player->SetAppName(appName);
	}

	if( !mEventListener )
	{ // for now, use common event listener (could be instance specific)
		AAMPCLI_PRINTF( "allocating new MyAAMPEventListener\n");
		mEventListener = new MyAAMPEventListener();
	}
	player->RegisterEvents(mEventListener);
	int playerId = player->GetId();
	AAMPCLI_PRINTF( "new playerInstance; id=%d\n", playerId );
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

void Aampcli::doHandleAampCliCommands(std::string args)
{
	CommandHandler lCommandHandler;
	if( !args.empty() )
	{
		lCommandHandler.dispatchAampcliCommands( args.c_str(), mAampcli.mSingleton);
	}
}

/**
 * @brief
 * @param argc
 * @param argv
 * @retval
 */
static int main_func(int argc, char **argv)
{
	AampLogManager::disableLogRedirection = true;
	ABRManager mAbrManager;

	gApplicationPath = argv[0];

	AAMPCLI_PRINTF("**************************************************************************\n");
	AAMPCLI_PRINTF("** ADVANCED ADAPTIVE MEDIA PLAYER (AAMP) - COMMAND LINE INTERFACE (CLI) **\n");
	AAMPCLI_PRINTF("**************************************************************************\n");

	mAampcli.initPlayerLoop(0,NULL);
	mAampcli.newPlayerInstance();

	// Read/create virtual channel map
	const std::string cfgCSV("/opt/aampcli.csv");
	const std::string cfgLegacy("/opt/aampcli.cfg");
	FILE *f;
	if ( (f = mAampcli.getConfigFile(cfgCSV)) != NULL)
	{ // open virtual map from csv file
		AAMPCLI_PRINTF("[AAMPCLI] opened aampcli.csv\n");
		mVirtualChannelMap.loadVirtualChannelMapFromCSV( f );
		fclose( f );
		f = NULL;
	}
	else if ( (f = mAampcli.getConfigFile(cfgLegacy)) != NULL)
	{  // open virtual map from legacy cfg file
		AAMPCLI_PRINTF("[AAMPCLI] opened aampcli.cfg\n");
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
	AAMPCLI_PRINTF( "[AAMPCLI] done\n" );
	return 0;
}

int main( int argc, char **argv )
{
#if defined(__APPLE__) && !defined(USE_OPENGL) && defined(__aarch64__)
	return gst_macos_main((GstMainFunc)main_func, argc, argv, NULL);
#else
	return main_func(argc,argv);
#endif
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
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_STATE_CHANGED: %s (%d)\n", mAampcli.mEventListener->stringifyPlayerState(ev->getState()), ev->getState());
				break;
			}
		case AAMP_EVENT_SEEKED:
			{
				SeekedEventPtr ev = std::dynamic_pointer_cast<SeekedEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_SEEKED: new positionMs %f\n", ev->getPosition());
				break;
			}
		case AAMP_EVENT_MEDIA_METADATA:
			{
				MediaMetadataEventPtr ev = std::dynamic_pointer_cast<MediaMetadataEvent>(e);
				std::vector<std::string> languages = ev->getLanguages();
				int langCount = ev->getLanguagesCount();
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_MEDIA_METADATA\n");
				for (int i = 0; i < langCount; i++)
				{
					AAMPCLI_PRINTF("[AAMPCLI] language: %s\n", languages[i].c_str());
				}
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_MEDIA_METADATA\n\tDuration=%ld\n\twidth=%d\n\tHeight=%d\n\tHasDRM=%d\n\tProgreamStartTime=%f\n\tTsbDepthMs=%d\n\tUrl=%s\n", ev->getDuration(), ev->getWidth(), ev->getHeight(), ev->hasDrm(), ev->getProgramStartTime(), ev->getTsbDepth(), ev->getUrl().c_str());
				int bitrateCount = ev->getBitratesCount();
				std::vector<BitsPerSecond> bitrates = ev->getBitrates();
				AAMPCLI_PRINTF("[AAMPCLI] Bitrates:\n");
				for(int i = 0; i < bitrateCount; i++)
				{
					AAMPCLI_PRINTF("\t[AAMPCLI] bitrate(%d)=%ld\n", i, bitrates.at(i));
				}
				AAMPCLI_PRINTF("[AAMPCLI] Supported Speeds:\n");
				const std::vector<float> &supportedSpeeds = ev->getSupportedSpeeds();
				for( int i=0; i<supportedSpeeds.size(); i++ )
				{
					AAMPCLI_PRINTF( "\t[AAMPCLI] supportedSpeed(%d)=%f\n", i, supportedSpeeds[i] );
				}
				break;
			}
		case AAMP_EVENT_TUNED:
			{
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_TUNED\n");
				break;
			}
		case AAMP_EVENT_TUNE_FAILED:
			{
				MediaErrorEventPtr ev = std::dynamic_pointer_cast<MediaErrorEvent>(e);
				mAampcli.mTuneFailureDescription = ev->getDescription();
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_TUNE_FAILED reason=%s\n",mAampcli.mTuneFailureDescription.c_str());
				break;
			}
		case AAMP_EVENT_SPEED_CHANGED:
			{
				SpeedChangedEventPtr ev = std::dynamic_pointer_cast<SpeedChangedEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_SPEED_CHANGED current rate=%f\n", ev->getRate());
				break;
			}
		case AAMP_EVENT_DRM_METADATA:
			{
				DrmMetaDataEventPtr ev = std::dynamic_pointer_cast<DrmMetaDataEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_DRM_FAILED Tune failure:%d\t\naccess status str:%s\t\naccess status val:%d\t\nResponse code:%d\t\nIs SecClient error:%d\t\n",ev->getFailure(), ev->getAccessStatus().c_str(), ev->getAccessStatusValue(), ev->getResponseCode(), ev->getSecclientError());
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_DRM_FAILED Tune failure:networkmetrics:%s\n",ev->getNetworkMetricData().c_str());
				break;
			}
		case AAMP_EVENT_EOS:
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_EOS\n");
			break;

		case AAMP_EVENT_PLAYLIST_INDEXED:
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_PLAYLIST_INDEXED\n");
			break;
		case AAMP_EVENT_PROGRESS:
			{
				ProgressEventPtr ev = std::dynamic_pointer_cast<ProgressEvent>(e);
				if(mAampcli.mEnableProgressLog)
				{
					char seekableRange[48];
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

					AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_PROGRESS duration=%.3fs position=%.3fs seekableRange%s currRate=%.3f bufferedVideoDuration=%.3fs bufferedAudioDuration=%.3fs  PTS=%lld timecode='%s' latency=%.3fs profileBandwidth=%ld networkBandwidth=%ld currentPlayRate=%.3f sessionId='%s'\n", ev->getDuration()/1000.0, ev->getPosition()/1000.0, seekableRange, ev->getSpeed(), ev->getVideoBufferedDuration()/1000.0, ev->getAudioBufferedDuration()/1000.0, ev->getPTS(), ev->getSEITimeCode(), ev->getLiveLatency()/1000.0, ev->getProfileBandwidth(), ev->getNetworkBandwidth(), ev->getCurrentPlayRate(), ev->GetSessionId().c_str());
				}
			}
			break;
		case AAMP_EVENT_CC_HANDLE_RECEIVED:
			{
				CCHandleEventPtr ev = std::dynamic_pointer_cast<CCHandleEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_CC_HANDLE_RECEIVED CCHandle=%lu\n",ev->getCCHandle());
				break;
			}
		case AAMP_EVENT_BITRATE_CHANGED:
			{
				BitrateChangeEventPtr ev = std::dynamic_pointer_cast<BitrateChangeEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_BITRATE_CHANGED\n\tbitrate=%" BITSPERSECOND_FORMAT "\n\tdescription=\"%s\"\n\tresolution=%dx%d@%ffps\n\ttime=%d\n\tposition=%lf\n", ev->getBitrate(), ev->getDescription().c_str(), ev->getWidth(), ev->getHeight(), ev->getFrameRate(), ev->getTime(), ev->getPosition());
				break;
			}
		case AAMP_EVENT_AUDIO_TRACKS_CHANGED:
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AUDIO_TRACKS_CHANGED\n");
			break;
		case AAMP_EVENT_TEXT_TRACKS_CHANGED:
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_TEXT_TRACKS_CHANGED\n");
			break;
		case AAMP_EVENT_ID3_METADATA:
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_ID3_METADATA\n");
			{
				auto ev = std::dynamic_pointer_cast<ID3MetadataEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] :: presentation time: %" PRIu64 "\n\n", ev->getPresentationTime());
			}

			break;
		case AAMP_EVENT_BLOCKED :
			{
				BlockedEventPtr ev = std::dynamic_pointer_cast<BlockedEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_BLOCKED Reason:%s\n" ,ev->getReason().c_str());
				break;
			}
		case AAMP_EVENT_CONTENT_GAP :
			{
				ContentGapEventPtr ev = std::dynamic_pointer_cast<ContentGapEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_CONTENT_GAP\n\tStart:%lf\n\tDuration:%lf\n", ev->getTime(), ev->getDuration());
				break;
			}
		case AAMP_EVENT_WATERMARK_SESSION_UPDATE:
			{
				WatermarkSessionUpdateEventPtr ev = std::dynamic_pointer_cast<WatermarkSessionUpdateEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_WATERMARK_SESSION_UPDATE SessionHandle:%d Status:%d System:%s\n" ,ev->getSessionHandle(), ev->getStatus(), ev->getSystem().c_str());
				break;
			}
		case AAMP_EVENT_BUFFERING_CHANGED:
			{
				BufferingChangedEventPtr ev = std::dynamic_pointer_cast<BufferingChangedEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_BUFFERING_CHANGED Sending Buffer Change event status (Buffering): %s", (ev->buffering() ? "End": "Start"));
				break;
			}
		case AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE:
			{
				ContentProtectionDataEventPtr ev =  std::dynamic_pointer_cast<ContentProtectionDataEvent>(e);
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_CONTENT_PROTECTION_UPDATE received stream type %s\n",ev->getStreamType().c_str());
				std::vector<uint8_t> key = ev->getKeyID();
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_CONTENT_PROTECTION_UPDATE received key is ");
				for(int i=0;i<key.size();i++)
					AAMPCLI_PRINTF("%x",key.at(i)&0xff);
				AAMPCLI_PRINTF("\n");
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
			if( ev->getName() == "SCTE35" )
			{
				AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_TIMED_METADATA received\n");
				/* Decode any SCTE35 splice info event. */
				std::vector<SCTE35SpliceInfo::Summary> spliceInfoSummary;
				SCTE35SpliceInfo spliceInfo(ev->getContent());
				spliceInfo.getSummary(spliceInfoSummary);
				bool mapped = false;
				for( auto &splice : spliceInfoSummary)
				{
					AAMPCLI_PRINTF("[AAMPCLI] SCTE35SpliceInfo type=%d time=%fs duration=%fs id=0x%" PRIx32 "\n",
						   static_cast<int>(splice.type), splice.time, splice.duration, splice.event_id );
					switch( splice.type )
					{
						case SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_ADVERTISEMENT_START:
						case SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_PLACEMENT_OPPORTUNITY_START:
							AAMPCLI_PRINTF("[AAMPCLI] [CDAI] Dynamic ad start signalled for breakId='%s'\n)", ev->getId().c_str() );
							for( const AdvertInfo &advertInfo : mAdvertList )
							{
								if( advertInfo.adBreakId == ev->getId() )
								{
									std::string adId = "adId" + std::to_string(++mAdReservationIndex);
									AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_TIMED_METADATA place advert breakId=%s adId=%s url=%s\n", ev->getId().c_str(), adId.c_str(), advertInfo.url.c_str());
									mAampcli.mSingleton->SetAlternateContents(ev->getId(), adId, advertInfo.url);
									mapped = true;
								}
							}
							if( !mapped )
							{
								AAMPCLI_PRINTF( "[AAMPCLI] unmapped breakId=%s\n", ev->getId().c_str() );
							}
							break;
						default:
							break;
					} // splice.type
				} // spliceInfoSummary
			} // SCTE35
			break;
		}

		case AAMP_EVENT_MANIFEST_REFRESH_NOTIFY:
		{
			std::string manifest;
			ManifestRefreshEventPtr ev = std::dynamic_pointer_cast<ManifestRefreshEvent>(e);
			AAMPCLI_PRINTF("\n[AAMPCLI] AAMP_EVENT_MANIFEST_REFRESH_NOTIFY received Dur[%u]:NoPeriods[%u]:PubTime[%u] manifestType[%s]\n",ev->getManifestDuration(),ev->getNoOfPeriods(),ev->getManifestPublishedTime(),ev->getManifestType().c_str());
			manifest = mAampcli.mSingleton->GetManifest();
			AAMPCLI_PRINTF("\n [AAMPCLI] Dash  Manifest length [%zu]\n",manifest.length());
			break;
		}
		case AAMP_EVENT_TUNE_TIME_METRICS:
		{
			TuneTimeMetricsEventPtr ev = std::dynamic_pointer_cast<TuneTimeMetricsEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_TUNE_TIME_METRICS\n\tData[%s]\n",ev->getTuneMetricsData().c_str());
			break;
		}

		case AAMP_EVENT_AD_RESOLVED:
		{
			AdResolvedEventPtr ev = std::dynamic_pointer_cast<AdResolvedEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AD_RESOLVED\tresolveStatus=%d\tadId=%s\tstart=%" PRIu64 "\tduration=%" PRIu64 "\terrorCode=%s\terrorDescription=%s\n", ev->getResolveStatus(), ev->getAdId().c_str(), ev->getStart(), ev->getDuration(), ev->getErrorCode().c_str(), ev->getErrorDescription().c_str());
			break;
		}

		case AAMP_EVENT_AD_RESERVATION_START:
		{
			AdReservationEventPtr ev = std::dynamic_pointer_cast<AdReservationEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=%s\tposition=%" PRIu64 "\n", ev->getAdBreakId().c_str(), ev->getPosition());
			break;
		}

		case AAMP_EVENT_AD_RESERVATION_END:
		{
			AdReservationEventPtr ev = std::dynamic_pointer_cast<AdReservationEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=%s\tposition=%" PRIu64 "\n", ev->getAdBreakId().c_str(), ev->getPosition());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_START:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_START\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_END:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_END\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_ERROR:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_ERROR\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}

		case AAMP_EVENT_AD_PLACEMENT_PROGRESS:
		{
			AdPlacementEventPtr ev = std::dynamic_pointer_cast<AdPlacementEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=%s\tposition=%u\toffset=%u\tduration=%u\terror=%d\n", ev->getAdId().c_str(), ev->getPosition(), ev->getOffset(), ev->getDuration(), ev->getErrorCode());
			break;
		}
		case AAMP_EVENT_NEED_MANIFEST_DATA:
		{
			AAMPCLI_PRINTF("[AAMPCLI]  AAMP_EVENT_NEED_MANIFEST_DATA received \n");
			std::string manifestData = PlaybackCommand::getManifestData(mAampcli.mManifestDataUrl);
			AAMPCLI_PRINTF("[AAMPCLI] updateManifest\n");
			mAampcli.mSingleton->updateManifest(manifestData.c_str());
			break;
		}
		case AAMP_EVENT_MONITORAV_STATUS:
		{
			MonitorAVStatusEventPtr ev = std::dynamic_pointer_cast<MonitorAVStatusEvent>(e);
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_MONITORAV_STATUS\tstatus=%s\tvposition =%" PRId64 "\taposition=%" PRId64 "\ttimeInStateMS= %" PRIu64 "\tdroppedFrames= %" PRIu64 "\n", ev->getMonitorAVStatus().c_str(), ev->getVideoPositionMS(), ev->getAudioPositionMS(), ev->getTimeInStateMS(),ev->getDroppedFrames());
		}
		case AAMP_EVENT_REPORT_ANOMALY:
		{
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_REPORT_ANOMALY received \n");
			break;
		}


		case AAMP_EVENT_ENTERING_LIVE:
		{
			AAMPCLI_PRINTF("[AAMPCLI] AAMP_EVENT_ENTERING_LIVE\n");
			break;
		}

		default:
			break;
	}
}
