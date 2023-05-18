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

#ifndef __PERSISTENT_WATERMARK_STORAGE_H__
#define __PERSISTENT_WATERMARK_STORAGE_H__

#include <JavaScriptCore/JavaScript.h>
#include <mutex>
#include <string>

namespace PersistentWatermark
{
    /**
	 @brief Encapsulates watermark data storage
	* */
	class Storage
	{
	public:
		/**
		 @brief Return a reference to the singleton instance
		**/
		static Storage& getInstance()
		{
			static Storage instance;
			return(instance);
		}

		/**
		 @brief true if a valid watermark is stored
		**/
		bool valid();

		/**
		 @brief returns the metadata for the stored watermark (or "" if no watermark is stored)
		**/
		std::string getMetadata();

		/**
		 @brief When valid returns the shared memory key
		**/
		int getKey();

		/**
		 @brief When valid returns the size of the stored watermark image
		**/
		int getSize();

		/**
		 @brief clears any previously stored data and if possible stores the watermark image & metadata supplied in the JS arguments
		**/
		JSValueRef Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

		~Storage();

		//singleton, no copy, move or assignment
		Storage(const Storage&) = delete;
		Storage(Storage&&) = delete;
		Storage& operator=(const Storage&) = delete;
		Storage& operator=(Storage&&) = delete;

		private:
		static constexpr int PNG_SIGNATURE_SIZE = 8;
		static constexpr int PNG_CHUNK_METADATA_SIZE = 12;
		static constexpr int PNG_IHDR_DATA_SIZE = 13;
		static constexpr int PNG_MIN_SIZE = (PNG_SIGNATURE_SIZE + (PNG_CHUNK_METADATA_SIZE*4) + PNG_IHDR_DATA_SIZE);

		std::string mMetaData;
		int mSharedMemoryKey;
		int msize;
		std::mutex mMutex;

		/**
		 @brief private constructor for singleton class
		**/
		Storage();

		/**
		 @brief convienience function, mark any previously used shared memory for deletion
		**/
		void deleteSharedMemory();

	};
};
#endif