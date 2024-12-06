/*
* Copyright 2018 RDK Management
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation, version 2.1
* of the license.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/

/**
 * @file StubsForAVEPlayer.cpp
 * @brief Various stubs required to compile AVE drm
 * @note To be removed once avedrm is used instead of aveplayer
 */

#ifdef AAMP_STUBS_FOR_JS

/*Stubs to link with libAVEPlayer.so*/
extern "C" {

void JSValueToObject(){ }
void JSStringCreateWithUTF8CString(){ }
void JSStringGetCharactersPtr(){ }
void JSValueMakeNull(){ }
void JSValueIsUndefined(){ }
void JSClassRelease(){ }
void JSValueIsObjectOfClass(){ }
void JSStringRelease(){ }
void JSObjectSetProperty(){ }
void JSValueMakeNumber(){ }
void JSValueToBoolean(){ }
void JSObjectIsFunction(){ }
void JSValueIsBoolean(){ }
void JSValueToStringCopy(){ }
void JSValueToNumber(){ }
void JSValueIsString(){ }
void JSValueUnprotect(){ }
void JSObjectSetPrivate(){ }
void JSStringGetUTF8CString(){ }
void JSContextGetGlobalObject(){ }
void JSValueMakeString(){ }
void JSGarbageCollect(){ }
void JSStringGetLength(){ }
void JSObjectMakeConstructor(){ }
void JSObjectCallAsFunction(){ }
void JSValueMakeBoolean(){ }
void JSValueIsObject(){ }
void JSObjectGetPropertyAtIndex(){ }
void JSObjectMakeFunction(){ }
void JSObjectCopyPropertyNames(){ }
void JSPropertyNameArrayRelease() { }
void JSObjectIsConstructor(){ }
void JSStringGetMaximumUTF8CStringSize(){ }
void JSValueMakeUndefined(){ }
void JSObjectMake(){ }
void JSPropertyNameArrayGetNameAtIndex(){ }
void JSValueIsInstanceOfConstructor(){ }
void JSContextGetGlobalContext(){ }
void JSValueIsNumber(){ }
void JSObjectHasProperty(){ }
void JSObjectGetPrivate(){ }
void JSPropertyNameArrayGetCount(){ }
void JSObjectMakeArray(){ }
void JSObjectGetProperty(){ }
void JSClassCreate(){ }
void JSValueProtect(){ }
void JSStringCreateWithCharacters(){ }
void JSObjectMakeDate(){ }
void JSObjectMakeError(){ }
void JSObjectDeleteProperty(){ }
void JSValueCreateJSONString(){ }
void JSObjectGetArrayBufferBytesPtr(){}
void JSObjectGetArrayBufferByteLength(){}
void JSEvaluateScript(){}
}
#endif
#ifndef REALTEKCE
#ifdef AVE_DRM

#include "psdk/PSDKEvents.h"
#include "psdkutils/PSDKError.h"

psdk::PSDKEventManager* GetPlatformCallbackManager()
{
    return NULL;
}


void* GetClosedCaptionSurface()
{
    return NULL;
}


void ClearClosedCaptionSurface()
{
}


void HideClosedCaptionSurface()
{
}

void ShowClosedCaptionSurface()
{
}

void* CreateSurface()
{
     return NULL;
}

void DestroySurface(void* surface)
{
}

void GetSurfaceScale(double *pScaleX, double *pScaleY)
{
}

void SetSurfaceSize(void* surface, int width, int height)
{
}

void SetSurfacePos(void* surface, int x, int y)
{
}

#endif
#endif
