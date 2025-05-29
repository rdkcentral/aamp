/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:errno
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
#ifdef USE_WATERMARK_JSBINDINGS
#include "PersistentWatermarkStorage.h"

#include <cstring>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstdlib>
#include <ctime>

#include  "PersistentWatermarkPluginAccess.h"
#include "_base64.h"

static constexpr int PNG_SIGNATURE_SIZE = 8;
static constexpr int PNG_CHUNK_METADATA_SIZE = 12;
static constexpr int PNG_IHDR_DATA_SIZE = 13;
static constexpr int PNG_MIN_SIZE = (PNG_SIGNATURE_SIZE + (PNG_CHUNK_METADATA_SIZE*4) + PNG_IHDR_DATA_SIZE);

PersistentWatermark::Storage::Storage():
mpState(new StoragePersistent) //starting in persistent mode (so that previously saved watermarks can be loaded without an update())
{
	std::srand(std::time(nullptr));
}

/**
 @brief returns the metadata for the stored watermark (or "" if no watermark is stored)
**/
std::string PersistentWatermark::Storage::getMetadata()
{
	LOG_TRACE("PersistentWatermark::Storage::getMetadata()");
	if(mpState)
	{
		LOG_WARN_EX("PersistentWatermark::Storage::getMetadata(): %s", mpState->getMetadata().c_str());
		return mpState->getMetadata();
	}
	else
	{
		LOG_WARN_EX("PersistentWatermark::Storage::getMetadata(): no state");
		return "";
	}
}

/**
 @brief clears any previously stored data and if possible stores the watermark image & metadata supplied in the JS arguments
**/
JSValueRef PersistentWatermark::Storage::Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	LOG_TRACE("PersistentWatermark::Storage::Update()");
	if(mpState)
	{
		delete mpState;
		mpState=nullptr;
	}
	else
	{
		LOG_WARN_EX("PersistentWatermark::Storage::Update(): no state");
	}

	//Arguments 1 & 2 are checked at lower levels
	bool PersistentArgumentSupplied = (argumentCount>=3);
	if(PersistentArgumentSupplied && !JSValueIsBoolean(ctx, arguments[2]))
	{
		LOG_ERROR_EX("PersistentWatermark: 'persistent' argument is not a Boolean.");
		return JSValueMakeBoolean(ctx, false);
	}

	bool persistent = PersistentArgumentSupplied && JSValueToBoolean(ctx, arguments[2]);
	if(persistent)
	{
		LOG_TRACE("PersistentWatermark::Storage::Update(): Persistent mode.");;
		mpState = new StoragePersistent;
	}
	else
	{
		LOG_TRACE("PersistentWatermark::Storage::Update(): Volatile mode.");;
		mpState = new StorageVolatile;
	}
	return mpState->Update(ctx, function, thisObject, argumentCount, arguments, exception);
}

/**
 @brief Call update the specified watermark plugin layer
**/
bool PersistentWatermark::Storage::UpdatePlugin(int layerID)
{
	LOG_TRACE("PersistentWatermark::Storage::UpdatePlugin()");
	if(mpState)
	{
		return mpState->UpdatePlugin(layerID);
	}
	else
	{
		LOG_WARN_EX("PersistentWatermark::Storage::UpdatePlugin(): no state");
		return false;
	}
}

PersistentWatermark::Storage::~Storage()
{
	LOG_TRACE("PersistentWatermark::Storage::~Storage()");
	if(mpState)
	{
		delete mpState;
		mpState=nullptr;
	}
	else
	{
		LOG_WARN_EX("PersistentWatermark::Storage::~Storage(): no state");
	}
}


PersistentWatermark::StorageVolatile::StorageVolatile():mMetaData(""), mSharedMemoryKey(0), msize(0)
{
	//empty
}

void PersistentWatermark::StorageVolatile::deleteSharedMemory()
{
	LOG_TRACE("Enter");
	if(mSharedMemoryKey)
	{
		int ID = shmget(mSharedMemoryKey, msize, 0644);
		if(ID==-1)
		{
			LOG_ERROR_EX("PersistentWatermark:Could not retrieve ID %s", strerror(errno));
		}
		else
		{
			int rtn = shmctl(ID, IPC_RMID, nullptr);
			switch(rtn)
			{
				case 0:
					LOG_TRACE("PersistentWatermark:Shared memory marked for deletion.");
					break;

				case -1:
					LOG_ERROR_EX("PersistentWatermark:Error deleting shared memory %s", strerror(errno));
					break;

				default:
					LOG_ERROR_EX("PersistentWatermark:Unexpected return from shmctl %d", rtn);
					break;
			};
		}
		mSharedMemoryKey = 0;
	}
	else
	{
		LOG_TRACE("Delete not required.");
	}
	LOG_TRACE("Exit");
}

std::string PersistentWatermark::StorageVolatile::getMetadata()
{
	LOG_TRACE("PersistentWatermark::StorageVolatile::getMetadata()");
	return mMetaData;
}

static std::tuple<void*, int, std::string> getImageBufferAndMetadata(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	std::tuple<void*, int, std::string> errorRtn = std::make_tuple(nullptr, 0, "");

	if(!JSValueIsObject(ctx, arguments[0]))
	{
		LOG_ERROR_EX("PersistentWatermark: Argument 0 is not an object.");
		return errorRtn;
	}

	if(!JSValueIsString(ctx, arguments[1]))
	{
		LOG_ERROR_EX("PersistentWatermark: Argument 2 is not a string.");
		return errorRtn;
	}

	const char* localMetaData = aamp_JSValueToCString(ctx, arguments[1], exception);
	LOG_WARN_EX("PersistentWatermark: supplied metadata: %s", localMetaData);

	JSObjectRef ArrayBuffer = JSValueToObject(ctx, arguments[0], NULL);
	int size = JSObjectGetArrayBufferByteLength(ctx, ArrayBuffer, exception);
	{
		std::string msg = "PersistentWatermark: ArrayBuffer size = ";
		msg+=std::to_string(size);
		if(size<PNG_MIN_SIZE)
		{
			msg+=". This is too small to be a valid .PNG file." ;
			LOG_ERROR_EX(msg.c_str());
			return errorRtn;
		}
		else
		{
			LOG_WARN_EX(msg.c_str());
		}
	}

	constexpr unsigned char PNG_SIGNATURE[PNG_SIGNATURE_SIZE] = {0x89, 'P', 'N', 'G', 0xd, 0xa, 0x1a, 0xa};
	void* inputBuffer = JSObjectGetArrayBufferBytesPtr(ctx, ArrayBuffer, exception);
	if(memcmp(PNG_SIGNATURE, inputBuffer, PNG_SIGNATURE_SIZE))
	{
		LOG_ERROR_EX("PersistentWatermark: Buffer does not contain a valid PNG.");
		return errorRtn;
	}
	else
	{
		LOG_TRACE("PersistentWatermark: Buffer contains PNG header.");
	}

	return std::make_tuple(inputBuffer, size, localMetaData);
}


JSValueRef PersistentWatermark::StorageVolatile::Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	LOG_TRACE("PersistentWatermark::StorageVolatile::Update()");

	void* inputBuffer =nullptr;
	int size=0;
	std::string localMetaData ="";
	std::tie(inputBuffer, size, localMetaData) = getImageBufferAndMetadata(ctx, function, thisObject, argumentCount, arguments, exception);

	if(inputBuffer==nullptr)
	{
		LOG_ERROR_EX("PersistentWatermark::StorageVolatile::Update() could not get image buffer");
		return JSValueMakeBoolean(ctx, false);
	}

	//Prevent metadata &/or size mismatch if the memory operations below fail
	mMetaData = "";
	deleteSharedMemory();
	msize =0;

	LOG_TRACE("New Shared memory operations begin.");
	int ID = -1;
	for(int i=0; (i<10) && (ID==-1); i++)
	{
		mSharedMemoryKey = std::rand();
		LOG_TRACE("Watermark, generated new Shared memory key");
		ID = shmget(mSharedMemoryKey, size, 0644|IPC_CREAT|IPC_EXCL);
	}

	if(ID==-1)
	{
		LOG_ERROR_EX("PersistentWatermark: could not create shared memory %s", strerror(errno));
		return JSValueMakeBoolean(ctx, false);
	}

	{
		void* pSharedMemory = shmat(ID, nullptr, 0);
		if (pSharedMemory == reinterpret_cast<void*>(-1)) {
		LOG_ERROR_EX("PersistentWatermark: error mapping shared memory %s", strerror(errno));
		return JSValueMakeBoolean(ctx, false);
		}

		memcpy(pSharedMemory, inputBuffer, size);

		int detachCode = shmdt(pSharedMemory);
		if(detachCode==-1)
		{
			LOG_ERROR_EX("PersistentWatermark: error detaching shared memory %s", strerror(errno));
			return JSValueMakeBoolean(ctx, false);
		}
	}
	LOG_TRACE("New Shared memory operations complete.");

	/* To avoid misleading metadata, metaData & size are cleared early in this function and
	is only set to actual values below, when the memory operations (above) have completed successfully.*/
	msize = size;
	mMetaData = localMetaData;
	LOG_WARN_EX("PersistentWatermark: success");
	return JSValueMakeBoolean(ctx, true);
}

bool PersistentWatermark::StorageVolatile::UpdatePlugin(int layerID)
{
	LOG_TRACE("PersistentWatermark::StorageVolatile::UpdatePlugin()");
	if(msize<PNG_MIN_SIZE)
	{
		return false;
	}

	LOG_TRACE("Watermark update key=%d, size=%d", mSharedMemoryKey, msize);
	if(PluginAccess::get().UpdateWatermark(layerID, mSharedMemoryKey, msize))
	{
		return true;
	}
	else
	{
		return false;
	}
}

PersistentWatermark::StorageVolatile::~StorageVolatile()
{
	LOG_TRACE("PersistentWatermark::StorageVolatile::~StorageVolatile(): Enter");
	deleteSharedMemory();
	LOG_TRACE("PersistentWatermark::StorageVolatile::~StorageVolatile(): Exit");
}

std::string PersistentWatermark::StoragePersistent::getMetadata()
{
	LOG_TRACE("PersistentWatermark::StoragePersistent::getMetadata()");
	std::string metaData = PluginAccess::get().GetMetaDataWatermark();
	if(!metaData.empty())
	{
		LOG_WARN_EX("metadata: %s", metaData.c_str());
	}
	else
	{
		LOG_WARN_EX("PersistentWatermark::StoragePersistent::getMetadata() failed");
	}

	return metaData;
}

JSValueRef PersistentWatermark::StoragePersistent::Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	LOG_TRACE("PersistentWatermark::StoragePersistent::Update()");

	bool success =false;
	void* inputBuffer =nullptr;
	int binarySize=0;
	std::string metaData ="";
	std::tie(inputBuffer, binarySize, metaData) = getImageBufferAndMetadata(ctx, function, thisObject, argumentCount, arguments, exception);

	if(inputBuffer)
	{
		LOG_TRACE("PersistentWatermark::StoragePersistent::Update() processing binary buffer of %d bytes", binarySize);

		const char* base64Image = base64_Encode(reinterpret_cast<unsigned char*>(inputBuffer), binarySize);

		if(PluginAccess::get().PersistentStoreSaveWatermark(base64Image, metaData))
		{
			LOG_TRACE("PersistentWatermark::StoragePersistent::Update() success");
		}
		else
		{
			LOG_WARN_EX("PersistentWatermark::StoragePersistent::Update() failed");
		}
	}
	else
	{
		LOG_ERROR_EX("PersistentWatermark::StoragePersistent::Update() could not get image buffer");
	}

	return JSValueMakeBoolean(ctx, success);
}

bool PersistentWatermark::StoragePersistent::UpdatePlugin(int layerID)
{
	LOG_TRACE("PersistentWatermark::StoragePersistent::UpdatePlugin() AKA PersistentStoreLoad");
	bool success = PluginAccess::get().PersistentStoreLoadWatermark(layerID);
	if(success)
	{
		LOG_TRACE("PersistentWatermark::StoragePersistent::UpdatePlugin() success");
	}
	else
	{
		LOG_WARN_EX("PersistentWatermark::StoragePersistent::UpdatePlugin() failed");
	}

	return success;
}

PersistentWatermark::StoragePersistent::~StoragePersistent()
{
	LOG_TRACE("PersistentWatermark::StoragePersistent::~StoragePersistent()");
}
#endif