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

#import "SettingsWindowController.h"
#import "PlayerEngine.h"
#import "SettingsStore.h"
#undef __ACCESSIBILITY__ // Apple Accessibility.h uses the same guard as Accessibility.hpp
#include "main_aamp.h"

typedef NS_ENUM(NSInteger, SettingKind) { SettingKindBool, SettingKindNumber, SettingKindString };

static SettingKind KindOf(id value)
{
	if ([value isKindOfClass:[NSNumber class]])
	{
		return (CFGetTypeID((__bridge CFTypeRef)value) == CFBooleanGetTypeID()) ? SettingKindBool : SettingKindNumber;
	}
	return SettingKindString;
}

static NSString *DisplayValue(id value)
{
	if (KindOf(value) == SettingKindBool)
	{
		return [value boolValue] ? @"true" : @"false";
	}
	return [NSString stringWithFormat:@"%@", value];
}

@implementation SettingsWindowController
{
	__unsafe_unretained PlayerEngine *_engine;
	NSWindow *_window;
	NSTableView *_table;
	NSSearchField *_search;
	NSTextField *_status;
	NSDictionary<NSString *, id> *_all;        // current values reported by AAMP
	NSMutableDictionary<NSString *, id> *_overrides;
	NSArray<NSString *> *_visibleKeys;
}

- (id)initWithEngine:(PlayerEngine *)engine
{
	self = [super init];
	if (self)
	{
		_engine = engine;
		_overrides = [SettingsStore load];
		[self buildWindow];
	}
	return self;
}

- (void)buildWindow
{
	NSRect frame = NSMakeRect(0, 0, 720, 560);
	_window = [[NSWindow alloc] initWithContentRect:frame
		styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered defer:NO];
	_window.title = @"AAMP Settings";
	_window.releasedWhenClosed = NO;
	NSView *content = _window.contentView;

	_search = [[NSSearchField alloc] initWithFrame:NSMakeRect(12, frame.size.height - 36, frame.size.width - 24, 24)];
	_search.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
	_search.placeholderString = @"Filter settings";
	_search.delegate = self;
	[content addSubview:_search];

	NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 44, frame.size.width, frame.size.height - 88)];
	scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	scroll.hasVerticalScroller = YES;
	_table = [[NSTableView alloc] initWithFrame:scroll.bounds];
	_table.usesAlternatingRowBackgroundColors = YES;
	NSArray *columns = @[@[@"name", @"Setting", @260], @[@"type", @"Type", @70], @[@"value", @"Value", @320]];
	for (NSArray *c in columns)
	{
		NSTableColumn *col = [[NSTableColumn alloc] initWithIdentifier:c[0]];
		col.title = c[1];
		col.width = [c[2] doubleValue];
		col.editable = [c[0] isEqualToString:@"value"];
		[_table addTableColumn:col];
	}
	_table.dataSource = self;
	_table.delegate = self;
	scroll.documentView = _table;
	[content addSubview:scroll];

	_status = [NSTextField labelWithString:@"Edits apply to the running player; most take effect on the next tune."];
	_status.frame = NSMakeRect(12, 14, 430, 18);
	_status.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin;
	_status.textColor = NSColor.secondaryLabelColor;
	[content addSubview:_status];

	NSButton *reload = [NSButton buttonWithTitle:@"Reload" target:self action:@selector(reload:)];
	reload.frame = NSMakeRect(frame.size.width - 270, 8, 80, 28);
	reload.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
	[content addSubview:reload];

	NSButton *reset = [NSButton buttonWithTitle:@"Reset to Defaults" target:self action:@selector(resetAll:)];
	reset.frame = NSMakeRect(frame.size.width - 180, 8, 168, 28);
	reset.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
	[content addSubview:reset];
}

- (void)show
{
	[self reload:nil];
	[_window center];
	[_window makeKeyAndOrderFront:nil];
}

- (void)reload:(id)sender
{
	PlayerInstanceAAMP *player = _engine.player;
	if (!player)
	{
		return;
	}
	std::string cfg = player->GetAAMPConfig();
	NSData *data = [NSData dataWithBytes:cfg.data() length:cfg.size()];
	id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
	_all = [obj isKindOfClass:[NSDictionary class]] ? obj : @{};
	[self applyFilter];
}

- (void)applyFilter
{
	NSString *needle = _search.stringValue.lowercaseString;
	NSArray *keys = [_all.allKeys sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
	if (needle.length > 0)
	{
		keys = [keys filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString *k, id _) {
			return [k.lowercaseString containsString:needle];
		}]];
	}
	_visibleKeys = keys;
	[_table reloadData];
	_status.stringValue = [NSString stringWithFormat:@"%lu settings, %lu overridden. Edits apply live; most take effect on the next tune.",
		(unsigned long)_visibleKeys.count, (unsigned long)_overrides.count];
}

- (void)controlTextDidChange:(NSNotification *)note
{
	if (note.object == _search)
	{
		[self applyFilter];
	}
}

- (void)resetAll:(id)sender
{
	PlayerInstanceAAMP *player = _engine.player;
	if (player)
	{
		player->ResetConfiguration();
	}
	[_overrides removeAllObjects];
	[SettingsStore save:_overrides];
	[self reload:nil];
}

#pragma mark - NSTableViewDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
	return (NSInteger)_visibleKeys.count;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
	NSString *key = _visibleKeys[row];
	id value = _all[key];
	if ([column.identifier isEqualToString:@"name"])
	{
		return _overrides[key] ? [key stringByAppendingString:@" *"] : key;
	}
	if ([column.identifier isEqualToString:@"type"])
	{
		return @[@"bool", @"number", @"string"][KindOf(value)];
	}
	return DisplayValue(value);
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)object forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
	NSString *key = _visibleKeys[row];
	NSString *text = [[object description] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
	id parsed = nil;
	switch (KindOf(_all[key]))
	{
		case SettingKindBool:
			if ([text isEqualToString:@"true"] || [text isEqualToString:@"1"] || [text isEqualToString:@"on"]) parsed = @YES;
			else if ([text isEqualToString:@"false"] || [text isEqualToString:@"0"] || [text isEqualToString:@"off"]) parsed = @NO;
			break;
		case SettingKindNumber:
		{
			NSScanner *scanner = [NSScanner scannerWithString:text];
			double d;
			if ([scanner scanDouble:&d] && scanner.isAtEnd)
			{
				parsed = (d == (long long)d) ? @((long long)d) : @(d);
			}
			break;
		}
		case SettingKindString:
			parsed = text;
			break;
	}
	if (!parsed)
	{
		NSBeep();
		return;
	}

	PlayerInstanceAAMP *player = _engine.player;
	NSData *json = [NSJSONSerialization dataWithJSONObject:@{key: parsed} options:0 error:nil];
	NSString *jsonText = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
	if (!player || !player->InitAAMPConfig(jsonText.UTF8String))
	{
		NSBeep();
		_status.stringValue = [NSString stringWithFormat:@"AAMP rejected %@", key];
		return;
	}
	_overrides[key] = parsed;
	[SettingsStore save:_overrides];
	[self reload:nil];
}

@end
