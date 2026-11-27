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

#import "DebugTerminalController.h"
#import "CommandInterpreter.h"

static const NSUInteger kMaxBufferChars = 300000;

@implementation DebugTerminalController
{
	NSWindow *_window;
	NSTextView *_textView;
	NSTextField *_input;
	CommandInterpreter *_interpreter;
	dispatch_queue_t _queue;
	NSMutableArray<NSString *> *_history;
	NSInteger _historyIndex;
}

@synthesize window = _window;

- (id)initWithInterpreter:(CommandInterpreter *)interpreter
{
	self = [super init];
	if (self)
	{
		_interpreter = interpreter;
		_queue = dispatch_queue_create("aamp.player.terminal", DISPATCH_QUEUE_SERIAL);
		_history = [NSMutableArray array];
		_historyIndex = 0;
		[self buildWindow];
		[self appendLine:@"AAMP debug terminal. Type 'help' for commands, 'help get' / 'help set' for names."];
	}
	return self;
}

- (void)buildWindow
{
	NSRect frame = NSMakeRect(0, 0, 780, 420);
	_window = [[NSWindow alloc] initWithContentRect:frame
		styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
		backing:NSBackingStoreBuffered defer:NO];
	_window.title = @"AAMP Debug Terminal";
	_window.releasedWhenClosed = NO;
	_window.delegate = self;
	NSView *content = _window.contentView;

	NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 32, frame.size.width, frame.size.height - 32)];
	scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	scroll.hasVerticalScroller = YES;
	_textView = [[NSTextView alloc] initWithFrame:scroll.bounds];
	_textView.autoresizingMask = NSViewWidthSizable;
	_textView.editable = NO;
	_textView.richText = NO;
	_textView.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];
	_textView.backgroundColor = [NSColor colorWithCalibratedWhite:0.08 alpha:1];
	_textView.textColor = [NSColor colorWithCalibratedRed:0.75 green:0.95 blue:0.75 alpha:1];
	scroll.documentView = _textView;
	[content addSubview:scroll];

	_input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, 32)];
	_input.autoresizingMask = NSViewWidthSizable;
	_input.font = [NSFont monospacedSystemFontOfSize:13 weight:NSFontWeightRegular];
	_input.placeholderString = @"command (tab completes, up/down for history)";
	_input.focusRingType = NSFocusRingTypeNone;
	_input.delegate = self;
	_input.target = self;
	_input.action = @selector(submit:);
	[content addSubview:_input];
	_window.initialFirstResponder = _input;
}

- (void)toggle
{
	if (_window.isVisible)
	{
		[_window orderOut:nil];
	}
	else
	{
		[_window makeKeyAndOrderFront:nil];
		[_window makeFirstResponder:_input];
	}
}

- (void)appendLine:(NSString *)line
{
	if (![NSThread isMainThread])
	{
		dispatch_async(dispatch_get_main_queue(), ^{ [self appendLine:line]; });
		return;
	}
	if (line.length == 0)
	{
		return;
	}
	NSDictionary *attrs = @{NSFontAttributeName: _textView.font, NSForegroundColorAttributeName: _textView.textColor};
	NSTextStorage *storage = _textView.textStorage;
	[storage appendAttributedString:[[NSAttributedString alloc] initWithString:[line stringByAppendingString:@"\n"] attributes:attrs]];
	if (storage.length > kMaxBufferChars)
	{
		[storage deleteCharactersInRange:NSMakeRange(0, storage.length - kMaxBufferChars / 2)];
	}
	[_textView scrollRangeToVisible:NSMakeRange(storage.length, 0)];
}

- (void)submit:(NSTextField *)sender
{
	NSString *cmd = [sender.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
	sender.stringValue = @"";
	if (cmd.length == 0)
	{
		return;
	}
	[_history addObject:cmd];
	_historyIndex = _history.count;
	[self appendLine:[@"> " stringByAppendingString:cmd]];
	if ([cmd isEqualToString:@"clear"])
	{
		_textView.string = @"";
		return;
	}
	dispatch_async(_queue, ^{
		NSString *out = [self->_interpreter execute:cmd];
		[self appendLine:out];
	});
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)selector
{
	if (selector == @selector(moveUp:) && _history.count > 0)
	{
		_historyIndex = MAX(0, _historyIndex - 1);
		control.stringValue = _history[_historyIndex];
		return YES;
	}
	if (selector == @selector(moveDown:) && _history.count > 0)
	{
		_historyIndex = MIN((NSInteger)_history.count, _historyIndex + 1);
		control.stringValue = (_historyIndex < (NSInteger)_history.count) ? _history[_historyIndex] : @"";
		return YES;
	}
	if (selector == @selector(insertTab:))
	{
		[self complete];
		return YES;
	}
	return NO;
}

- (void)complete
{
	NSString *line = _input.stringValue;
	NSArray<NSString *> *matches = [_interpreter completionsForLine:line];
	if (matches.count == 0)
	{
		return;
	}
	NSRange lastSpace = [line rangeOfString:@" " options:NSBackwardsSearch];
	NSString *head = (lastSpace.location == NSNotFound) ? @"" : [line substringToIndex:lastSpace.location + 1];
	if (matches.count == 1)
	{
		_input.stringValue = [NSString stringWithFormat:@"%@%@ ", head, matches[0]];
		return;
	}
	NSString *common = matches[0];
	for (NSString *m in matches)
	{
		common = [common commonPrefixWithString:m options:0];
	}
	_input.stringValue = [head stringByAppendingString:common];
	[self appendLine:[matches componentsJoinedByString:@"  "]];
}

@end
