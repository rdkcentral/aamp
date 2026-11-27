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

#import "VideoView.h"
#import <Accelerate/Accelerate.h>
#import <QuartzCore/QuartzCore.h>
#include <atomic>
#include <cstdlib>

#define RU2(v) (((v) + 1) / 2 * 2)
#define RU4(v) (((v) + 3) / 4 * 4)

static void ReleaseFrame(void *info, const void *data, size_t size)
{
	free((void *)data);
}

@implementation VideoView
{
	std::atomic<bool> _pending;
	CGColorSpaceRef _colorSpace;
	vImage_YpCbCrToARGB _conversion;
	BOOL _conversionReady;
}

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if (self)
	{
		_pending = false;
		self.wantsLayer = YES;
		self.layer.backgroundColor = NSColor.blackColor.CGColor;
		self.layer.contentsGravity = kCAGravityResizeAspect;
		_colorSpace = CGColorSpaceCreateDeviceRGB();

		vImage_YpCbCrPixelRange range = {16, 128, 235, 240, 255, 0, 255, 1};
		vImage_Error err = vImageConvert_YpCbCrToARGB_GenerateConversion(
			kvImage_YpCbCrToARGBMatrix_ITU_R_601_4, &range, &_conversion,
			kvImage420Yp8_Cb8_Cr8, kvImageARGB8888, kvImageNoFlags);
		_conversionReady = (err == kvImageNoError);
	}
	return self;
}

- (void)dealloc
{
	if (_colorSpace)
	{
		CGColorSpaceRelease(_colorSpace);
	}
}

- (void)displayI420:(const unsigned char *)buffer width:(int)width height:(int)height
{
	if (!_conversionReady || !buffer || width <= 0 || height <= 0)
	{
		return;
	}
	if (_pending.exchange(true))
	{
		return;
	}

	// I420 layout used by the AAMP appsink: rows padded to 4 bytes, even dimensions
	const int yStride = RU4(width);
	const int yRows = RU2(height);
	const int cStride = RU4(RU2(width) / 2);
	const int cRows = RU2(height) / 2;
	const unsigned char *yPlane = buffer;
	const unsigned char *cbPlane = yPlane + (size_t)yStride * yRows;
	const unsigned char *crPlane = cbPlane + (size_t)cStride * cRows;

	vImage_Buffer y = {(void *)yPlane, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)yStride};
	vImage_Buffer cb = {(void *)cbPlane, (vImagePixelCount)height / 2, (vImagePixelCount)width / 2, (size_t)cStride};
	vImage_Buffer cr = {(void *)crPlane, (vImagePixelCount)height / 2, (vImagePixelCount)width / 2, (size_t)cStride};

	const size_t outBytes = (size_t)width * height * 4;
	void *argb = malloc(outBytes);
	if (!argb)
	{
		_pending = false;
		return;
	}
	vImage_Buffer dst = {argb, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)width * 4};
	const uint8_t permute[4] = {0, 1, 2, 3};
	vImage_Error err = vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&y, &cb, &cr, &dst, &_conversion, permute, 255, kvImageNoFlags);
	if (err != kvImageNoError)
	{
		free(argb);
		_pending = false;
		return;
	}

	CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, argb, outBytes, ReleaseFrame);
	CGImageRef image = CGImageCreate(width, height, 8, 32, (size_t)width * 4, _colorSpace,
		kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Big, provider, NULL, false, kCGRenderingIntentDefault);
	CGDataProviderRelease(provider);

	dispatch_async(dispatch_get_main_queue(), ^{
		[CATransaction begin];
		[CATransaction setDisableActions:YES];
		self.layer.contents = (__bridge id)image;
		[CATransaction commit];
		CGImageRelease(image);
		self->_pending = false;
	});
}

@end
