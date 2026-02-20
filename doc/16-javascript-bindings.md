# JavaScript Bindings

## Overview

AAMP provides comprehensive JavaScript bindings that enable WebKit-based web applications to access AAMP's playback functionality through the UVE (Universal Video Engine) API. The bindings bridge the gap between JavaScript's dynamic, garbage-collected environment and AAMP's C++ implementation, providing type-safe, efficient access to player functionality while maintaining JavaScript's ease of use.

The JavaScript bindings use JavaScriptCore (WebKit's JavaScript engine) to create JavaScript objects that wrap C++ AAMP classes, enabling JavaScript code to call C++ methods, register event listeners, and access player state. The bindings handle memory management, type conversion, and event dispatching, ensuring seamless integration between JavaScript applications and AAMP's native implementation.

The UVE API provides a web-standard interface similar to HTML5 Media Element API, enabling web developers to use familiar patterns while leveraging AAMP's advanced adaptive streaming capabilities. The API supports modern JavaScript features (Promises, async/await) and provides comprehensive event handling for playback state changes, progress updates, and error reporting.

## Architecture

**Files**: `jsbindings/jsbindings.cpp`, `jsbindings/jsmediaplayer.cpp`, `jsbindings/jsevent.cpp`, `jsbindings/jseventlistener.cpp`, `jsbindings/jsutils.cpp`

**Purpose**: Bridge between JavaScript and C++ AAMP implementation

The JavaScript bindings architecture follows a layered design:

- **Binding Layer** (`jsbindings.cpp`): Core binding infrastructure that registers JavaScript classes, methods, and properties with JavaScriptCore. Handles JavaScript object creation, method invocation, property access, and garbage collection callbacks. Provides foundation for all JavaScript-C++ interoperation.

- **Player Binding** (`jsmediaplayer.cpp`): JavaScript wrapper for `PlayerInstanceAAMP` that exposes player methods (load, play, pause, seek, etc.) to JavaScript. Handles method parameter conversion (JavaScript types to C++ types), method invocation, and return value conversion (C++ types to JavaScript types). Manages JavaScript object lifecycle and ensures C++ object lifetime matches JavaScript object lifetime.

- **Event Binding** (`jsevent.cpp`, `jseventlistener.cpp`): JavaScript event system that converts C++ events (`AAMPEvent`) to JavaScript event objects and dispatches them to registered JavaScript listeners. Handles event listener registration, event object creation, and event data conversion. Provides JavaScript event API compatible with web standards (addEventListener, removeEventListener, event properties).

- **Utility Bindings** (`jsutils.cpp`): Utility functions for JavaScript-C++ interoperation, including type conversion helpers, error handling, and debugging utilities. Provides common functionality used across all binding components.

## UVE API

The UVE (Universal Video Engine) API provides JavaScript access to AAMP functionality through a web-standard interface:

```javascript
var player = new AAMPMediaPlayer();
player.addEventListener('progress', function(event) {
    console.log('Position: ' + event.currentPosition);
});
player.load('http://example.com/manifest.mpd');
```

**API Design Principles**:
- **Web Standard Compatibility**: UVE API follows web media element patterns, making it familiar to web developers. Methods and events align with HTML5 Media Element API where applicable, reducing learning curve.
- **Asynchronous Operations**: Operations that may take time (load, seek) support both callback and Promise-based APIs, enabling modern JavaScript async/await patterns. Callbacks provide compatibility with older JavaScript code.
- **Event-Driven Architecture**: Comprehensive event system provides notifications for all playback state changes, progress updates, errors, and metadata. Events use standard web event patterns (addEventListener, event objects with properties).
- **Type Safety**: JavaScript bindings perform runtime type checking and conversion, ensuring JavaScript values are correctly converted to C++ types. Invalid types trigger JavaScript exceptions, providing clear error messages.

**Core API Methods**:
- **`load(url, options)`**: Loads and starts playback of a media stream. URL can be HLS (.m3u8), DASH (.mpd), or progressive MP4. Options object allows configuration (autoplay, initial bitrate, etc.). Returns Promise that resolves when playback starts or rejects on error.
- **`play()`**: Resumes paused playback. Returns Promise.
- **`pause()`**: Pauses playback at current position. Synchronous operation.
- **`seek(position)`**: Seeks to specified time position in seconds. Returns Promise that resolves when seek completes.
- **`setPlaybackRate(rate)`**: Sets playback speed (1.0 = normal, 2.0 = 2x speed, etc.). Supports trick play for VOD content.
- **`setVolume(volume)`**: Sets audio volume (0.0 to 1.0). Synchronous operation.
- **`addEventListener(type, listener)`**: Registers event listener for specified event type. Listener receives event object with event-specific properties.
- **`removeEventListener(type, listener)`**: Removes previously registered event listener.
- **`getVideoPlaybackQuality()`**: Returns video quality metrics (droppedFrames, totalVideoFrames, etc.) for performance monitoring.

## Key Components

### JSMediaPlayer

`JSMediaPlayer` is the JavaScript wrapper for `PlayerInstanceAAMP` that exposes player functionality to JavaScript:

- **Method Exposure**: JavaScript methods (load, play, pause, seek, etc.) are bound to corresponding `PlayerInstanceAAMP` C++ methods. Method binding uses JavaScriptCore's `JSObjectSetPropertyWithCFString()` and function callbacks to create JavaScript functions that invoke C++ methods. Parameters are converted from JavaScript types (numbers, strings, objects) to C++ types (int, std::string, structs) before method invocation.

- **Event Callback Handling**: JavaScript event listeners registered via `addEventListener()` are stored in JavaScriptCore and invoked when corresponding C++ events occur. Event dispatching converts C++ event objects (`AAMPEvent`) to JavaScript event objects with appropriate properties. Event listeners receive event objects compatible with web event standards.

- **Object Lifecycle Management**: JavaScript objects maintain references to C++ `PlayerInstanceAAMP` instances via smart pointers (`std::shared_ptr`). When JavaScript objects are garbage collected, JavaScriptCore finalizers release C++ object references, ensuring proper cleanup. C++ objects are kept alive as long as JavaScript objects reference them, preventing premature destruction.

**JavaScript Object Structure**: `JSMediaPlayer` creates JavaScript objects with:
- **Methods**: Bound C++ methods accessible as JavaScript functions
- **Properties**: Read-only properties (duration, currentTime, readyState) that reflect C++ player state
- **Event Target**: Implements EventTarget interface for addEventListener/removeEventListener

### JSEvent

`JSEvent` converts C++ events to JavaScript event objects:

- **Event Object Creation**: When C++ events occur (`AAMP_EVENT_PROGRESS`, `AAMP_EVENT_BITRATE_CHANGED`, etc.), `JSEvent` creates JavaScript event objects with properties matching event data. Event objects are created using JavaScriptCore APIs (`JSObjectMake()`, `JSObjectSetProperty()`) and populated with event-specific properties.

- **Event Data Access**: JavaScript event objects expose event data as properties:
  - **Progress Events**: `currentPosition`, `duration`, `playbackSpeed`
  - **Bitrate Events**: `bitrate`, `width`, `height`, `profileIndex`
  - **State Events**: `state` (string representation of player state)
  - **Error Events**: `errorCode`, `errorMessage`, `errorDescription`
  - **Metadata Events**: `metadata` (object containing metadata properties)

- **Event Type Identification**: JavaScript event objects include `type` property identifying the event type (string matching C++ event type enum). Event types use web-standard names where applicable ("progress", "ended", "error") or AAMP-specific names for advanced features ("bitratechanged", "timedmetadata").

**Event Object Lifecycle**: Event objects are created when events occur and passed to registered listeners. JavaScript code can access event properties synchronously during listener execution. Event objects are garbage collected after listener execution completes, unless JavaScript code retains references.

### JSEventListener

`JSEventListener` manages JavaScript event listener registration and dispatching:

- **Listener Registration**: `addEventListener(type, listener)` stores JavaScript function references in listener maps keyed by event type. Multiple listeners can be registered for the same event type, and listeners are invoked in registration order. Listener storage uses JavaScriptCore's value retention APIs to prevent garbage collection of listener functions.

- **Event Dispatching**: When C++ events occur, `JSEventListener` retrieves registered listeners for the event type and invokes them with JavaScript event objects. Listener invocation uses JavaScriptCore's `JSObjectCallAsFunction()` to call JavaScript functions with event objects as arguments. Listener execution occurs synchronously during event dispatch, ensuring event order and immediate notification.

- **Listener Removal**: `removeEventListener(type, listener)` removes listener references from listener maps, preventing future invocations. Listener removal requires exact function reference matching (same JavaScript function object), matching web standard behavior. Removed listeners are released from JavaScriptCore retention, allowing garbage collection.

**Listener Invocation**: Event listeners are invoked with `this` context set to the player object, enabling listeners to access player methods and properties. Listener exceptions are caught and logged, preventing listener errors from disrupting event dispatching or player operation.

## Integration

### WebKit Integration

For WPE WebKit (RDK's web runtime), JavaScript bindings are integrated as an injected bundle:

- **Injected Bundle**: JavaScript bindings are compiled as a WebKit injected bundle (`libaampjsbindings.so`) that is loaded by WebKit when web pages require AAMP functionality. Bundle injection occurs via WebKit's bundle loading mechanism, making AAMP API available to all web pages in the WebKit instance.

- **JavaScriptCore API**: Bindings use JavaScriptCore's C API (`JSContextRef`, `JSObjectRef`, `JSValueRef`) to create JavaScript objects, register functions, and interact with JavaScript execution environment. JavaScriptCore provides the foundation for JavaScript-C++ interoperation, handling memory management, garbage collection, and JavaScript execution.

- **Native Object Binding**: C++ `PlayerInstanceAAMP` objects are wrapped as JavaScript objects using JavaScriptCore's object creation APIs. JavaScript objects maintain references to C++ objects via JavaScriptCore's private data mechanism (`JSObjectSetPrivate()`, `JSObjectGetPrivate()`), enabling bidirectional access between JavaScript and C++.

**Bundle Initialization**: When WebKit loads the injected bundle, bundle initialization code (`JSObjectRef AAMPJSBindingsInitialize(JSContextRef ctx)`) creates global JavaScript objects (`AAMPMediaPlayer` constructor) and registers them with the JavaScript execution context. Initialization occurs once per WebKit instance, making AAMP API available to all web pages.

### Standalone Integration

For non-WebKit environments (testing, development, custom JavaScript runtimes):

- **API Wrapper Library**: JavaScript bindings can be compiled as a standalone library that provides AAMP API to custom JavaScript runtimes. The library exports initialization functions that create JavaScript objects in provided JavaScript contexts, enabling integration with any JavaScript engine supporting JavaScriptCore-compatible APIs.

- **Event Simulation**: Standalone integration may include event simulation capabilities for testing scenarios. Event simulation allows test code to trigger AAMP events programmatically, enabling comprehensive testing of JavaScript event handling without requiring actual media playback.

**Standalone Usage**: Standalone bindings are useful for:
- **Unit Testing**: Testing JavaScript application code that uses AAMP API without requiring full WebKit environment
- **Development Tools**: Command-line tools or development environments that need AAMP API access
- **Custom Runtimes**: Integration with custom JavaScript runtimes or embedded JavaScript engines

## Summary

JavaScript bindings provide comprehensive web integration capabilities:

- **Web Integration**: Seamless integration with WebKit-based web applications enables web developers to use AAMP's advanced streaming capabilities in web applications. The bindings provide web-standard APIs that integrate naturally with web development workflows and tools.

- **UVE API Exposure**: Complete exposure of AAMP functionality through UVE API enables web applications to leverage adaptive streaming, DRM, trick play, and advanced features. The API provides both simple interfaces for basic playback and advanced interfaces for sophisticated use cases.

- **Event Handling**: Comprehensive event system provides real-time notifications for all playback aspects, enabling responsive web applications. Event handling follows web standards, making it familiar to web developers and compatible with web development patterns.

- **Cross-Platform Support**: JavaScript bindings work across platforms (RDK, Linux, macOS) where WebKit is available, providing consistent API regardless of underlying platform. Platform-specific optimizations are transparent to JavaScript code, ensuring portable web applications.
