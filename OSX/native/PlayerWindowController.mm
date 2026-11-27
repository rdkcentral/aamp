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

#import "PlayerWindowController.h"
#import "VideoView.h"
#import "ContentCatalog.h"
#undef __ACCESSIBILITY__ // Apple Accessibility.h uses the same guard as Accessibility.hpp
#include "main_aamp.h"

static const CGFloat kBarHeightTop = 44;
static const CGFloat kBarHeightBottom = 124;
static const double kSkipSeconds = 10;

static NSString *FormatTime(double seconds)
{
	if (seconds < 0 || !isfinite(seconds)) seconds = 0;
	long total = (long)seconds;
	return [NSString stringWithFormat:@"%ld:%02ld:%02ld", total / 3600, (total / 60) % 60, total % 60];
}

static NSButton *IconButton(NSString *symbol, NSString *tip, id target, SEL action)
{
	NSImage *image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:tip];
	NSButton *button = [NSButton buttonWithImage:image target:target action:action];
	button.bordered = NO;
	button.toolTip = tip;
	button.contentTintColor = NSColor.whiteColor;
	button.frame = NSMakeRect(0, 0, 32, 28);
	return button;
}

static NSVisualEffectView *OverlayBar(NSRect frame, NSAutoresizingMaskOptions mask)
{
	NSVisualEffectView *bar = [[NSVisualEffectView alloc] initWithFrame:frame];
	bar.material = NSVisualEffectMaterialHUDWindow;
	bar.blendingMode = NSVisualEffectBlendingModeWithinWindow;
	bar.state = NSVisualEffectStateActive;
	bar.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
	bar.autoresizingMask = mask;
	return bar;
}

@implementation PlayerWindowController
{
	NSWindow *_window;
	PlayerEngine *_engine;
	VideoView *_video;
	NSVisualEffectView *_topBar, *_bottomBar;
	NSTextField *_urlField, *_stateLabel, *_timeLabel, *_infoLabel;
	NSSlider *_seekSlider, *_volumeSlider;
	NSButton *_playPause, *_stop, *_back, *_forward, *_live, *_mute, *_settings, *_terminal;
	NSPopUpButton *_ratePopup, *_contentPopup;
	NSArray<NSDictionary<NSString *, id> *> *_catalog;
	NSTextField *_seekField, *_overlay;
	NSButton *_seekButton, *_keepPausedCheck, *_monitorCheck;
	NSString *_stateName;
	long long _profileBps;
	BOOL _scrubbing;
	double _positionSec;
	int _savedVolume;
}

@synthesize window = _window;
@synthesize engine = _engine;

- (id)init
{
	self = [super init];
	if (self)
	{
		_savedVolume = 100;
		[self buildWindow];
		_engine = [[PlayerEngine alloc] initWithVideoView:_video];
		_engine.delegate = self;
	}
	return self;
}

- (void)buildWindow
{
	NSRect frame = NSMakeRect(0, 0, 960, 540);
	_window = [[NSWindow alloc] initWithContentRect:frame
		styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered defer:NO];
	_window.title = @"AAMP Player";
	_window.releasedWhenClosed = NO;
	_window.delegate = self;
	_window.minSize = NSMakeSize(640, 360);
	_window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;

	_video = [[VideoView alloc] initWithFrame:frame];
	_video.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	_window.contentView = _video;

	// Top bar: URL entry and state
	_topBar = OverlayBar(NSMakeRect(0, frame.size.height - kBarHeightTop, frame.size.width, kBarHeightTop),
		NSViewWidthSizable | NSViewMinYMargin);
	_urlField = [NSTextField textFieldWithString:@""];
	_urlField.placeholderString = @"Manifest URL (DASH / HLS), press Return to play";
	_urlField.target = self;
	_urlField.action = @selector(urlEntered:);
	[_topBar addSubview:_urlField];

	_contentPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 200, 26) pullsDown:NO];
	_catalog = [ContentCatalog load];
	[_contentPopup addItemWithTitle:@"Select content..."];
	for (NSDictionary *entry in _catalog)
	{
		[_contentPopup addItemWithTitle:entry[@"name"] ?: entry[@"url"]];
		_contentPopup.lastItem.toolTip = entry[@"description"];
	}
	_contentPopup.target = self;
	_contentPopup.action = @selector(contentChosen:);
	[_topBar addSubview:_contentPopup];
	_stateLabel = [NSTextField labelWithString:@"IDLE"];
	_stateLabel.textColor = NSColor.whiteColor;
	_stateLabel.alignment = NSTextAlignmentRight;
	[_topBar addSubview:_stateLabel];
	[_video addSubview:_topBar];

	// Bottom bar: seek, time, transport, rate, volume
	_bottomBar = OverlayBar(NSMakeRect(0, 0, frame.size.width, kBarHeightBottom), NSViewWidthSizable | NSViewMaxYMargin);
	_seekSlider = [NSSlider sliderWithValue:0 minValue:0 maxValue:1 target:self action:@selector(seekChanged:)];
	_seekSlider.continuous = YES;
	[_bottomBar addSubview:_seekSlider];
	_timeLabel = [NSTextField labelWithString:@"0:00:00 / 0:00:00"];
	_timeLabel.textColor = NSColor.whiteColor;
	_timeLabel.alignment = NSTextAlignmentRight;
	_timeLabel.font = [NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightRegular];
	[_bottomBar addSubview:_timeLabel];

	_playPause = IconButton(@"play.fill", @"Play / Pause", self, @selector(togglePlayPause:));
	_stop = IconButton(@"stop.fill", @"Stop", self, @selector(stopPlayback:));
	_back = IconButton(@"gobackward.10", @"Back 10s", self, @selector(skipBack:));
	_forward = IconButton(@"goforward.10", @"Forward 10s", self, @selector(skipForward:));
	_live = IconButton(@"antenna.radiowaves.left.and.right", @"Go to live", self, @selector(goLive:));
	_mute = IconButton(@"speaker.wave.2.fill", @"Mute", self, @selector(toggleMute:));
	_settings = IconButton(@"gearshape", @"Settings", self, @selector(openSettings:));
	_terminal = IconButton(@"terminal", @"Debug terminal", self, @selector(openTerminal:));
	for (NSButton *b in @[_playPause, _stop, _back, _forward, _live, _mute, _settings, _terminal])
	{
		[_bottomBar addSubview:b];
	}

	_ratePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 70, 24) pullsDown:NO];
	for (NSString *title in @[@"-16x", @"-4x", @"-2x", @"1x", @"2x", @"4x", @"16x"])
	{
		[_ratePopup addItemWithTitle:title];
	}
	[_ratePopup selectItemWithTitle:@"1x"];
	_ratePopup.target = self;
	_ratePopup.action = @selector(rateChosen:);
	[_bottomBar addSubview:_ratePopup];

	_volumeSlider = [NSSlider sliderWithValue:100 minValue:0 maxValue:100 target:self action:@selector(volumeChanged:)];
	[_bottomBar addSubview:_volumeSlider];

	_infoLabel = [NSTextField labelWithString:@""];
	_infoLabel.textColor = NSColor.secondaryLabelColor;
	_infoLabel.font = [NSFont systemFontOfSize:11];
	[_bottomBar addSubview:_infoLabel];

	// Advanced row
	_seekField = [NSTextField textFieldWithString:@""];
	_seekField.placeholderString = @"seconds";
	_seekField.target = self;
	_seekField.action = @selector(seekFromField:);
	[_bottomBar addSubview:_seekField];
	_seekButton = [NSButton buttonWithTitle:@"Seek" target:self action:@selector(seekFromField:)];
	[_bottomBar addSubview:_seekButton];
	_keepPausedCheck = [NSButton checkboxWithTitle:@"Remain paused" target:nil action:nil];
	[_bottomBar addSubview:_keepPausedCheck];
	_monitorCheck = [NSButton checkboxWithTitle:@"Progress monitor" target:self action:@selector(monitorToggled:)];
	[_bottomBar addSubview:_monitorCheck];
	[_video addSubview:_bottomBar];

	_overlay = [NSTextField labelWithString:@""];
	_overlay.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];
	_overlay.textColor = NSColor.whiteColor;
	_overlay.drawsBackground = YES;
	_overlay.backgroundColor = [NSColor colorWithWhite:0 alpha:0.6];
	_overlay.hidden = YES;
	[_video addSubview:_overlay];

	[self layoutBars];
}

- (void)layoutBars
{
	CGFloat w = _window.contentView.bounds.size.width;

	_urlField.frame = NSMakeRect(220, 10, w - 232 - 190, 24);
	_contentPopup.frame = NSMakeRect(12, 9, 200, 26);
	_stateLabel.frame = NSMakeRect(w - 12 - 180, 12, 180, 20);

	_seekSlider.frame = NSMakeRect(12, 92, w - 24 - 140, 20);
	_timeLabel.frame = NSMakeRect(w - 12 - 130, 92, 130, 20);

	_seekField.frame = NSMakeRect(12, 10, 110, 24);
	_seekButton.frame = NSMakeRect(128, 8, 70, 28);
	_keepPausedCheck.frame = NSMakeRect(208, 11, 140, 20);
	_monitorCheck.frame = NSMakeRect(356, 11, 160, 20);

	CGFloat x = 12;
	for (NSButton *b in @[_playPause, _stop, _back, _forward, _live])
	{
		b.frame = NSMakeRect(x, 52, 32, 28);
		x += 36;
	}
	_ratePopup.frame = NSMakeRect(x + 4, 52, 72, 26);
	_infoLabel.frame = NSMakeRect(x + 90, 56, MAX(60, w - x - 90 - 12 - 4 * 36 - 120), 16);

	CGFloat r = w - 12;
	_terminal.frame = NSMakeRect(r - 32, 52, 32, 28);
	_settings.frame = NSMakeRect(r - 68, 52, 32, 28);
	_volumeSlider.frame = NSMakeRect(r - 68 - 100, 54, 90, 20);
	_mute.frame = NSMakeRect(r - 68 - 100 - 36, 52, 32, 28);

	NSSize size = _overlay.intrinsicContentSize;
	CGFloat h = _window.contentView.bounds.size.height;
	_overlay.frame = NSMakeRect(12, h - kBarHeightTop - 12 - size.height, size.width, size.height);
}

- (void)showAndTune:(NSString *)initialURL
{
	[_window center];
	[_window makeKeyAndOrderFront:nil];
	if (initialURL.length > 0)
	{
		_urlField.stringValue = initialURL;
		[_engine tune:initialURL];
	}
}

#pragma mark - Actions

- (void)urlEntered:(NSTextField *)sender
{
	NSString *url = [sender.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
	if (url.length > 0)
	{
		[_contentPopup selectItemAtIndex:0];
		[_engine tune:url];
		[_window makeFirstResponder:nil];
	}
}

- (void)contentChosen:(NSPopUpButton *)popup
{
	NSInteger index = popup.indexOfSelectedItem - 1;
	if (index < 0 || index >= (NSInteger)_catalog.count)
	{
		return;
	}
	NSDictionary *entry = _catalog[index];
	_urlField.stringValue = entry[@"url"];
	NSDictionary *config = [entry[@"config"] isKindOfClass:[NSDictionary class]] ? entry[@"config"] : nil;
	NSString *type = [entry[@"content_type"] isKindOfClass:[NSString class]] ? entry[@"content_type"] : nil;
	[_engine tune:entry[@"url"] config:config contentType:type];
}

- (BOOL)keepPaused
{
	return _keepPausedCheck.state == NSControlStateValueOn;
}

- (void)seekFromField:(id)sender
{
	NSString *text = [_seekField.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
	NSScanner *scanner = [NSScanner scannerWithString:text];
	double target;
	if (![scanner scanDouble:&target] || !scanner.isAtEnd)
	{
		NSBeep();
		return;
	}
	target = MIN(MAX(target, _seekSlider.minValue), _seekSlider.maxValue);
	[_engine seekTo:target keepPaused:[self keepPaused]];
}

- (void)monitorToggled:(NSButton *)sender
{
	_overlay.hidden = (sender.state != NSControlStateValueOn);
}

- (void)focusURL:(id)sender
{
	[_window makeKeyAndOrderFront:nil];
	[_window makeFirstResponder:_urlField];
}

- (void)togglePlayPause:(id)sender
{
	if (_engine.state == eSTATE_PLAYING || _engine.state == eSTATE_BUFFERING)
	{
		[_engine pause];
	}
	else
	{
		[_engine play];
	}
}

- (void)stopPlayback:(id)sender
{
	[_engine stop];
}

- (void)skipBack:(id)sender
{
	[_engine seekTo:MAX(0, _positionSec - kSkipSeconds) keepPaused:[self keepPaused]];
}

- (void)skipForward:(id)sender
{
	[_engine seekTo:_positionSec + kSkipSeconds keepPaused:[self keepPaused]];
}

- (void)goLive:(id)sender
{
	[_engine seekToLiveKeepPaused:[self keepPaused]];
}

- (void)seekChanged:(NSSlider *)slider
{
	BOOL ended = (NSApp.currentEvent.type == NSEventTypeLeftMouseUp);
	_scrubbing = !ended;
	_timeLabel.stringValue = [NSString stringWithFormat:@"%@ / %@", FormatTime(slider.doubleValue), FormatTime(slider.maxValue)];
	if (ended)
	{
		[_engine seekTo:slider.doubleValue keepPaused:[self keepPaused]];
	}
}

- (void)rateChosen:(NSPopUpButton *)popup
{
	float rate = popup.titleOfSelectedItem.floatValue;
	[_engine setRate:rate];
}

- (void)volumeChanged:(NSSlider *)slider
{
	int volume = (int)slider.doubleValue;
	_engine.player->SetAudioVolume(volume);
	[self updateMuteIcon:volume == 0];
}

- (void)toggleMute:(id)sender
{
	int current = (int)_volumeSlider.doubleValue;
	int target;
	if (current > 0)
	{
		_savedVolume = current;
		target = 0;
	}
	else
	{
		target = _savedVolume;
	}
	_volumeSlider.doubleValue = target;
	_engine.player->SetAudioVolume(target);
	[self updateMuteIcon:target == 0];
}

- (void)updateMuteIcon:(BOOL)muted
{
	_mute.image = [NSImage imageWithSystemSymbolName:(muted ? @"speaker.slash.fill" : @"speaker.wave.2.fill")
		accessibilityDescription:@"Mute"];
}

- (void)openSettings:(id)sender
{
	if (self.showSettings) self.showSettings();
}

- (void)openTerminal:(id)sender
{
	if (self.toggleTerminal) self.toggleTerminal();
}

#pragma mark - PlayerEngineDelegate

- (void)engineStateChanged:(int)state name:(NSString *)name
{
	_stateName = name;
	if (state == eSTATE_IDLE || state == eSTATE_STOPPED || state == eSTATE_ERROR || state == eSTATE_RELEASED)
	{
		_profileBps = 0;
	}
	[self refreshStateLabel];
	BOOL playing = (state == eSTATE_PLAYING || state == eSTATE_BUFFERING);
	_playPause.image = [NSImage imageWithSystemSymbolName:(playing ? @"pause.fill" : @"play.fill")
		accessibilityDescription:@"Play / Pause"];
	_window.title = _engine.currentURL.length ? [NSString stringWithFormat:@"AAMP Player - %@ (%@)", _engine.currentURL.lastPathComponent, name] : @"AAMP Player";
	if (state == eSTATE_IDLE || state == eSTATE_STOPPED || state == eSTATE_COMPLETE || state == eSTATE_ERROR || state == eSTATE_RELEASED)
	{
		_seekSlider.doubleValue = 0;
		_positionSec = 0;
		_timeLabel.stringValue = @"0:00:00 / 0:00:00";
	}
}

- (void)refreshStateLabel
{
	NSString *rate = _profileBps > 0 ? [NSString stringWithFormat:@" | %.2f Mbps", _profileBps / 1e6] : @"";
	_stateLabel.stringValue = [(_stateName ?: @"IDLE") stringByAppendingString:rate];
}

- (void)engineProgressDetails:(NSDictionary<NSString *, id> *)d
{
	_profileBps = [d[@"profileBps"] longLongValue];
	[self refreshStateLabel];
	if (_overlay.hidden)
	{
		return;
	}
	_overlay.stringValue = [NSString stringWithFormat:
		@"video buffer : %.0f ms\naudio buffer : %.0f ms\nlive latency : %.0f ms\ntarget latency: %.0f ms\n"
		@"profile bw   : %.2f Mbps\nnetwork bw   : %.2f Mbps\nspeed        : %g (play rate %g)\npts          : %lld\nSEI timecode : %@",
		[d[@"videoBufMs"] doubleValue], [d[@"audioBufMs"] doubleValue],
		[d[@"liveLatencyMs"] doubleValue], [d[@"targetLatencyMs"] doubleValue],
		[d[@"profileBps"] doubleValue] / 1e6, [d[@"networkBps"] doubleValue] / 1e6,
		[d[@"speed"] doubleValue], [d[@"playRate"] doubleValue], [d[@"pts"] longLongValue], d[@"sei"]];
	[self layoutBars];
}

- (void)engineProgressPosition:(double)positionMs duration:(double)durationMs start:(double)startMs end:(double)endMs
{
	_positionSec = positionMs / 1000.0;
	if (_scrubbing)
	{
		return;
	}
	double startSec = startMs / 1000.0;
	double endSec = MAX(endMs / 1000.0, startSec + 1);
	_seekSlider.minValue = startSec;
	_seekSlider.maxValue = endSec;
	_seekSlider.doubleValue = _positionSec;
	_seekField.placeholderString = [NSString stringWithFormat:@"%.0f-%.0f s", startSec, endSec];
	_timeLabel.stringValue = [NSString stringWithFormat:@"%@ / %@", FormatTime(_positionSec), FormatTime(endSec)];
}

- (void)engineSpeedChanged:(float)rate
{
	NSString *title = [NSString stringWithFormat:@"%gx", rate];
	if ([_ratePopup itemWithTitle:title])
	{
		[_ratePopup selectItemWithTitle:title];
	}
}

- (void)engineMetadataWidth:(int)width height:(int)height durationMs:(long)durationMs
{
	_infoLabel.stringValue = [NSString stringWithFormat:@"%dx%d", width, height];
}

- (void)engineError:(NSString *)message
{
	_infoLabel.stringValue = message;
}

#pragma mark - NSWindowDelegate

- (void)windowDidResize:(NSNotification *)notification
{
	[self layoutBars];
}

- (void)windowWillClose:(NSNotification *)notification
{
	[NSApp terminate:nil];
}

@end
