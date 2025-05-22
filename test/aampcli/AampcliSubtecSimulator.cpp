/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#include "AampcliSubtecSimulator.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "SubtecPacket.hpp"		// needed for Packet class


#define WRITE_HASCII( DST, BYTE ) \
{ \
	*DST++ = "0123456789abcdef"[BYTE>>4]; \
	*DST++ = "0123456789abcdef"[BYTE&0xf]; \
}


static struct SubtecSimulatorState
{
	bool started;
	pthread_t threadId;
	int sockfd;
} mSubtecSimulatorState = {};

static bool read32(const unsigned char *ptr, size_t len, std::uint32_t &ret32)
{
	bool ret = false;
	//Load packet header
	if (len >= sizeof(std::uint32_t))
	{
		const std::uint32_t byte0 = static_cast<const uint32_t>(ptr[0]) & 0xFF;
		const std::uint32_t byte1 = static_cast<const uint32_t>(ptr[1]) & 0xFF;
		const std::uint32_t byte2 = static_cast<const uint32_t>(ptr[2]) & 0xFF;
		const std::uint32_t byte3 = static_cast<const uint32_t>(ptr[3]) & 0xFF;
		ret32 =  byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
		ret = true;
	}

	return ret;
}

/**
 * @brief Compactly log blobs of binary data
 */
static void DumpBinaryData(const unsigned char *ptr, size_t len)
{
#define FIT_CHARS 64
	char buf[FIT_CHARS + 1]; // pad for NUL
	char *dst = buf;
	const unsigned char *fin = ptr+len;
	int fit = FIT_CHARS;
	while (ptr < fin)
	{
		unsigned char c = *ptr++;
		if (c >= ' ' && c < 128)
		{ // printable ascii
			*dst++ = c;
			fit--;
		}
		else if( fit>=4 )
		{
			*dst++ = '[';
			WRITE_HASCII( dst, c );
			*dst++ = ']';
			fit -= 4;
		}
		else
		{
			fit = 0;
		}
		if (fit==0 || ptr==fin )
		{
			*dst++ = 0x00;

			printf("%s\n", buf);
			dst = buf;
			fit = FIT_CHARS;
		}
	}
}

static void DumpPacket(const unsigned char *ptr, size_t len)
{
	//Get type
	std::uint32_t type;
	if (read32(ptr, len, type))
	{
		printf("Type:%s:%d\n", Packet::getTypeString(type).c_str(), type);
		ptr += 4;
		len -= 4;
	}
	else
	{
		printf("Packet read failed on type - returning\n");
		return;
	}
	//Get Packet counter
	std::uint32_t counter;
	if (read32(ptr, len, counter))
	{
		printf("Counter:%d\n", counter);
		ptr += 4;
		len -= 4;
	}
	else
	{
		printf("Packet read failed on type - returning\n");
		return;
	}
	//Get size
	std::uint32_t size;
	if (read32(ptr, len, size))
	{
		printf("Packet size:%d\n", size);
		ptr += 4;
		len -= 4;
	}
	else
	{
		printf("Packet read failed on type - returning\n");
		return;
	}

	if (Packet::getTypeString(type) == "CC_SET_ATTRIBUTE")
	{
		uint32_t channelId = 0;
		uint32_t ccType = 0;
		uint32_t attribType = 0;
		uint32_t attribValue = 0;

		if (read32(ptr, len, channelId))
		{
			printf("channelId:%d\n", channelId);
			ptr += 4;
			len -= 4;
		}
		else
		{
			printf("channelId read failed\n");
		}

		if (read32(ptr, len, ccType))
		{
			printf("ccType:%d\n", ccType);
			ptr += 4;
			len -= 4;
		}
		else
		{
			printf("ccType read failed\n");
		}

		if (read32(ptr, len, attribType))
		{
			ptr += 4;
			len -= 4;
		}
		else
		{
			printf("AttribType read failed\n");
		}

		uint32_t i = 0;
		while (attribType != 0)
		{
			if (read32(ptr, len, attribValue))
			{
				ptr += 4;
				len -= 4;
			}
			else
			{
				printf("attribValue read failed\n");
				break;
			}
			if (attribType & 1)
			{
				printf("attribute[%u]: %u\n", i, attribValue);
			}

			attribType >>= 1;
			i++;
		}
	}
	else
	{
		if (len > 0)
		{
			printf("Packet data:\n");
			DumpBinaryData(ptr, len);
		}
	}
}

static void *SubtecSimulatorThread( void *param )
{
	struct SubtecSimulatorState *state = (SubtecSimulatorState *)param;
	struct sockaddr cliaddr;
	socklen_t sockLen = sizeof(cliaddr);
	struct timeval timeout;
	size_t maxBuf = 8*1024; // big enough?
	unsigned char *buffer = (unsigned char *)malloc(maxBuf);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if( buffer )
	{
		printf( "SubtecSimulatorThread - listening for packets\n" );
		while(state->started)
		{
			fd_set readfds, masterfds;
			FD_ZERO(&masterfds);
			FD_SET(state->sockfd, &masterfds);
			memcpy(&readfds, &masterfds, sizeof(fd_set));
			if (select(state->sockfd + 1, &readfds, NULL, NULL, &timeout) < 0)
			{
				printf( "select failed\n" );
			}
			else if (FD_ISSET(state->sockfd, &readfds))
			{
				int numBytes = (int)recvfrom( state->sockfd, (void *)buffer, maxBuf, MSG_WAITALL, (struct sockaddr *) &cliaddr, &sockLen);
				printf( "***SubtecSimulatorThread:\n" );
				DumpPacket( buffer, numBytes );
			}
			else
			{
				// Nothing to read, continue waiting or exit the loop
			}
		}
		free( buffer );
	}
	(void)close( state->sockfd );
	printf( "SubtecSimulatorThread - exit\n" );
	return 0;
}

bool StartSubtecSimulator( const char *socket_path )
{
	struct SubtecSimulatorState *state = &mSubtecSimulatorState;
	if( !state->started )
	{
		(void)unlink( socket_path );	// close if left over from previous session to avoid bind failure
		state->sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
		if( state->sockfd>=0 )
		{
			struct sockaddr_un serverAddr;
			memset(&serverAddr, 0, sizeof(serverAddr));
			serverAddr.sun_family = AF_UNIX;
			strncpy(serverAddr.sun_path, socket_path, sizeof(serverAddr.sun_path) - 1);
			socklen_t len = sizeof(serverAddr);
			if( bind( state->sockfd, (struct sockaddr*)&serverAddr, len ) == 0 )
			{
				state->started = true; //assume it is going to start
				if( pthread_create(&state->threadId, NULL, &SubtecSimulatorThread, (void *)state) )
				{
					printf( "SubtecSimulatorThread create() error: %d\n", errno );
					state->started = false;
				}
			}
			else
			{
				printf( "SubtecSimulatorThread bind() error: %d\n", errno );
			}
		}
	}
	else
	{
		printf( "Subtec Simulator already started\n" );
	}
	return state->started;
}

bool StopSubtecSimulator( void )
{
	struct SubtecSimulatorState *state = &mSubtecSimulatorState;
	if( state->started )
	{
		state->started = false;
		(void)pthread_join(state->threadId, NULL);
	}
	else
	{
		printf( "Subtec Simulator already stopped\n" );
	}
	return !state->started;
}
