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
#include <string>

namespace PersistentWatermark
{
	class StorageInterface
	{
		public:
		/**
		 @brief returns the metadata for the stored watermark (or "" if no watermark is stored)
		**/
		virtual std::string getMetadata() =0;

		/**
		 @brief clears any previously stored data and if possible stores the watermark image & metadata supplied in the JS arguments
		**/
		virtual JSValueRef Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) =0;

		/**
		 @brief Call to update the specified watermark plugin layer
		**/
		virtual bool UpdatePlugin(int layerID)=0;

		/**
		 @brief Ensure that destructors of derived classes are called on delete
		**/
		virtual ~StorageInterface(){/*empty*/};
	};


    /**
	 @brief singleton, class that abstracts away from storage specifics (i.e. volatile/none-volatile)
	*/
	class Storage: public StorageInterface
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

		//implement StorageInterface
		std::string getMetadata() override;
		JSValueRef Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) override;
		bool UpdatePlugin(int layerID) override;

		~Storage();

		//singleton, no copy, move or assignment
		Storage(const Storage&) = delete;
		Storage(Storage&&) = delete;
		Storage& operator=(const Storage&) = delete;
		Storage& operator=(Storage&&) = delete;

		private:
		StorageInterface* mpState;

		/**
		 @brief private constructor for singleton class
		**/
		Storage();
	};

    /**
	 @brief Encapsulates volatile storage specifics, intended to be used as a state by Storage
	* */
	class StorageVolatile: public StorageInterface
	{
	public:
		StorageVolatile();

		//implement StorageInterface
		std::string getMetadata() override;
		JSValueRef Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) override;
		bool UpdatePlugin(int layerID) override;

		~StorageVolatile();

	private:
		std::string mMetaData;
		int mSharedMemoryKey;
		int msize;

		/**
		 @brief convenience function, mark any previously used shared memory for deletion
		**/
		void deleteSharedMemory();
	};

	class StoragePersistent: public StorageInterface
	{
		public:		
		//implement StorageInterface
		std::string getMetadata() override;
		JSValueRef Update(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) override;
		bool UpdatePlugin(int layerID) override;

		~StoragePersistent();
	};
};
#endif
