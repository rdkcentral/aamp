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
 * @file CProfileInfo.h
 * @brief This file Manage the stat of Manifest , Fragment and License
 */

#ifndef __PROFILEINFO_H__
#define __PROFILEINFO_H__

#include "IPFragmentStatistics.h"
#include "IPLicnsStatistics.h"


/**
 *  @class CProfileInfo
 *  @brief Profile is consist of Manifest , Fragment and License stat,
 *         Here there are two types of fragments Init and Normal and last failed URL is stored
 *         for only fragments hence separate class is created for fragment CFragmentStatistics
 */
class CProfileInfo
{
/*private:*/
protected:
	CHTTPStatistics * mpManifestStat;
	CFragmentStatistics * mpFragmentStat;
	int mWidth;     /**< Frame width */
	int mHeight;    /**< Frame height */
public:

	/**
	 *   @brief Default constructor
	 *   @return None
	 */
	CProfileInfo() : mpManifestStat(NULL),mpFragmentStat(NULL),mWidth(0),mHeight(0)
	{

	}

	CProfileInfo(const CProfileInfo & newObj) :CProfileInfo()
	{
		if(newObj.mpManifestStat)
		{
			mpManifestStat = new CHTTPStatistics(*newObj.mpManifestStat);
		}

		if(newObj.mpFragmentStat)
		{
			this->mpFragmentStat = new CFragmentStatistics(*newObj.mpFragmentStat);
		}
	}

	CProfileInfo& operator=(const CProfileInfo& newObj)
	{
		if(newObj.mpManifestStat)
		{
			mpManifestStat = GetManifestStat(); // Allocate if required

			*mpManifestStat = *newObj.mpManifestStat;
		}
		else
		{
			if(mpManifestStat)
			{
				delete mpManifestStat;
				mpManifestStat = NULL;
			}
		}

		if(newObj.mpFragmentStat)
		{
			mpFragmentStat = GetFragmentStat(); // Allocate if required

			*mpFragmentStat = *newObj.mpFragmentStat;
		}
		else
		{
			if(mpFragmentStat)
			{
				delete mpFragmentStat;
				mpFragmentStat = NULL;
			}
		}

		return *this;
	}

	/**
	 *   @brief Default Destructor
	 */
	~CProfileInfo()
	{
		if(mpManifestStat)
		{
			delete mpManifestStat;
		}
		if(mpFragmentStat)
		{
			delete mpFragmentStat;
		}
	}

	/**
	 *   @fn GetManifestStat
	 *
	 *   @return CHTTPStatistics pointer
	 */
	CHTTPStatistics * GetManifestStat()
	{
		if(!mpManifestStat)
		{
			mpManifestStat = new CHTTPStatistics();
		}
		return mpManifestStat;
	}

	/**
	 *   @fn GetFragmentStat
	 *   @return CFragmentStatistics pointer
	 */
	CFragmentStatistics * GetFragmentStat()
	{
		if(!mpFragmentStat)
		{
			mpFragmentStat = new CFragmentStatistics();
		}
		return mpFragmentStat;
	}

	/**
	 *   @fn SetSize
	 *
	 *   @param[in] width width of profile
	 *   @param[in] height  Height of profile
         *
	 *   @return None
	 */
	void SetSize(int width, int height)
	{
		mWidth = width;
		mHeight = height;
	}

	/**
	 *   @fn ToJson
	 *
	 *   @return cJSON pointer
	 */
	cJSON * ToJson() const;


};

#endif //AAMP_SRC_PROFILEINFO_H
