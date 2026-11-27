/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#import "PlayerEngine.h"
#import "VideoView.h"
#import "SettingsStore.h"
#undef __ACCESSIBILITY__ // Apple Accessibility.h uses the same guard as Accessibility.hpp
#include "main_aamp.h"
#include "GstUtils.h"
#include <glib.h>
#include <atomic>
#include <memory>
#include <thread>

@interface PlayerEngine ()
- (void)handleEvent:(const AAMPEventPtr &)event;
@end

namespace
{
class EventBridge : public AAMPEventObjectListener
{
public:
	__unsafe_unretained PlayerEngine *engine = nil;

	void Event(const AAMPEventPtr &event) override
	{
		[engine handleEvent:event];
	}
};

void InitGStreamerOnce()
{
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		int argc = 0;
		char **argv = nullptr;
		PlayerCliGstInit(&argc, &argv);
		GMainLoop *loop = g_main_loop_new(NULL, FALSE);
		std::thread([loop]() { g_main_loop_run(loop); }).detach();
	});
}
}

@implementation PlayerEngine
{
	PlayerInstanceAAMP *_player;
	std::unique_ptr<EventBridge> _bridge;
	std::atomic<int> _state;
	VideoView *_videoView;
	NSString *_currentURL;
	NSDictionary *_currentConfig;
	NSString *_currentContentType;
	BOOL _streamConfigApplied;
}

@synthesize player = _player;
@synthesize currentURL = _currentURL;

+ (NSString *)nameForState:(int)state
{
	static NSArray *names;
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		names = @[@"IDLE", @"INITIALIZING", @"INITIALIZED", @"PREPARING", @"PREPARED", @"BUFFERING",
			@"PAUSED", @"SEEKING", @"PLAYING", @"STOPPING", @"STOPPED", @"COMPLETE", @"ERROR",
			@"RELEASED", @"BLOCKED"];
	});
	return (state >= 0 && state < (int)names.count) ? names[state] : @"UNKNOWN";
}

- (id)initWithVideoView:(VideoView *)view
{
	self = [super init];
	if (self)
	{
		_state = eSTATE_IDLE;
		_videoView = view;
		InitGStreamerOnce();

		VideoView *target = view;
		_player = new PlayerInstanceAAMP(NULL, [target](const unsigned char *buf, int size, int width, int height) {
			[target displayI420:buf width:width height:height];
		});
		_player->SetAppName("AampPlayer");
		[SettingsStore applyTo:_player];

		_bridge = std::make_unique<EventBridge>();
		_bridge->engine = self;
		_player->RegisterEvents(_bridge.get());
	}
	return self;
}

- (int)state
{
	return _state.load();
}

- (void)log:(NSString *)line
{
	void (^logger)(NSString *) = self.eventLogger;
	if (logger)
	{
		dispatch_async(dispatch_get_main_queue(), ^{ logger(line); });
	}
}

- (void)handleEvent:(const AAMPEventPtr &)event
{
	switch (event->getType())
	{
		case AAMP_EVENT_STATE_CHANGED:
		{
			auto ev = std::dynamic_pointer_cast<StateChangedEvent>(event);
			int state = (int)ev->getState();
			_state = state;
			NSString *name = [PlayerEngine nameForState:state];
			dispatch_async(dispatch_get_main_queue(), ^{
				[self.delegate engineStateChanged:state name:name];
			});
			[self log:[NSString stringWithFormat:@"[event] STATE_CHANGED %@", name]];
			break;
		}
		case AAMP_EVENT_PROGRESS:
		{
			auto ev = std::dynamic_pointer_cast<ProgressEvent>(event);
			double pos = ev->getPosition(), dur = ev->getDuration(), start = ev->getStart(), end = ev->getEnd();
			const char *sei = ev->getSEITimeCode();
			NSDictionary *details = @{
				@"speed": @(ev->getSpeed()),
				@"playRate": @(ev->getCurrentPlayRate()),
				@"videoBufMs": @(ev->getVideoBufferedDuration()),
				@"audioBufMs": @(ev->getAudioBufferedDuration()),
				@"liveLatencyMs": @(ev->getLiveLatency()),
				@"targetLatencyMs": @(ev->getTargetLatency()),
				@"profileBps": @((long long)ev->getProfileBandwidth()),
				@"networkBps": @((long long)ev->getNetworkBandwidth()),
				@"pts": @(ev->getPTS()),
				@"sei": sei ? @(sei) : @""
			};
			dispatch_async(dispatch_get_main_queue(), ^{
				[self.delegate engineProgressPosition:pos duration:dur start:start end:end];
				[self.delegate engineProgressDetails:details];
			});
			if (self.logProgressEvents)
			{
				[self log:[NSString stringWithFormat:@"[event] PROGRESS pos=%.0fms dur=%.0fms start=%.0fms end=%.0fms", pos, dur, start, end]];
			}
			break;
		}
		case AAMP_EVENT_SPEED_CHANGED:
		{
			auto ev = std::dynamic_pointer_cast<SpeedChangedEvent>(event);
			float rate = ev->getRate();
			dispatch_async(dispatch_get_main_queue(), ^{
				[self.delegate engineSpeedChanged:rate];
			});
			[self log:[NSString stringWithFormat:@"[event] SPEED_CHANGED rate=%.2f", rate]];
			break;
		}
		case AAMP_EVENT_MEDIA_METADATA:
		{
			auto ev = std::dynamic_pointer_cast<MediaMetadataEvent>(event);
			int w = ev->getWidth(), h = ev->getHeight();
			long dur = ev->getDuration();
			dispatch_async(dispatch_get_main_queue(), ^{
				[self.delegate engineMetadataWidth:w height:h durationMs:dur];
			});
			[self log:[NSString stringWithFormat:@"[event] MEDIA_METADATA %dx%d duration=%ldms", w, h, dur]];
			break;
		}
		case AAMP_EVENT_TUNE_FAILED:
		{
			auto ev = std::dynamic_pointer_cast<MediaErrorEvent>(event);
			NSString *msg = [NSString stringWithFormat:@"Tune failed (%d): %s", ev->getCode(), ev->getDescription().c_str()];
			dispatch_async(dispatch_get_main_queue(), ^{
				[self.delegate engineError:msg];
			});
			[self log:[@"[event] " stringByAppendingString:msg]];
			break;
		}
		case AAMP_EVENT_TUNED:
			[self log:@"[event] TUNED"];
			break;
		case AAMP_EVENT_EOS:
			[self log:@"[event] EOS"];
			break;
		case AAMP_EVENT_BITRATE_CHANGED:
		{
			auto ev = std::dynamic_pointer_cast<BitrateChangeEvent>(event);
			[self log:[NSString stringWithFormat:@"[event] BITRATE_CHANGED %lld", (long long)ev->getBitrate()]];
			break;
		}
		default:
			[self log:[NSString stringWithFormat:@"[event] type=%d", (int)event->getType()]];
			break;
	}
}

- (void)tune:(NSString *)url
{
	[self tune:url config:nil contentType:nil];
}

- (void)tune:(NSString *)url config:(NSDictionary<NSString *, id> *)config contentType:(NSString *)contentType
{
	_currentURL = [url copy];
	_currentConfig = [config copy];
	_currentContentType = [contentType copy];

	// Restore the global settings when a stream-specific config was applied before or is needed now
	if (_streamConfigApplied || config.count > 0)
	{
		_player->ResetConfiguration();
		[SettingsStore applyTo:_player];
		_streamConfigApplied = NO;
	}
	if (config.count > 0)
	{
		NSData *data = [NSJSONSerialization dataWithJSONObject:config options:0 error:nil];
		NSString *json = data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
		if (json && _player->InitAAMPConfig(json.UTF8String))
		{
			_streamConfigApplied = YES;
		}
		else
		{
			[self log:@"[player] stream config rejected, using global settings"];
		}
	}
	_player->Tune(url.UTF8String, true, contentType.length > 0 ? contentType.UTF8String : NULL);
}

- (void)play
{
	int state = _state.load();
	if (state == eSTATE_PAUSED)
	{
		_player->SetRate(1);
	}
	else if (_currentURL.length > 0 &&
		(state == eSTATE_IDLE || state == eSTATE_STOPPED || state == eSTATE_COMPLETE || state == eSTATE_ERROR || state == eSTATE_RELEASED))
	{
		[self tune:_currentURL config:_currentConfig contentType:_currentContentType];
	}
}

- (void)pause
{
	_player->SetRate(0);
}

- (void)stop
{
	_player->Stop();
}

- (void)seekTo:(double)seconds
{
	_player->Seek(seconds);
}

- (void)seekTo:(double)seconds keepPaused:(BOOL)keepPaused
{
	_player->Seek(seconds, keepPaused);
}

- (void)seekToLive
{
	_player->SeekToLive();
}

- (void)seekToLiveKeepPaused:(BOOL)keepPaused
{
	_player->SeekToLive(keepPaused);
}

- (void)setRate:(float)rate
{
	_player->SetRate(rate);
}

- (void)shutdown
{
	if (_player)
	{
		_player->UnRegisterEvents(_bridge.get());
		_player->Stop();
		delete _player;
		_player = nullptr;
	}
}

@end
