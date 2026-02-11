# JavaScript Bindings

## Overview

AAMP provides JavaScript bindings for WebKit integration, exposing the UVE (Universal Video Engine) API.

## Architecture

**Files**: `jsbindings/jsbindings.cpp`, `jsbindings/jsmediaplayer.cpp`, etc.

**Purpose**: Bridge between JavaScript and C++ AAMP implementation

## UVE API

The UVE API provides JavaScript access to AAMP functionality:

```javascript
var player = new AAMPMediaPlayer();
player.addEventListener('progress', function(event) {
    console.log('Position: ' + event.currentPosition);
});
player.load('http://example.com/manifest.mpd');
```

## Key Components

### JSMediaPlayer

JavaScript wrapper for `PlayerInstanceAAMP`:
- Exposes player methods to JavaScript
- Handles event callbacks
- Manages object lifecycle

### JSEvent

JavaScript event objects:
- Convert C++ events to JavaScript
- Provide event data access

### JSEventListener

JavaScript event listener registration:
- Register JavaScript callbacks
- Dispatch events to JavaScript

## Integration

### WebKit Integration

For WPE WebKit:
- Injected bundle
- JavaScriptCore API
- Native object binding

### Standalone

For non-WebKit environments:
- API wrapper library
- Event simulation

## Summary

JavaScript bindings provide:
- Web integration
- UVE API exposure
- Event handling
- Cross-platform support
