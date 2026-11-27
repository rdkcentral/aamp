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

#import <Foundation/Foundation.h>

@class VideoView;
class PlayerInstanceAAMP;

/** Callbacks are always delivered on the main queue. */
@protocol PlayerEngineDelegate <NSObject>
- (void)engineStateChanged:(int)state name:(NSString *)name;
- (void)engineProgressPosition:(double)positionMs duration:(double)durationMs start:(double)startMs end:(double)endMs;
/** Keys: speed, playRate, videoBufMs, audioBufMs, liveLatencyMs, targetLatencyMs, profileBps, networkBps, pts (NSNumber), sei (NSString). */
- (void)engineProgressDetails:(NSDictionary<NSString *, id> *)details;
- (void)engineSpeedChanged:(float)rate;
- (void)engineMetadataWidth:(int)width height:(int)height durationMs:(long)durationMs;
- (void)engineError:(NSString *)message;
@end

/** Owns the PlayerInstanceAAMP and bridges its events to Objective-C. */
@interface PlayerEngine : NSObject

@property (nonatomic, weak) id<PlayerEngineDelegate> delegate;
/** Receives one line per AAMP event (called on the main queue). */
@property (nonatomic, copy) void (^eventLogger)(NSString *line);
@property (nonatomic) BOOL logProgressEvents;
@property (nonatomic, readonly) PlayerInstanceAAMP *player;
@property (nonatomic, copy, readonly) NSString *currentURL;
@property (nonatomic, readonly) int state;

+ (NSString *)nameForState:(int)state;

- (id)initWithVideoView:(VideoView *)view;
- (void)tune:(NSString *)url;
/** Applies `config` (AAMP config JSON object) for this playback only; later tunes revert to the global settings. */
- (void)tune:(NSString *)url config:(NSDictionary<NSString *, id> *)config contentType:(NSString *)contentType;
- (void)play;
- (void)pause;
- (void)stop;
- (void)seekTo:(double)seconds;
- (void)seekTo:(double)seconds keepPaused:(BOOL)keepPaused;
- (void)seekToLive;
- (void)seekToLiveKeepPaused:(BOOL)keepPaused;
- (void)setRate:(float)rate;
- (void)shutdown;

@end
