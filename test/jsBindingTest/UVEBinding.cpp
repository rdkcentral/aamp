/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#include <gst/gst.h>
#include <JavaScriptCore/JavaScriptCore.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>

// AAMP JS registration
extern "C" void aamp_LoadJSController(JSGlobalContextRef context);

static GMainLoop* gMainLoop = nullptr;

// ---------------------------------------------------------------------------
// setTimeout / clearTimeout implementation backed by GLib timers
// ---------------------------------------------------------------------------

struct TimeoutData {
    JSGlobalContextRef ctx;
    JSObjectRef        callback;
    guint              timerId;
};

static std::map<guint, TimeoutData*> gPendingTimeouts;
static std::mutex                    gTimeoutMutex;

static gboolean timeout_callback(gpointer userData)
{
    TimeoutData* data = static_cast<TimeoutData*>(userData);

    {
        std::lock_guard<std::mutex> lock(gTimeoutMutex);
        gPendingTimeouts.erase(data->timerId);
    }

    JSValueRef exc = nullptr;
    JSObjectCallAsFunction(data->ctx, data->callback, nullptr, 0, nullptr, &exc);
    JSValueUnprotect(data->ctx, data->callback);
    delete data;
    return G_SOURCE_REMOVE;
}

static JSValueRef js_set_timeout(
    JSContextRef ctx,
    JSObjectRef  /*function*/,
    JSObjectRef  /*thisObject*/,
    size_t argumentCount,
    const JSValueRef arguments[],
    JSValueRef* exception)
{
    if (argumentCount < 1 || !JSValueIsObject(ctx, arguments[0])) {
        return JSValueMakeNumber(ctx, 0);
    }

    JSObjectRef cb = JSValueToObject(ctx, arguments[0], exception);
    if (!cb) { return JSValueMakeNumber(ctx, 0); }

    unsigned int delay = 0;
    if (argumentCount >= 2) {
        delay = static_cast<unsigned int>(JSValueToNumber(ctx, arguments[1], exception));
    }

    JSGlobalContextRef globalCtx = JSContextGetGlobalContext(ctx);
    JSValueProtect(globalCtx, cb);

    TimeoutData* data = new TimeoutData{globalCtx, cb, 0};
    guint timerId = g_timeout_add(delay, timeout_callback, data);
    data->timerId = timerId;

    {
        std::lock_guard<std::mutex> lock(gTimeoutMutex);
        gPendingTimeouts[timerId] = data;
    }

    return JSValueMakeNumber(ctx, timerId);
}

static JSValueRef js_clear_timeout(
    JSContextRef ctx,
    JSObjectRef  /*function*/,
    JSObjectRef  /*thisObject*/,
    size_t argumentCount,
    const JSValueRef arguments[],
    JSValueRef* exception)
{
    if (argumentCount >= 1) {
        guint timerId = static_cast<guint>(JSValueToNumber(ctx, arguments[0], exception));

        TimeoutData* data = nullptr;
        {
            std::lock_guard<std::mutex> lock(gTimeoutMutex);
            auto it = gPendingTimeouts.find(timerId);
            if (it != gPendingTimeouts.end()) {
                data = it->second;
                gPendingTimeouts.erase(it);
            }
        }

        if (data) {
            g_source_remove(timerId);
            JSValueUnprotect(data->ctx, data->callback);
            delete data;
        }
    }
    return JSValueMakeUndefined(ctx);
}

/**
 * Browser API polyfill injected before the user script.
 * Provides stubs for DOM/window APIs not available in the JavaScriptCore
 * CLI environment so that l3.js test scripts can run unmodified.
 */
static const char* kBrowserPolyfill = R"JS(
// Stub window.location so URLSearchParams can read query params (always empty in CLI)
window.location = { search: "" };

// Stub window.addEventListener (error / unhandledrejection hooks are no-ops in CLI)
window.addEventListener = function(type, listener, options) {};

// Minimal URLSearchParams implementation
function URLSearchParams(search) {
    this._params = {};
    if (typeof search === "string") {
        if (search.charAt(0) === "?") { search = search.slice(1); }
        if (search.length > 0) {
            var pairs = search.split("&");
            for (var i = 0; i < pairs.length; i++) {
                var idx = pairs[i].indexOf("=");
                if (idx >= 0) {
                    this._params[decodeURIComponent(pairs[i].slice(0, idx))] =
                        decodeURIComponent(pairs[i].slice(idx + 1));
                } else {
                    this._params[decodeURIComponent(pairs[i])] = "";
                }
            }
        }
    }
}
URLSearchParams.prototype.get = function(key) {
    return Object.prototype.hasOwnProperty.call(this._params, key) ? this._params[key] : null;
};

// Minimal document stub — getElementById returns an inert object whose
// innerHTML setter is a no-op (UI updates are silently ignored in CLI)
var document = {
    getElementById: function(id) {
        return { get innerHTML() { return ""; }, set innerHTML(v) {} };
    }
};
)JS";

/**
 * Convert JSStringRef to std::string (C++14 safe)
 */
static std::string JSStringToStdString(JSStringRef str)
{
    size_t maxSize = JSStringGetMaximumUTF8CStringSize(str);
    std::vector<char> buffer(maxSize);
    JSStringGetUTF8CString(str, buffer.data(), maxSize);
    return std::string(buffer.data());
}

static JSValueRef js_console_log(
    JSContextRef ctx,
    JSObjectRef /*function*/,
    JSObjectRef /*thisObject*/,
    size_t argumentCount,
    const JSValueRef arguments[],
    JSValueRef* exception)
{
    for (size_t i = 0; i < argumentCount; i++) {
        JSStringRef str = JSValueToStringCopy(ctx, arguments[i], exception);
        std::string out = JSStringToStdString(str);
        JSStringRelease(str);

        std::cout << out;
        if (i + 1 < argumentCount) {
            std::cout << " ";
        }
    }

    std::cout << std::endl;
    return JSValueMakeUndefined(ctx);
}

static void installConsole(JSGlobalContextRef ctx)
{
    JSObjectRef global = JSContextGetGlobalObject(ctx);

    // window = global
    JSStringRef windowName = JSStringCreateWithUTF8CString("window");
    JSObjectSetProperty(ctx, global, windowName, global,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(windowName);

    // navigator object
    JSObjectRef navigator = JSObjectMake(ctx, nullptr, nullptr);
    JSStringRef navName = JSStringCreateWithUTF8CString("navigator");
    JSObjectSetProperty(ctx, global, navName, navigator,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(navName);

    // navigator.userAgent
    JSStringRef uaName = JSStringCreateWithUTF8CString("userAgent");
    JSStringRef uaValue = JSStringCreateWithUTF8CString(
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AAMP/1.0");
    JSObjectSetProperty(ctx, navigator, uaName,
                        JSValueMakeString(ctx, uaValue),
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(uaName);
    JSStringRelease(uaValue);

    // console object
    JSObjectRef console = JSObjectMake(ctx, nullptr, nullptr);

    JSStringRef logName = JSStringCreateWithUTF8CString("log");
    JSObjectRef logFunc =
        JSObjectMakeFunctionWithCallback(ctx, logName, js_console_log);
    JSObjectSetProperty(ctx, console, logName, logFunc,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(logName);

    JSStringRef consoleName = JSStringCreateWithUTF8CString("console");
    JSObjectSetProperty(ctx, global, consoleName, console,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(consoleName);

    // setTimeout
    JSStringRef setTimeoutName = JSStringCreateWithUTF8CString("setTimeout");
    JSObjectRef setTimeoutFunc = JSObjectMakeFunctionWithCallback(ctx, setTimeoutName, js_set_timeout);
    JSObjectSetProperty(ctx, global, setTimeoutName, setTimeoutFunc,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(setTimeoutName);

    // clearTimeout
    JSStringRef clearTimeoutName = JSStringCreateWithUTF8CString("clearTimeout");
    JSObjectRef clearTimeoutFunc = JSObjectMakeFunctionWithCallback(ctx, clearTimeoutName, js_clear_timeout);
    JSObjectSetProperty(ctx, global, clearTimeoutName, clearTimeoutFunc,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(clearTimeoutName);
}

static int main_func(int argc, char** argv)
{

    gst_init(&argc, &argv);

    if (argc < 2) {
        std::cerr << "Usage: ./jsbind <script.js>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Failed to open JS file\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    installConsole(ctx);

    // Register AAMP bindings
    aamp_LoadJSController(ctx);

    // Combine the browser-API polyfill with the user script.
    // Wrap in an async IIFE so that top-level `await` expressions in the
    // user script (e.g. `await TST_2000_UVE_AampPlayback()`) work correctly
    // under JSEvaluateScript, which does not support module-level top-level await.
    std::string combined =
        std::string(kBrowserPolyfill) +
        "\n(async function() {\n" +
        script +
        "\n})().catch(function(e) { console.log('Unhandled error: ' + e); });\n";

    JSStringRef jsSource = JSStringCreateWithUTF8CString(combined.c_str());
    JSEvaluateScript(ctx, jsSource, nullptr, nullptr, 1, nullptr);
    JSStringRelease(jsSource);

    gMainLoop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(gMainLoop);

    // Cleanup (only reached if loop exits)
    g_main_loop_unref(gMainLoop);
    JSGlobalContextRelease(ctx);

    return 0;
}

int main(int argc, char** argv)
{
#if defined(__APPLE__) && defined (__GST_MACOS_H__)
	return gst_macos_main((GstMainFunc)main_func, argc, argv, NULL);
#else
    return main_func(argc, argv);
#endif
}
