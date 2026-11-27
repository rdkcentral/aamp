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

#import "AppDelegate.h"
#import "PlayerWindowController.h"
#import "DebugTerminalController.h"
#import "SettingsWindowController.h"
#import "CommandInterpreter.h"

@implementation AppDelegate
{
	PlayerWindowController *_player;
	DebugTerminalController *_terminal;
	SettingsWindowController *_settings;
	CommandInterpreter *_interpreter;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
	_player = [[PlayerWindowController alloc] init];
	_interpreter = [[CommandInterpreter alloc] initWithEngine:_player.engine logPath:self.logPath];
	_terminal = [[DebugTerminalController alloc] initWithInterpreter:_interpreter];
	_settings = [[SettingsWindowController alloc] initWithEngine:_player.engine];

	DebugTerminalController *terminal = _terminal;
	_player.engine.eventLogger = ^(NSString *line) { [terminal appendLine:line]; };

	SettingsWindowController *settings = _settings;
	_player.showSettings = ^{ [settings show]; };
	_player.toggleTerminal = ^{ [terminal toggle]; };

	[self buildMenu];
	[_player showAndTune:self.initialURL];

	// Terminal is visible at launch, offset beside the player
	NSRect pf = _player.window.frame;
	[_terminal.window setFrameTopLeftPoint:NSMakePoint(pf.origin.x + 40, pf.origin.y - 10)];
	[_terminal toggle];
	[_player.window makeKeyAndOrderFront:nil];
	[NSApp activateIgnoringOtherApps:YES];
}

- (void)buildMenu
{
	NSMenu *bar = [[NSMenu alloc] init];
	NSMenuItem *item;

	NSMenu *app = [self addMenu:@"AAMP Player" to:bar];
	[app addItemWithTitle:@"Settings..." action:@selector(showSettings:) keyEquivalent:@","].target = self;
	[app addItem:NSMenuItem.separatorItem];
	[app addItemWithTitle:@"Quit AAMP Player" action:@selector(terminate:) keyEquivalent:@"q"];

	NSMenu *file = [self addMenu:@"File" to:bar];
	item = [file addItemWithTitle:@"Open URL..." action:@selector(focusURL:) keyEquivalent:@"o"];
	item.target = _player;

	NSMenu *view = [self addMenu:@"View" to:bar];
	[view addItemWithTitle:@"Debug Terminal" action:@selector(toggleTerminal:) keyEquivalent:@"d"].target = self;
	item = [view addItemWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
	item.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagControl;

	NSMenu *play = [self addMenu:@"Playback" to:bar];
	item = [play addItemWithTitle:@"Play / Pause" action:@selector(togglePlayPause:) keyEquivalent:@" "];
	item.keyEquivalentModifierMask = 0;
	item.target = _player;
	[play addItemWithTitle:@"Stop" action:@selector(stopPlayback:) keyEquivalent:@"."].target = _player;
	[play addItemWithTitle:@"Back 10s" action:@selector(skipBack:) keyEquivalent:@"["].target = _player;
	[play addItemWithTitle:@"Forward 10s" action:@selector(skipForward:) keyEquivalent:@"]"].target = _player;
	[play addItemWithTitle:@"Go to Live" action:@selector(goLive:) keyEquivalent:@"l"].target = _player;

	NSApp.mainMenu = bar;
}

- (NSMenu *)addMenu:(NSString *)title to:(NSMenu *)bar
{
	NSMenuItem *top = [[NSMenuItem alloc] init];
	NSMenu *menu = [[NSMenu alloc] initWithTitle:title];
	top.submenu = menu;
	[bar addItem:top];
	return menu;
}

- (void)showSettings:(id)sender
{
	[_settings show];
}

- (void)toggleTerminal:(id)sender
{
	[_terminal toggle];
}

- (void)toggleFullScreen:(id)sender
{
	[_player.window toggleFullScreen:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
	return NO;
}

- (void)applicationWillTerminate:(NSNotification *)note
{
	[_player.engine shutdown];
}

@end
