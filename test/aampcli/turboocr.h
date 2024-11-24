/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

/**
 * @file turboocr.h
 */
#define THRESHOLD 0x84

typedef enum
{
	eHOLE_NONE, // 1,2,3,5,7
	eHOLE_MID,	// 0
	eHOLE_TOP,	// 4,9
	eHOLE_BOT,	// 6
	eHOLE_PAIR,	// 8
} HoleType;

typedef enum {
	eEDGE_WIDE,		 	// top: 0,2,3,5,6,7,8,9
						// bot: 0,1,2,3,5,6,8,9
	// not: subset of these are "flat" like top of 5,7 and bottom of 2,1
	// but this can be difficult to distinguish for characters rendered at small font size
	
	eEDGE_SKINNY_LEFT, 	// top:	1
						// bot: 7

	eEDGE_SKINNY_RIGHT	// top: 4
						// bot: 4
} EdgeType;

struct GlyphCharacteristics
{
	HoleType hole;
	int msect; // identifies 1 and disambiguates 0,9
	EdgeType top;
	EdgeType bot;
	bool notTwo;
	bool notThree;
};

static const GlyphCharacteristics mGlyphCharacteristics[10] =
{
	// hole,        msect, 	top, 				bot
	{ eHOLE_MID, 	2,		eEDGE_WIDE,			eEDGE_WIDE },			// #0
	{ eHOLE_NONE,	1,		eEDGE_SKINNY_RIGHT,	eEDGE_WIDE },			// #1
	{ eHOLE_NONE,	3,		eEDGE_WIDE,			eEDGE_WIDE },			// #2
	{ eHOLE_NONE,	3,		eEDGE_WIDE,			eEDGE_WIDE },			// #3
	{ eHOLE_TOP,	2,		eEDGE_SKINNY_RIGHT,	eEDGE_SKINNY_RIGHT },	// #4
	{ eHOLE_NONE,	3,		eEDGE_WIDE,			eEDGE_WIDE },			// #5
	{ eHOLE_BOT,	3,		eEDGE_WIDE,			eEDGE_WIDE },			// #6
	{ eHOLE_NONE,	2,		eEDGE_WIDE,			eEDGE_SKINNY_LEFT },	// #7
	{ eHOLE_PAIR,	3,		eEDGE_WIDE,			eEDGE_WIDE },			// #8
	{ eHOLE_TOP,	3,		eEDGE_WIDE,			eEDGE_WIDE },			// #9
};

struct FloodFillStats
{
	int count;
	int miny;
	int maxy;
};

static void FloodFill( GlyphInfo *info, int x, int y, int pen, struct FloodFillStats *stats )
{
	if( GetPixel(info,x,y)==0 )
	{
		if( y > stats->maxy ) stats->maxy = y;
		if( y < stats->miny ) stats->miny = y;
		stats->count++;
		SetPixel(info,x,y,pen);
		FloodFill(info,x-1,y,pen,stats);
		FloodFill(info,x+1,y,pen,stats);
		FloodFill(info,x,y-1,pen,stats);
		FloodFill(info,x,y+1,pen,stats);
	}
}

static void FilterImage( GlyphInfo *info )
{
	for( int iy=0; iy<info->h; iy++ )
	{
		for( int ix=0; ix<info->w; ix++ )
		{
			int sample = GetPixel( info, ix, iy );
			if( sample<THRESHOLD )
			{
				SetPixel( info, ix, iy, 0x00 );
			}
			else
			{
				SetPixel( info, ix, iy, 0xff );
			}
		}
	}
}

EdgeType AnalyzeEdge( GlyphInfo *info, int iy )
{
	int inext = iy?(iy-1):1;
	int left = info->w;
	int right = 0;
	for( int ix=0; ix<info->w; ix++ )
	{
		int outer = GetPixel(info,ix,iy);
		int inner = GetPixel(info,ix,inext);
		if( outer || inner )
		{
			if( ix<left ) left = ix;
			if( ix>right ) right = ix;
		}
	}
	if( right<info->w/2 )
	{
		return eEDGE_SKINNY_LEFT;
	}
	else if( left >= info->w/2 )
	{
		return eEDGE_SKINNY_RIGHT;
	}
	return eEDGE_WIDE;
}

static void FillBackdrop( GlyphInfo *info )
{
	for(;;)
	{
		bool done = true;
		for( int iy=0; iy<info->h; iy++ )
		{
			for( int ix=0; ix<info->w; ix++ )
			{
				if( GetPixel(info,ix,iy)==0 )
				{
					if(
					   GetPixel(info,ix-1,iy)==1 ||
					   GetPixel(info,ix+1,iy)==1 ||
					   GetPixel(info,ix,iy-1)==1 ||
					   GetPixel(info,ix,iy+1)==1 )
					{
						SetPixel( info,ix,iy,1 );
						done = false;
					}
				}
			}
		}
		if( done ) break;
	}
}

static int ComputeMSect( GlyphInfo *info, int ix )
{
	int rc = 0;
	int prev = 0;
	for( int iy=0; iy<info->h; iy++ )
	{
		int pen = GetPixel(info,ix,iy)!=0;
		if( pen!= prev )
		{
			prev = pen;
			if( pen )
			{
				rc++;
			}
		}
	}
	return rc;
}

static HoleType CountHoles( GlyphInfo *info )
{
	HoleType hole = eHOLE_NONE;
	for( int iy=0; iy<info->h; iy++ )
	{
		for( int ix=0; ix<info->w; ix++ )
		{
			if( GetPixel(info,ix,iy)==0 )
			{
				FloodFillStats stats;
				stats.count = 0;
				stats.miny = info->h;
				stats.maxy = 0;
				FloodFill(info,ix,iy,0x01,&stats);
				if( stats.count<=3 )
				{ // despeckle
					SetPixel( info,ix,iy,0xff );
				}
				else if( hole == eHOLE_NONE )
				{
					if( stats.maxy - stats.miny > info->h/2 )
					{
						hole = eHOLE_MID;
					}
					else if( (stats.miny + stats.maxy)/2 > info->h/2 )
					{
						hole = eHOLE_BOT;
					}
					else
					{
						hole = eHOLE_TOP;
					}
				}
				else
				{
					hole = eHOLE_PAIR;
				}
			}
		}
	}
	return hole;
}

static bool IsEmptyCol( GlyphInfo *info, int x )
{
	for( int y=0; y<info->h; y++ )
	{
		if( GetPixel(info,x,y)!=0 )
		{
			return false;
		}
	}
	return true;
}

static bool IsEmptyRow( const GlyphInfo *info, int y )
{
	for( int x=0; x<info->w; x++ )
	{
		if( GetPixel(info,x,y)!=0 )
		{
			return false;
		}
	}
	return true;
}

static void TrimBounds( GlyphInfo *info )
{
	while( info->h>0 && IsEmptyRow(info, info->h-1) )
	{
		info->h--;
	}
	while( info->h>0 && IsEmptyRow(info, 0) )
	{
		info->y++;
		info->h--;
	}
	
	while( info->w>0 && IsEmptyCol(info, info->w-1) )
	{
		info->w--;
	}
	while( info->w>0 && IsEmptyCol(info, 0) )
	{
		info->x++;
		info->w--;
	}
}

static GlyphInfo *ScaleImage( const GlyphInfo *old_img )
{
	GlyphInfo *new_img = (GlyphInfo *)malloc( sizeof(*new_img) );
	new_img->x = 0;
	new_img->y = 0;
	new_img->w = old_img->w*GLYPH_REFERENCE_HEIGHT/old_img->h;
	new_img->h = GLYPH_REFERENCE_HEIGHT;
	new_img->pitch = new_img->w;
	int numPixels = new_img->w*new_img->h;
	new_img->data = (unsigned char *)malloc( numPixels );
	memset( new_img->data, 0, numPixels ); // needed?
	for( int iy=0; iy<new_img->h; iy++ )
	{
		int y_int = iy*old_img->h/new_img->h;
		for( int ix=0; ix<new_img->w; ix++ )
		{
			float x_int = ix*old_img->w/new_img->w;
			int pen = GetPixel( old_img, x_int, y_int );
			SetPixel( new_img, ix, iy, pen );
		}
	}
	return new_img;
}

int TurboOCRHelper( GlyphInfo *orig )
{
	static int idx;
	char name[64];
	
	int digit = -1;
	GlyphCharacteristics attrs;

	GlyphInfo *info = ScaleImage( orig );
	FilterImage( info ); // to 0x00 or 0xff
	TrimBounds( info );
	attrs.msect = ComputeMSect(info,info->w/2);
	
	attrs.notThree = false;
	// look for filled pixels near top-left of sprite (present for "5" bot not "3")
	for( int iy=4; iy<6; iy++ )
	{
		for( int ix=0; ix<3; ix++ )
		{
			if( GetPixel(info,ix,iy)!=0 )
			{
				attrs.notThree = true;
			}
		}
	}

	// look for filled pixels near bottom-right of sprite (present for "3" but not "2")
	attrs.notTwo = false;
	for( int iy=10; iy<=12; iy++ )
	{
		for( int ix=info->w-2/*3*/; ix<info->w; ix++ )
		{
			if( GetPixel(info,ix,iy)!=0 )
			{
				attrs.notTwo = true;
			}
		}
	}
	
	attrs.top = AnalyzeEdge( info, 0 );
	attrs.bot = AnalyzeEdge( info, info->h-1 );
	FillBackdrop( info );
	attrs.hole = CountHoles( info );

	if( attrs.msect == 1 )
	{ // unique, sufficient discriminator for "1" glyph
		digit = 1;
	}
	else
	{
		for( int i=0; i<10; i++ )
		{
			const GlyphCharacteristics *candidate = &mGlyphCharacteristics[i];
			if(
			   candidate->hole == attrs.hole &&
			   candidate->top == attrs.top &&
			   candidate->bot == attrs.bot )
			{
				digit = i;
				if( i==2 && attrs.notTwo )
				{ // stable discriminator for "2" and "3"
					continue;
				}
				if( i==3 && attrs.notThree )
				{ // stable discriminator for "3" and "5"
					continue;
				}
				break;
			}
		}
	}
	if( 0 )
	{ // debugging - dump post-processed glyphs and extracted characteristics
		snprintf( name, sizeof(name),
				 "%d.%c%c%c%d%s%s.%03d",
				 digit,
				 "?O968"[attrs.hole],
				 "_/\\"[attrs.top],
				 "_/\\"[attrs.bot],
				 attrs.msect,
				 attrs.notTwo?"!2":"",
				 attrs.notThree?"!3":"",
				 idx++ );
		CreatePGM( info, name );
	}
	
	free( info->data );
	free( info );
	
	return digit;
}

int TurboOCR( void )
{
	int mediaTime = 0;
	int frameWidth = Shader::appsinkData.width;
	int frameHeight = Shader::appsinkData.height;
	GlyphInfo info;
	info.data = Shader::appsinkData.yuvBuffer;
	info.pitch = ((frameWidth+3)/4)*4;
	const struct
	{
		int x; // reference horizontal position of this digit as baked into video
		int ms; // digit contributions in milliseconds
	} mOcrMap[9] =
	{ // HH:MM:SS.SSS
		// TODO: slice automatically into characters by looking for vertical whitespace
		{350,1000*60*60*10}, // hour
		{364,1000*60*60}, // hour
		{387,1000*60*10}, // minute
		{401,1000*60}, // minute
		{424,1000*10}, // second
		{437,1000}, // second
		{458,100}, // millisecond
		{472,10}, // millisecond
		{486,1} // millisecond
	};
	for( int i=0; i<sizeof(mOcrMap)/sizeof(mOcrMap[0]); i++ )
	{
		info.x = mOcrMap[i].x*frameWidth/GLYPH_REFERENCE_FRAMEWIDTH;
		info.y = 228*frameHeight/GLYPH_REFERENCE_FRAMEHEIGHT;
		info.w = GLYPH_REFERENCE_WIDTH*frameWidth/GLYPH_REFERENCE_FRAMEWIDTH;
		info.h = GLYPH_REFERENCE_HEIGHT*frameHeight/GLYPH_REFERENCE_FRAMEHEIGHT;
		int digit = TurboOCRHelper(&info);
		if( digit<0 )
		{ // error
			break;
		}
		mediaTime += digit*mOcrMap[i].ms;
	}
	return mediaTime;
}
