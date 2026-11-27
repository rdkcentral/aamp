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

#import "ContentCatalog.h"

static NSArray *DefaultEntries(void)
{
	return @[
		@{@"name": @"Big Buck Bunny (HLS)",
		  @"url": @"https://devstreaming-cdn.apple.com/videos/streaming/examples/img_bipbop_adv_example_ts/master.m3u8",
		  @"content_type": @"", @"description": @"Apple sample HLS stream", @"config": @{}},
		@{@"name": @"Sintel (DASH)",
		  @"url": @"https://bitmovin-a.akamaihd.net/content/sintel/sintel.mpd",
		  @"content_type": @"", @"description": @"Bitmovin sample DASH stream", @"config": @{}},
		@{@"name": @"Tears of Steel (DASH)",
		  @"url": @"https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel.ism/.mpd",
		  @"content_type": @"", @"description": @"Unified Streaming demo", @"config": @{}},
		@{@"name": @"Peacock LLD",
		  @"url": @"https://lowlatencytester.cdn.peacocktv.com/pcksle1/Content/CMAF_CTR-4s-v2/Live/channel(8570955-880351-7cb94c7d79f)/master_2hr-ll.mpd",
		  @"content_type": @"", @"description": @"Peacock low-latency DASH stream", @"config": @{}},
		@{@"name": @"Sample VOD Stream",
		  @"url": @"https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd",
		  @"content_type": @"DASH", @"description": @"Sample VOD DASH stream", @"config": @{}},
	];
}

@implementation ContentCatalog

+ (NSString *)path
{
	NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	NSString *dir = [dirs.firstObject stringByAppendingPathComponent:@"AampPlayer"];
	[[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
	return [dir stringByAppendingPathComponent:@"contentMap.json"];
}

+ (NSArray<NSDictionary<NSString *, id> *> *)load
{
	NSString *path = [self path];
	NSData *data = [NSData dataWithContentsOfFile:path];
	if (!data)
	{
		NSData *seed = [NSJSONSerialization dataWithJSONObject:DefaultEntries()
			options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys error:nil];
		[seed writeToFile:path atomically:YES];
		return DefaultEntries();
	}
	id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
	if (![obj isKindOfClass:[NSArray class]])
	{
		NSLog(@"contentMap.json is not a valid JSON array (no trailing commas allowed): %@", path);
		return @[];
	}
	NSMutableArray *entries = [NSMutableArray array];
	for (id item in obj)
	{
		if ([item isKindOfClass:[NSDictionary class]] && [item[@"url"] isKindOfClass:[NSString class]])
		{
			[entries addObject:item];
		}
	}
	return entries;
}

@end
