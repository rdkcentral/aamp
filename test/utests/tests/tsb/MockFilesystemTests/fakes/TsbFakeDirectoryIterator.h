/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#ifndef __TSB_FAKE_DIRECTORY_ITERATOR__
#define __TSB_FAKE_DIRECTORY_ITERATOR__

#include <filesystem>

namespace TSB
{

namespace FS
{

class directory_iterator
{
public:
	directory_iterator() = default;

	directory_iterator(const directory_iterator&)
	{ /* This is a NOP */
	}

	explicit directory_iterator(const std::filesystem::path&)
	{ /* This is a NOP */
	}

	directory_iterator& operator++()
	{ /* This is a NOP */
		return *this;
	}

	std::filesystem::directory_entry operator*();
	bool operator!=(const directory_iterator&);
};

directory_iterator begin(directory_iterator);
directory_iterator end(directory_iterator);

} // namespace FS

} // namespace TSB

#endif // __TSB_FAKE_DIRECTORY_ITERATOR__
