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

class PlayerInstanceAAMP;

/** Persists AAMP config overrides made in the settings window. */
@interface SettingsStore : NSObject

/** ~/Library/Application Support/AampPlayer/settings.json */
+ (NSString *)path;
+ (NSMutableDictionary<NSString *, id> *)load;
+ (void)save:(NSDictionary<NSString *, id> *)overrides;
/** Applies persisted overrides to a player; returns the number applied. */
+ (NSUInteger)applyTo:(PlayerInstanceAAMP *)player;

@end
