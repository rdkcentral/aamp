/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 * @file AampcliShader.cpp
 * @brief Aampcli shader file.
 */
#include "AampcliShader.h"
#include "AampUtils.h"

std::string aamp_GetLocalPath( const char *filename )
{ // TODO: move to AampUtils
	std::string cfgPath;
	const char *env_aamp_enable_opt = "true"; // default
#ifdef AAMP_SIMULATOR_BUILD
	const char *baseDir = getenv("AAMP_CFG_DIR");
	if( !baseDir )
	{
		baseDir = getenv("HOME");
	}
	cfgPath = baseDir;
	if( aamp_StartsWith(filename,"/opt/" ) )
	{ // skip leading /opt in simulator
		filename += 4;
	}
#elif defined(AAMP_CPC) // AAMP_ENABLE_OPT_OVERRIDE defined only in Comcast PROD builds
	env_aamp_enable_opt = getenv("AAMP_ENABLE_OPT_OVERRIDE");
#endif
	if( env_aamp_enable_opt )
	{
		cfgPath += filename;
	}
	return cfgPath;
}

//#define PROCESS_CAPTURED_HDMI_VIDEO
// optional compile-time configuration to process frames from /capture/%06d.jpg
// this can be used to post-process HDMI-captured video frames, extracted using:
// ffmpeg -i *.mp4 %06d.jpg

#ifdef PROCESS_CAPTURED_HDMI_VIDEO
#include "jpeglib.h"
#endif

// temporarily exclude Ubuntu, to avoid ubuntu L2 tests failing when run in container
#if defined(__APPLE__) // || defined(UBUNTU)
#define USE_OPENGL
// for now, use with simulator only, to avoid device compilation issues
#endif

#ifdef USE_OPENGL
// extract each frame and render using opengl shader

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLUT/glut.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/freeglut.h>
#endif // Linux

struct GlyphInfo
{
	unsigned char *data;
	int pitch;
	int x;
	int y;
	int w;
	int h;
};
static void CreatePGM( const GlyphInfo *info, const char *name );

struct AppsinkData
{
	int width = 0;
	int height = 0;
	uint8_t *yuvBuffer = NULL;
};

class Shader
{
	private:
		static bool ocrEnabled;

	public:
		static const int FPS = 60;
		static GLuint mProgramID;
		static GLuint id_y, id_u, id_v; // texture id
		static GLuint textureUniformY, textureUniformU,textureUniformV;
		static GLuint _vertexArray;
		static GLuint _vertexBuffer[2];

		GLuint LoadShader(GLenum shader, const char *code);
		void InitShaders();
		static void glRender(void);
		static void timer(int v);
		static AppsinkData appsinkData;
		static std::mutex appsinkData_mutex;
		static void updateYUVFrame( const unsigned char *buffer, int size, int width, int height);
};

std::function< void(const unsigned char *, int, int, int) > gpUpdateYUV = Shader::updateYUVFrame;

void createAppWindow( int argc, char **argv )
{ // render frames in graphics plane using opengl
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowPosition(80, 80);
	glutInitWindowSize(640, 480);
	glutCreateWindow("AAMP");
	printf("[AAMPCLI] OpenGL Version[%s] GLSL Version[%s]\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
#ifndef __APPLE__
	glewInit();
#endif
	Shader l_Shader;
	l_Shader.InitShaders();
	glutDisplayFunc(l_Shader.glRender);
	glutTimerFunc(40, l_Shader.timer, 0);
	
	glutMainLoop();
}

void destroyAppWindow( void )
{ // stub
}

bool Shader::ocrEnabled = getenv("ocr");

AppsinkData Shader::appsinkData = AppsinkData();
std::mutex Shader::appsinkData_mutex;

static const char *VSHADER =
"attribute vec2 vertexIn;"
"attribute vec2 textureIn;"
"varying vec2 textureOut;"
"void main() {"
"gl_Position = vec4(vertexIn,0, 1);"
"textureOut = textureIn;"
"}";

static const char *FSHADER =
"#ifdef GL_ES \n"
"  precision mediump float; \n"
"#endif \n"
"varying vec2 textureOut;"
"uniform sampler2D tex_y;"
"uniform sampler2D tex_u;"
"uniform sampler2D tex_v;"
"void main() {"
"vec3 yuv;"
"vec3 rgb;"
"yuv.x = texture2D(tex_y, textureOut).r;"
"yuv.y = texture2D(tex_u, textureOut).r - 0.5;"
"yuv.z = texture2D(tex_v, textureOut).r - 0.5;"
"rgb = mat3( 1, 1, 1, 0, -0.39465, 2.03211, 1.13983, -0.58060,  0) * yuv;"
"gl_FragColor = vec4(rgb, 1);"
"}";

GLuint Shader::mProgramID = 0;
GLuint Shader::id_y = 0;
GLuint Shader::id_u = 0;
GLuint Shader::id_v = 0;
GLuint Shader::textureUniformY = 0;
GLuint Shader::textureUniformU = 0;
GLuint Shader::textureUniformV = 0;
GLuint Shader::_vertexArray = 0;
GLuint Shader::_vertexBuffer[2] = {0};

GLuint Shader::LoadShader( GLenum shader, const char *code )
{
	GLuint shaderHandle = 0;
	if( code )
	{
		const char *sources[1] = { code };
		shaderHandle = glCreateShader(shader);
		glShaderSource(shaderHandle, 1, sources, 0);
		glCompileShader(shaderHandle);
		GLint compileSuccess;
		glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileSuccess);
		if (compileSuccess == GL_FALSE)
		{
			GLchar msg[1024];
			glGetShaderInfoLog(shaderHandle, sizeof(msg), 0, &msg[0]);
			printf("[AAMPCLI] %s\n", msg );
		}
	}
	return shaderHandle;
}

#define ATTRIB_VERTEX 0
#define ATTRIB_TEXTURE 1

void Shader::InitShaders()
{
#ifdef PROCESS_CAPTURED_HDMI_VIDEO
	{ // process offline-captured frames
		int frameNumber = 0;
		for(;;)
		{
			char filename[32];
			snprintf( filename, sizeof(filename), "/capture/%06d.jpg", ++frameNumber );
			std::string path = aamp_GetLocalPath(filename);
			FILE *f = fopen( path.c_str(), "rb" );
			if( !f )
			{ // no more frames
				printf( "DONE! processed %d frames\n", frameNumber-1 );
				exit(0);
			}
			else
			{
				struct jpeg_decompress_struct cinfo;
				struct jpeg_error_mgr jerr;
				cinfo.err = jpeg_std_error(&jerr);
				jpeg_create_decompress(&cinfo);
				jpeg_stdio_src(&cinfo, f);
				jpeg_read_header(&cinfo, TRUE);
				jpeg_start_decompress(&cinfo);
				auto width = cinfo.output_width;
				auto height = cinfo.output_height;
				unsigned char *lineBuf = (unsigned char *)malloc(width*3);
				if( !lineBuf )
				{
					exit(1);
				}
				else
				{
					unsigned char *buffer_array[1] = { lineBuf };
					unsigned char *yuvBuffer = (unsigned char *)malloc(width*height);
					if( !yuvBuffer )
					{
						exit(1);
					}
					else
					{
						for( int ypos=0; ypos<height; ypos++ )
						{
							assert( cinfo.output_scanline == ypos );
							jpeg_read_scanlines(&cinfo, buffer_array, 1);
							const unsigned char *src = lineBuf;
							for( int xpos=0; xpos<width; xpos++ )
							{
								int intensity = (src[0]+src[1]+src[2])/3;
								src += 3;
								yuvBuffer[ypos*width+xpos] = intensity;
							}
						}
						updateYUVFrame( yuvBuffer, width*height, width, height );
						free( yuvBuffer );
					}
					free( lineBuf );
				}
				(void) jpeg_finish_decompress(&cinfo);
				jpeg_destroy_decompress(&cinfo);
				fclose( f );
			}
		}
	}
#endif // PROCESS_CAPTURED_HDMI_VIDEO
	GLint linked;

	GLint vShader = LoadShader(GL_VERTEX_SHADER, VSHADER );
	GLint fShader = LoadShader(GL_FRAGMENT_SHADER, FSHADER);
	mProgramID = glCreateProgram();
	glAttachShader(mProgramID,vShader);
	glAttachShader(mProgramID,fShader);

	glBindAttribLocation(mProgramID, ATTRIB_VERTEX, "vertexIn");
	glBindAttribLocation(mProgramID, ATTRIB_TEXTURE, "textureIn");
	glLinkProgram(mProgramID);
	glValidateProgram(mProgramID);

	glGetProgramiv(mProgramID, GL_LINK_STATUS, &linked);
	if( linked == GL_FALSE )
	{
		GLint logLen;
		glGetProgramiv(mProgramID, GL_INFO_LOG_LENGTH, &logLen);
		GLchar *msg = (GLchar *)malloc(sizeof(GLchar)*logLen);
		glGetProgramInfoLog(mProgramID, logLen, &logLen, msg );
		printf( "%s\n", msg );
		free( msg );
	}
	glUseProgram(mProgramID);
	glDeleteShader(vShader);
	glDeleteShader(fShader);

	//Get Uniform Variables Location
	textureUniformY = glGetUniformLocation(mProgramID, "tex_y");
	textureUniformU = glGetUniformLocation(mProgramID, "tex_u");
	textureUniformV = glGetUniformLocation(mProgramID, "tex_v");

	typedef struct _vertex
	{
		float p[2];
		float uv[2];
	} Vertex;

	static const Vertex vertexPtr[4] =
	{
		{{-1,-1}, {0.0,1 } },
		{{ 1,-1}, {1,1 } },
		{{ 1, 1}, {1,0.0 } },
		{{-1, 1}, {0.0,0.0} }
	};
	static const unsigned short index[6] =
	{
		0,1,2, 2,3,0
	};

	glGenVertexArrays(1, &_vertexArray);
	glBindVertexArray(_vertexArray);
	glGenBuffers(2, _vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexPtr), vertexPtr, GL_STATIC_DRAW );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vertexBuffer[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW );
	glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE,
			sizeof(Vertex), (const GLvoid *)offsetof(Vertex,p) );
	glEnableVertexAttribArray(ATTRIB_VERTEX);

	glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, GL_FALSE,
			sizeof(Vertex), (const GLvoid *)offsetof(Vertex, uv ) );
	glEnableVertexAttribArray(ATTRIB_TEXTURE);
	glBindVertexArray(0);

	glGenTextures(1, &id_y);
	glBindTexture(GL_TEXTURE_2D, id_y);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &id_u);
	glBindTexture(GL_TEXTURE_2D, id_u);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &id_v);
	glBindTexture(GL_TEXTURE_2D, id_v);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/**
  @brief upack and render next I420 (4:3:0 YUV) frame
 
		Component 0: Y
		  depth:           8
		  pstride:         1
		  default offset:  0
		  default rstride: RU4 (width)
		  default size:    rstride (component0) * RU2 (height)

		Component 1: U
		  depth:           8
		  pstride:         1
		  default offset:  size (component0)
		  default rstride: RU4 (RU2 (width) / 2)
		  default size:    rstride (component1) * RU2 (height) / 2

		Component 2: V
		  depth            8
		  pstride:         1
		  default offset:  offset (component1) + size (component1)
		  default rstride: RU4 (RU2 (width) / 2)
*/
void Shader::glRender(void){

	int width = 0;
	int height = 0;
	const uint8_t *yuvBuffer = NULL;

	{
		std::lock_guard<std::mutex> lock(appsinkData_mutex);
		yuvBuffer = appsinkData.yuvBuffer;
		appsinkData.yuvBuffer = NULL;
		width = appsinkData.width;
		height = appsinkData.height;
	}

#define RU4(VALUE) (((VALUE+3)/4)*4)
#define RU2(VALUE) (((VALUE+1)/2)*2)

	if(yuvBuffer)
	{
		const unsigned char *yPlane = yuvBuffer;
		const unsigned char *uPlane = yPlane + RU4(width)*RU2(height);
		const unsigned char *vPlane = uPlane + RU4(RU2(width)/2) * RU2(height)/2;
		
		glClearColor(0.0,0.0,0.0,0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		//Y
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, id_y);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, yPlane);
		glUniform1i(textureUniformY, 0);

		//U
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, id_u);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, uPlane);
		glUniform1i(textureUniformU, 1);

		//V
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, id_v);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, vPlane);
		glUniform1i(textureUniformV, 2);
		
		glBindVertexArray(_vertexArray);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );
		glBindVertexArray(0);

		glutSwapBuffers();
		SAFE_DELETE(yuvBuffer);
	}
}

void Shader::timer(int v)
{
	glutPostRedisplay();
	glutTimerFunc(1000/FPS, timer, v);
}

#define GLYPH_REFERENCE_FRAMEWIDTH 842
#define GLYPH_REFERENCE_FRAMEHEIGHT 474
#define GLYPH_REFERENCE_WIDTH 13
#define GLYPH_REFERENCE_HEIGHT 18

static void SetPixel( GlyphInfo *info, int x, int y, int pen )
{
	if( x>=0 && y>=0 && x<info->w && y<info->h )
	{
		int idx = (y+info->y)*info->pitch+(x+info->x);
		info->data[idx] = pen;
	}
}

static int GetPixel( const GlyphInfo *info, int x, int y )
{
	if( x>=0 && y>=0 && x<info->w && y<info->h )
	{
		int idx = (y+info->y)*info->pitch+(x+info->x);
		return info->data[idx];
	}
	else
	{ // out of range
		return 1;
	}
}

static void CreatePGM( const GlyphInfo *info, const char *name )
{ // utility function: export image as grayscale pgm; can be used to debug TurboOCR
	char filename[32];
	snprintf( filename, sizeof(filename), "/export/%s.pgm", name );
	std::string path = aamp_GetLocalPath(filename);
	FILE *f = fopen( path.c_str(), "wb" );
	if( f )
	{
		fprintf( f, "P2\n%d %d 255\n", info->w, info->h );
		for( int iy=0; iy<info->h; iy++ )
		{
			for( int ix=0; ix<info->w; ix++ )
			{
				fprintf( f, "%d ", GetPixel(info,ix,iy) );
			}
			fprintf( f, "\n" );
		}
		fclose( f );
	}
}

#include "turboocr.h"

void Shader::updateYUVFrame(const unsigned char *buffer, int size, int width, int height)
{
	uint8_t* frameBuf = new uint8_t[size];
	memcpy(frameBuf, buffer, size);

	{
		std::lock_guard<std::mutex> lock(appsinkData_mutex);
		if(appsinkData.yuvBuffer)
		{ // opengl rendering can't keep up
			SAFE_DELETE(appsinkData.yuvBuffer);
		}
		appsinkData.yuvBuffer = frameBuf;
		appsinkData.width = width;
		appsinkData.height = height;
		
		if( ocrEnabled )
		{
			int mediaTime = TurboOCR();
			static int prevMediaTime;
			AAMPLOG_MIL( "%02d:%02d:%02d.%03d (%d)",
						mediaTime/(1000*60*60), // hour
						(mediaTime/(1000*60))%60, // min
						(mediaTime/1000)%60, // sec
						(mediaTime%1000), // ms
						mediaTime-prevMediaTime );
			prevMediaTime = mediaTime;
		}
	}
}

#else // USE_OPENGL

std::function< void(const unsigned char *, int, int, int) > gpUpdateYUV = nullptr;

/**
	No need to have OSX-specific variant here, as we're leveraging gstreamer windows.
	But we still need to create a dummy cocoa window or nothing renders(!)
	Historically, an osx cocoa window was once used andconnected under hood with gstreamer
	This gave us ability to programatically resize the window.
	This also gave us ability to update title bar contextually.
*/
#ifdef __APPLE__
#import <cocoa_window.h>
#else
#define osx_createAppWindow(argc,argv) // NOP
#define osx_destroyAppWindow() // NOP
#endif

void createAppWindow( int argc, char **argv )
{
	osx_createAppWindow( argc, argv );
}
void destroyAppWindow( void )
{
	osx_destroyAppWindow();
}

#endif // !USE_OPENGL

