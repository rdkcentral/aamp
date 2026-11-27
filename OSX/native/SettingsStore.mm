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

#import "SettingsStore.h"
#include "main_aamp.h"

@implementation SettingsStore

+ (NSString *)path
{
	NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	NSString *dir = [dirs.firstObject stringByAppendingPathComponent:@"AampPlayer"];
	[[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
	return [dir stringByAppendingPathComponent:@"settings.json"];
}

+ (NSMutableDictionary<NSString *, id> *)load
{
	NSData *data = [NSData dataWithContentsOfFile:[self path]];
	if (data)
	{
		id obj = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil];
		if ([obj isKindOfClass:[NSMutableDictionary class]])
		{
			return obj;
		}
	}
	return [NSMutableDictionary dictionary];
}

+ (void)save:(NSDictionary<NSString *, id> *)overrides
{
	NSData *data = [NSJSONSerialization dataWithJSONObject:overrides
		options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys error:nil];
	[data writeToFile:[self path] atomically:YES];
}

+ (NSUInteger)applyTo:(PlayerInstanceAAMP *)player
{
	NSDictionary *overrides = [self load];
	if (overrides.count == 0 || !player)
	{
		return 0;
	}
	NSData *data = [NSJSONSerialization dataWithJSONObject:overrides options:0 error:nil];
	NSString *json = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
	player->InitAAMPConfig(json.UTF8String);
	return overrides.count;
}

@end
