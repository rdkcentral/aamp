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

#import <Cocoa/Cocoa.h>
#import "PlayerEngine.h"

@class VideoView;

/** Main player window: video plane with overlaid transport controls. */
@interface PlayerWindowController : NSObject <NSWindowDelegate, PlayerEngineDelegate>

@property (nonatomic, readonly) NSWindow *window;
@property (nonatomic, readonly) PlayerEngine *engine;
/** Invoked from the settings and terminal buttons. */
@property (nonatomic, copy) void (^showSettings)(void);
@property (nonatomic, copy) void (^toggleTerminal)(void);

- (id)init;
- (void)showAndTune:(NSString *)initialURL;

- (void)togglePlayPause:(id)sender;
- (void)stopPlayback:(id)sender;
- (void)skipBack:(id)sender;
- (void)skipForward:(id)sender;
- (void)goLive:(id)sender;
- (void)focusURL:(id)sender;

@end
