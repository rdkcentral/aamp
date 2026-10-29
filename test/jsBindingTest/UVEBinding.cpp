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
#ifdef __APPLE__
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

#include <curl/curl.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

// AAMP JS registration
extern "C" void aamp_LoadJSController(JSGlobalContextRef context);
extern "C" void aamp_UnloadJSController(JSGlobalContextRef context);

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

// ---------------------------------------------------------------------------
// setInterval / clearInterval implementation backed by repeating GLib timers
// ---------------------------------------------------------------------------

static std::map<guint, TimeoutData*> gPendingIntervals;

static gboolean interval_callback(gpointer userData)
{
    TimeoutData* data = static_cast<TimeoutData*>(userData);
    JSValueRef exc = nullptr;
    JSObjectCallAsFunction(data->ctx, data->callback, nullptr, 0, nullptr, &exc);
    return G_SOURCE_CONTINUE;
}

static JSValueRef js_set_interval(
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
    data->timerId = g_timeout_add(delay, interval_callback, data);
    gPendingIntervals[data->timerId] = data;
    return JSValueMakeNumber(ctx, data->timerId);
}

static JSValueRef js_clear_interval(
    JSContextRef ctx,
    JSObjectRef  /*function*/,
    JSObjectRef  /*thisObject*/,
    size_t argumentCount,
    const JSValueRef arguments[],
    JSValueRef* exception)
{
    if (argumentCount >= 1) {
        guint timerId = static_cast<guint>(JSValueToNumber(ctx, arguments[0], exception));
        auto it = gPendingIntervals.find(timerId);
        if (it != gPendingIntervals.end()) {
            TimeoutData* data = it->second;
            gPendingIntervals.erase(it);
            g_source_remove(timerId);
            JSValueUnprotect(data->ctx, data->callback);
            delete data;
        }
    }
    return JSValueMakeUndefined(ctx);
}

/**
 * Remove outstanding intervals; must run on the main thread after the loop
 * exits and before the JS context is released.
 */
static void interval_shutdown()
{
    for (auto& entry : gPendingIntervals) {
        g_source_remove(entry.first);
        JSValueUnprotect(entry.second->ctx, entry.second->callback);
        delete entry.second;
    }
    gPendingIntervals.clear();
}

// ---------------------------------------------------------------------------
// __httpRequest__(method, url, headerLines, body, callback) backing fetch().
// libcurl runs on a worker thread; the callback runs on the GLib main loop.
// ---------------------------------------------------------------------------

static const long kHttpTimeoutMs = 30000;

struct HttpRequest {
    JSGlobalContextRef ctx = nullptr;
    JSObjectRef        callback = nullptr;
    std::string        method;
    std::string        url;
    std::string        headerLines;
    std::string        body;
    long               status = 0;
    std::string        response;
    std::string        effectiveUrl;
    std::string        error;
};

// gHttpWorkers is only touched on the main thread; workers hand results back via gHttpDone.
static std::map<HttpRequest*, std::thread> gHttpWorkers;
static std::vector<HttpRequest*>           gHttpDone;
static std::mutex                          gHttpDoneMutex;
static std::atomic<bool>                   gHttpShutdown{false};

static std::string JSValueToStdString(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    JSStringRef str = JSValueToStringCopy(ctx, value, exception);
    if (!str) { return std::string(); }
    size_t maxSize = JSStringGetMaximumUTF8CStringSize(str);
    std::vector<char> buffer(maxSize);
    JSStringGetUTF8CString(str, buffer.data(), maxSize);
    JSStringRelease(str);
    return std::string(buffer.data());
}

static size_t http_write(char* ptr, size_t size, size_t nmemb, void* userData)
{
    static_cast<std::string*>(userData)->append(ptr, size * nmemb);
    return size * nmemb;
}

static int http_progress(void* /*clientp*/, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                         curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
    return gHttpShutdown ? 1 : 0;
}

static void http_deliver(HttpRequest* req)
{
    JSGlobalContextRef ctx = req->ctx;

    JSStringRef responseStr = JSStringCreateWithUTF8CString(req->response.c_str());
    JSStringRef effectiveUrlStr = JSStringCreateWithUTF8CString(req->effectiveUrl.c_str());
    JSValueRef args[4];
    args[0] = JSValueMakeNumber(ctx, static_cast<double>(req->status));
    args[1] = JSValueMakeString(ctx, responseStr);
    if (req->error.empty()) {
        args[2] = JSValueMakeUndefined(ctx);
    } else {
        JSStringRef errorStr = JSStringCreateWithUTF8CString(req->error.c_str());
        args[2] = JSValueMakeString(ctx, errorStr);
        JSStringRelease(errorStr);
    }
    args[3] = JSValueMakeString(ctx, effectiveUrlStr);
    JSStringRelease(responseStr);
    JSStringRelease(effectiveUrlStr);

    JSValueRef exc = nullptr;
    JSObjectCallAsFunction(ctx, req->callback, nullptr, 4, args, &exc);
    JSValueUnprotect(ctx, req->callback);
    delete req;
}

static gboolean http_drain(gpointer /*userData*/)
{
    std::vector<HttpRequest*> done;
    {
        std::lock_guard<std::mutex> lock(gHttpDoneMutex);
        done.swap(gHttpDone);
    }
    for (HttpRequest* req : done) {
        auto it = gHttpWorkers.find(req);
        if (it != gHttpWorkers.end()) {
            it->second.join();
            gHttpWorkers.erase(it);
        }
        http_deliver(req);
    }
    return G_SOURCE_REMOVE;
}

static void http_finish(HttpRequest* req)
{
    {
        std::lock_guard<std::mutex> lock(gHttpDoneMutex);
        gHttpDone.push_back(req);
    }
    g_idle_add(http_drain, nullptr);
}

/**
 * Cancel and join outstanding requests; must run on the main thread after the
 * loop exits and before the JS context is released.
 */
static void http_shutdown()
{
    gHttpShutdown = true;
    for (auto& worker : gHttpWorkers) {
        worker.second.join();
        JSValueUnprotect(worker.first->ctx, worker.first->callback);
        delete worker.first;
    }
    gHttpWorkers.clear();
    std::lock_guard<std::mutex> lock(gHttpDoneMutex);
    gHttpDone.clear();
}

static void http_worker(HttpRequest* req)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        req->error = "curl_easy_init failed";
        http_finish(req);
        return;
    }

    struct curl_slist* headers = nullptr;
    std::istringstream lines(req->headerLines);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty()) {
            headers = curl_slist_append(headers, line.c_str());
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, req->url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    if (req->method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else if (req->method != "GET") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(req->body.size()));
        if (req->method != "POST") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req->method.c_str());
        }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req->response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, kHttpTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, http_progress);

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &req->status);
        char* effectiveUrl = nullptr;
        if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl) == CURLE_OK && effectiveUrl) {
            req->effectiveUrl = effectiveUrl;
        }
    } else {
        req->error = curl_easy_strerror(rc);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_finish(req);
}

static JSValueRef js_http_request(
    JSContextRef ctx,
    JSObjectRef  /*function*/,
    JSObjectRef  /*thisObject*/,
    size_t argumentCount,
    const JSValueRef arguments[],
    JSValueRef* exception)
{
    if (argumentCount < 5 || !JSValueIsObject(ctx, arguments[4])) {
        return JSValueMakeUndefined(ctx);
    }
    JSObjectRef cb = JSValueToObject(ctx, arguments[4], exception);
    if (!cb || !JSObjectIsFunction(ctx, cb)) {
        return JSValueMakeUndefined(ctx);
    }

    JSGlobalContextRef globalCtx = JSContextGetGlobalContext(ctx);
    HttpRequest* req = new HttpRequest();
    req->ctx         = globalCtx;
    req->callback    = cb;
    req->method      = JSValueToStdString(ctx, arguments[0], exception);
    req->url         = JSValueToStdString(ctx, arguments[1], exception);
    req->headerLines = JSValueToStdString(ctx, arguments[2], exception);
    req->body        = JSValueToStdString(ctx, arguments[3], exception);
    JSValueProtect(globalCtx, cb);

    gHttpWorkers.emplace(req, std::thread(http_worker, req));
    return JSValueMakeUndefined(ctx);
}

/**
 * Browser API polyfill injected before the user script.
 * Provides stubs for DOM/window APIs not available in the JavaScriptCore
 * CLI environment so that l3.js test scripts can run unmodified.
 */
static const char* kBrowserPolyfill = R"JS(
// Stub window.location so URLSearchParams can read query params (always empty in CLI).
// Include href so that `new URL(window.location.href)` does not throw.
window.location = { search: "", href: "http://localhost/" };

// window.addEventListener — keep a real listener registry so that
// unhandledrejection and error handlers registered by TST_UVE_utils.js fire.
var _windowListeners = {};
window.addEventListener = function(type, listener, options) {
    if (!_windowListeners[type]) { _windowListeners[type] = []; }
    _windowListeners[type].push(listener);
};
window.removeEventListener = function(type, listener) {
    if (_windowListeners[type]) {
        _windowListeners[type] = _windowListeners[type].filter(function(l) { return l !== listener; });
    }
};
function __dispatchWindowEvent__(type, eventObj) {
    var listeners = _windowListeners[type] || [];
    for (var i = 0; i < listeners.length; i++) {
        try { listeners[i](eventObj); } catch(e) {}
    }
}

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
URLSearchParams.prototype.has = function(key) {
    return Object.prototype.hasOwnProperty.call(this._params, key);
};

// Minimal URL implementation — enough for `new URL(window.location.href)` and
// reading searchParams in test utilities like TST_UVE_vidcap.js.
function URL(href) {
    if (typeof href !== "string") { href = String(href); }
    // Extract the search string (everything after the first '?', before any '#')
    var qIdx = href.indexOf("?");
    var hIdx = href.indexOf("#");
    var search = "";
    if (qIdx >= 0) {
        search = hIdx >= 0 ? href.slice(qIdx, hIdx) : href.slice(qIdx);
    }
    this.href = href;
    this.search = search;
    this.searchParams = new URLSearchParams(search);
}

// Minimal document stub — elements are inert objects whose innerHTML setter
// is a no-op (UI updates are silently ignored in CLI). Enough of the element
// API is stubbed for browser libraries (e.g. PPJS) that touch the DOM.
function __inertElement__(tag) {
    return {
        tagName: String(tag || "div").toUpperCase(),
        style: {},
        dataset: {},
        children: [],
        attributes: {},
        classList: { add: function() {}, remove: function() {}, toggle: function() {}, contains: function() { return false; } },
        get innerHTML() { return ""; }, set innerHTML(v) {},
        innerText: "", textContent: "",
        get firstChild() { return this.children[0] || null; },
        appendChild: function(c) { this.children.push(c); c.parentNode = this; return c; },
        insertBefore: function(c) { this.children.push(c); c.parentNode = this; return c; },
        removeChild: function(c) { this.children = this.children.filter(function(x) { return x !== c; }); return c; },
        setAttribute: function(k, v) { this.attributes[k] = String(v); },
        getAttribute: function(k) { return Object.prototype.hasOwnProperty.call(this.attributes, k) ? this.attributes[k] : null; },
        removeAttribute: function(k) { delete this.attributes[k]; },
        addEventListener: function() {}, removeEventListener: function() {}, dispatchEvent: function() { return true; },
        getBoundingClientRect: function() { return { x: 0, y: 0, top: 0, left: 0, width: 1280, height: 720 }; },
        querySelector: function() { return null; }, querySelectorAll: function() { return []; },
        getElementsByTagName: function() { return []; },
        offsetWidth: 1280, offsetHeight: 720, clientWidth: 1280, clientHeight: 720
    };
}
var document = {
    getElementById: function(id) { return __inertElement__("div"); },
    createElement: function(tag) { return __inertElement__(tag); },
    createTextNode: function(text) { return { textContent: String(text) }; },
    getElementsByTagName: function(tag) {
        tag = String(tag).toLowerCase();
        return tag === "head" ? [document.head] : tag === "body" ? [document.body] : [];
    },
    querySelector: function() { return null; },
    querySelectorAll: function() { return []; },
    addEventListener: function() {},
    removeEventListener: function() {},
    readyState: "complete",
    hidden: false,
    visibilityState: "visible"
};
document.head = __inertElement__("head");
document.body = __inertElement__("body");
document.documentElement = __inertElement__("html");

// Browser globals expected by UMD/webpack bundles and browser SDKs (e.g. PPJS).
// NOTE: do not polyfill queueMicrotask or MutationObserver here. core-js then
// falls back to native promises; with those polyfilled its promise jobs spin forever.
var self = window;

if (typeof performance === "undefined") {
    var __perfOrigin__ = Date.now();
    var performance = {
        timeOrigin: __perfOrigin__,
        timing: { navigationStart: __perfOrigin__ },
        now: function() { return Date.now() - __perfOrigin__; },
        mark: function() {}, measure: function() {}, clearMarks: function() {}, clearMeasures: function() {},
        getEntries: function() { return []; }, getEntriesByName: function() { return []; }, getEntriesByType: function() { return []; }
    };
}

if (typeof localStorage === "undefined") {
    var __makeStorage__ = function() {
        var data = {};
        return {
            getItem: function(k) { return Object.prototype.hasOwnProperty.call(data, k) ? data[k] : null; },
            setItem: function(k, v) { data[k] = String(v); },
            removeItem: function(k) { delete data[k]; },
            clear: function() { data = {}; }
        };
    };
    var localStorage = __makeStorage__();
    var sessionStorage = __makeStorage__();
}

if (typeof Event === "undefined") {
    var Event = function Event(type, init) {
        init = init || {};
        this.type = type;
        this.bubbles = !!init.bubbles;
        this.cancelable = !!init.cancelable;
        this.defaultPrevented = false;
    };
    Event.prototype.preventDefault = function() { this.defaultPrevented = true; };
    Event.prototype.stopPropagation = function() {};
    Event.prototype.stopImmediatePropagation = function() {};
    var CustomEvent = function CustomEvent(type, init) {
        Event.call(this, type, init);
        this.detail = init ? init.detail : undefined;
    };
    CustomEvent.prototype = Object.create(Event.prototype);
}

if (typeof EventTarget === "undefined") {
    var EventTarget = function EventTarget() {};
    EventTarget.prototype.addEventListener = function(type, listener) {
        this.__listeners__ = this.__listeners__ || {};
        (this.__listeners__[type] = this.__listeners__[type] || []).push(listener);
    };
    EventTarget.prototype.removeEventListener = function(type, listener) {
        if (this.__listeners__ && this.__listeners__[type]) {
            this.__listeners__[type] = this.__listeners__[type].filter(function(l) { return l !== listener; });
        }
    };
    EventTarget.prototype.dispatchEvent = function(event) {
        var target = this;
        ((this.__listeners__ && this.__listeners__[event.type]) || []).slice().forEach(function(l) {
            if (typeof l === "function") { l.call(target, event); } else { l.handleEvent(event); }
        });
        return !event.defaultPrevented;
    };
}

// Minimal XMLHttpRequest over native __httpRequest__ (text bodies only, no response headers).
if (typeof XMLHttpRequest === "undefined") {
    var XMLHttpRequest = function XMLHttpRequest() {
        this.readyState = 0;
        this.status = 0;
        this.responseText = "";
        this.response = "";
        this._headers = {};
    };
    XMLHttpRequest.prototype.open = function(method, url) { this._method = method; this._url = String(url); this.readyState = 1; };
    XMLHttpRequest.prototype.setRequestHeader = function(name, value) { this._headers[name] = value; };
    XMLHttpRequest.prototype.getAllResponseHeaders = function() { return ""; };
    XMLHttpRequest.prototype.getResponseHeader = function() { return null; };
    XMLHttpRequest.prototype.abort = function() { this._aborted = true; };
    XMLHttpRequest.prototype.send = function(body) {
        var xhr = this;
        var headerLines = [];
        for (var name in xhr._headers) { headerLines.push(name + ": " + xhr._headers[name]); }
        __httpRequest__(String(xhr._method || "GET").toUpperCase(), xhr._url, headerLines.join("\n"),
            (body === undefined || body === null) ? "" : String(body),
            function(status, text, error, effectiveUrl) {
                if (xhr._aborted) { return; }
                xhr.readyState = 4;
                xhr.status = error !== undefined ? 0 : status;
                xhr.responseText = xhr.response = text;
                xhr.responseURL = effectiveUrl || xhr._url;
                if (xhr.onreadystatechange) { xhr.onreadystatechange(); }
                if (error !== undefined) {
                    if (xhr.onerror) { xhr.onerror({ type: "error" }); }
                } else if (xhr.onload) {
                    xhr.onload({ type: "load" });
                }
            });
    };
}

// Inert constructors for browser classes some bundles reference at module load time.
["DOMParser", "XMLSerializer", "Blob", "Worker", "MediaSource", "SourceBuffer", "HTMLElement",
 "HTMLVideoElement", "HTMLMediaElement", "Image", "FileReader", "TextTrackCue", "VTTCue", "WebSocket",
 "ResizeObserver", "IntersectionObserver", "AbortController"].forEach(function(name) {
    if (typeof window[name] === "undefined") {
        var Ctor = function() {};
        Ctor.prototype.parseFromString = function() {
            return { documentElement: null, getElementsByTagName: function() { return []; } };
        };
        Ctor.prototype.observe = Ctor.prototype.disconnect = Ctor.prototype.abort = function() {};
        Ctor.isTypeSupported = function() { return false; };
        window[name] = Ctor;
    }
});

navigator.platform = navigator.platform || "MacIntel";
navigator.onLine = true;

// Minimal fetch() over native __httpRequest__: method, plain-object headers and
// string body in; ok, status, url, text() and json() out.  Text bodies only.
function fetch(input, init) {
    init = init || {};
    var url = String(input);
    var method = String(init.method || "GET").toUpperCase();
    var headerLines = [];
    if (init.headers) {
        for (var name in init.headers) {
            headerLines.push(name + ": " + init.headers[name]);
        }
    }
    var body = (init.body === undefined || init.body === null) ? "" : String(init.body);
    return new Promise(function(resolve, reject) {
        __httpRequest__(method, url, headerLines.join("\n"), body, function(status, text, error, effectiveUrl) {
            if (error !== undefined) {
                reject(new TypeError("fetch failed: " + error));
                return;
            }
            resolve({
                ok: status >= 200 && status < 300,
                status: status,
                url: effectiveUrl || url,
                text: function() { return Promise.resolve(text); },
                json: function() { return Promise.resolve().then(function() { return JSON.parse(text); }); }
            });
        });
    });
}

// Wrap setTimeout so the returned handle has a .clear() method.
// Some test utilities call timer.clear() instead of clearTimeout(timer).
// The handle also has valueOf() so it can be passed to clearTimeout directly.
(function() {
    var _rawSetTimeout = setTimeout;
    var _rawClearTimeout = clearTimeout;
    setTimeout = function(fn, delay) {
        var id = _rawSetTimeout(fn, delay);
        return {
            _id: id,
            valueOf: function() { return this._id; },
            clear: function() { _rawClearTimeout(this._id); }
        };
    };
    clearTimeout = function(handle) {
        if (handle && typeof handle === 'object' && handle._id !== undefined) {
            _rawClearTimeout(handle._id);
        } else {
            _rawClearTimeout(handle);
        }
    };
})();
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

static JSValueRef js_quit_main_loop(
    JSContextRef /*ctx*/,
    JSObjectRef /*function*/,
    JSObjectRef /*thisObject*/,
    size_t /*argumentCount*/,
    const JSValueRef /*arguments*/[],
    JSValueRef* /*exception*/)
{
    if (gMainLoop && g_main_loop_is_running(gMainLoop)) {
        g_main_loop_quit(gMainLoop);
    }
    return JSValueMakeUndefined(nullptr);
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

    // console.log plus the other common console methods, all writing to stdout
    for (const char* method : {"log", "info", "warn", "error", "debug", "trace"}) {
        JSStringRef methodName = JSStringCreateWithUTF8CString(method);
        JSObjectRef methodFunc =
            JSObjectMakeFunctionWithCallback(ctx, methodName, js_console_log);
        JSObjectSetProperty(ctx, console, methodName, methodFunc,
                            kJSPropertyAttributeNone, nullptr);
        JSStringRelease(methodName);
    }

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

    // setInterval
    JSStringRef setIntervalName = JSStringCreateWithUTF8CString("setInterval");
    JSObjectRef setIntervalFunc = JSObjectMakeFunctionWithCallback(ctx, setIntervalName, js_set_interval);
    JSObjectSetProperty(ctx, global, setIntervalName, setIntervalFunc,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(setIntervalName);

    // clearInterval
    JSStringRef clearIntervalName = JSStringCreateWithUTF8CString("clearInterval");
    JSObjectRef clearIntervalFunc = JSObjectMakeFunctionWithCallback(ctx, clearIntervalName, js_clear_interval);
    JSObjectSetProperty(ctx, global, clearIntervalName, clearIntervalFunc,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(clearIntervalName);

    JSStringRef httpName = JSStringCreateWithUTF8CString("__httpRequest__");
    JSObjectRef httpFunc = JSObjectMakeFunctionWithCallback(ctx, httpName, js_http_request);
    JSObjectSetProperty(ctx, global, httpName, httpFunc,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(httpName);

    // __quitMainLoop__ — called when the top-level async IIFE settles
    JSStringRef quitName = JSStringCreateWithUTF8CString("__quitMainLoop__");
    JSObjectRef quitFunc = JSObjectMakeFunctionWithCallback(ctx, quitName, js_quit_main_loop);
    JSObjectSetProperty(ctx, global, quitName, quitFunc,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(quitName);
}

static bool readFile(const char* path, std::string& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open JS file: " << path << "\n";
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

/**
 * Evaluate a script at global scope; report any exception (with stack) to stderr.
 */
static bool evaluateGlobalScript(JSGlobalContextRef ctx, const std::string& source, const char* sourceUrl)
{
    JSStringRef jsSource = JSStringCreateWithUTF8CString(source.c_str());
    JSStringRef jsUrl = JSStringCreateWithUTF8CString(sourceUrl);
    JSValueRef exc = nullptr;
    JSEvaluateScript(ctx, jsSource, nullptr, jsUrl, 1, &exc);
    JSStringRelease(jsSource);
    JSStringRelease(jsUrl);
    if (exc) {
        std::cerr << "Exception while evaluating " << sourceUrl << ": " << JSValueToStdString(ctx, exc, nullptr) << "\n";
        if (JSValueIsObject(ctx, exc)) {
            JSStringRef stackName = JSStringCreateWithUTF8CString("stack");
            JSValueRef stack = JSObjectGetProperty(ctx, JSValueToObject(ctx, exc, nullptr), stackName, nullptr);
            JSStringRelease(stackName);
            if (stack && !JSValueIsUndefined(ctx, stack)) {
                std::cerr << JSValueToStdString(ctx, stack, nullptr) << "\n";
            }
        }
        return false;
    }
    return true;
}

static int main_func(int argc, char** argv)
{

    gst_init(&argc, &argv);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (argc < 2) {
        std::cerr << "Usage: ./uveExecuter [preload.js ...] <script.js>\n"
                     "  preload.js  evaluated in order at global scope before script.js (e.g. a UMD bundle)\n"
                     "  script.js   test script; may use top-level await\n";
        return 1;
    }

    std::vector<std::string> preloads(argc - 2);
    for (int i = 1; i < argc - 1; i++) {
        if (!readFile(argv[i], preloads[i - 1])) { return 1; }
    }
    std::string script;
    if (!readFile(argv[argc - 1], script)) { return 1; }

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    installConsole(ctx);

    // Register AAMP bindings
    aamp_LoadJSController(ctx);

    // Browser-API polyfill, then any preload scripts, all at global scope.
    evaluateGlobalScript(ctx, kBrowserPolyfill, "uveExecuter-polyfill.js");
    for (int i = 1; i < argc - 1; i++) {
        if (!evaluateGlobalScript(ctx, preloads[i - 1], argv[i])) {
			interval_shutdown();
			http_shutdown();
			aamp_UnloadJSController(ctx);
			JSGlobalContextRelease(ctx);
			return 1;
        }
    }

    // Wrap the user script in an async IIFE so that top-level `await` expressions in the
    // user script (e.g. `await TST_2000_UVE_AampPlayback()`) work correctly
    // under JSEvaluateScript, which does not support module-level top-level await.
    std::string combined =
        "(async function() {\n" +
        script +
        "\n})().then(\n"
        "    function() { __quitMainLoop__(); },\n"
        "    function(e) {\n"
        "        __dispatchWindowEvent__('unhandledrejection', { reason: e, preventDefault: function(){} });\n"
        "        __quitMainLoop__();\n"
        "    }\n"
        ");\n";

    JSStringRef jsSource = JSStringCreateWithUTF8CString(combined.c_str());
    JSEvaluateScript(ctx, jsSource, nullptr, nullptr, 1, nullptr);
    JSStringRelease(jsSource);

    gMainLoop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(gMainLoop);

    // Cleanup (only reached if loop exits)
    g_main_loop_unref(gMainLoop);
    interval_shutdown();
    http_shutdown();
    // Explicitly tear down the AAMP player while C++ statics (gMutex, gActivePrivAAMPs,
    // PlayerCCManager, AampStreamSinkManager, ...) are still alive.  _aamp_term() is a
    // __attribute__((destructor(101))) that fires very late — after those statics are
    // destroyed — so it must find _allocated_aamp already NULL or it will crash trying
    // to lock a destroyed mutex (SIGABRT / std::terminate).
    aamp_UnloadJSController(ctx);
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
