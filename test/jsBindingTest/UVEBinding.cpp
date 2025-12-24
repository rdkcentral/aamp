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

// AAMP JS registration
extern "C" void aamp_LoadJSController(JSGlobalContextRef context);

static GMainLoop* gMainLoop = nullptr;

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

    // Execute JS
    JSStringRef jsSource =
        JSStringCreateWithUTF8CString(script.c_str());
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
