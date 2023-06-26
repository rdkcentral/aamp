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
//
//  Path.cpp

/**
 * @file Path.cpp
 * @brief
 */

#include "Path.h"
//#include "../DownloadHelper.h"
#include <stdlib.h>

// system includes
#ifdef _WIN32
#include <windows.h>
#define SEP '\\'
#else

#include <unistd.h>
#include <stdexcept>

#define SEP '/'
#endif

using std::string;

/**
 * @brief   Path Constructor
 * @param   Path
 */
Path::Path(const string &path) : path(path) {

}

/**
 * @brief   Returns Path
 * @retval  Returns Path
 */
string Path::toString() {
    return path;
}

#if 0
/**
 * @brief   creates directory
 */
void Path::mkdirs() {
    mkdir_recursive(this->toString());
}
#endif

/**
 * @brief   Removes last component
 * @retval  Path
 */
Path Path::removeLastComponent() {
    auto pos = path.find_last_of(SEP);
    if (pos != string::npos) {
        return Path(path.substr(0, pos));
    } else {
        return path;
    }
}

/**
 * @brief   appends / and Other path
 * @param   Other path
 * @retval  Path
 */
Path Path::operator/(const Path &other) {
    Path self(path);

    string sep(1, SEP);

    if (self.path.empty()) {
        return other;
    }

    if (other.path.empty()) {
        return self;
    }

    if (!other.isRelative()) {
        throw std::runtime_error("Other path must be relative: " + other.path);
    }

    self = self.removeTrailingSeparator();
    self.path.append(sep).append(other);
    return self;
}

// system implementation
#ifdef _WIN32
/**
 * @brief   Gets File Attributes
 * @retval  True or False
 */
bool Path::exists() {
    return GetFileAttributesA(path.c_str()) != 0xffffffff;
}

/**
 * @brief   Absolute Path
 * @retval  Path
 */
Path Path::absolutePath() {
    char resolved_path[_MAX_PATH];
    _fullpath(resolved_path, path.c_str(), _MAX_PATH);
    return Path(String(resolved_path));
}

#else

/**
 * @brief   Gets File Attributes
 * @retval  Returns True or False
 */
bool Path::exists() {
    return access(path.c_str(), F_OK) == 0;
}

/**
 * @brief   Absolute Path
 * @retval  Path
 */
Path Path::absolutePath() {
    char resolved_path[1 << 16];
    realpath(path.c_str(), resolved_path);
    return Path(String(resolved_path));
}
#endif

/**
 * @brief   Relative Path
 * @param   Other Path
 * @retval  Path
 */
Path Path::relativeTo(const Path &parent) {
    if (this->isRelative() || parent.isRelative())
        return path;

    if (this->isRoot() || parent.isRoot()) {
        return path;
    }

    Path _parent = parent.removeTrailingSeparator();

    Path self(*this);
    self = self.removeTrailingSeparator();

    while (self.path.size() >= _parent.path.size()) {
        if (_parent == self) {
            return path.substr(_parent.path.size() + 1);
        }
        self = self.removeLastComponent();
    }

    return path;
}

/**
 * @brief   validates paths
 * @param   To be comapared other Path
 * @retval  true if both paths are not same
 */
bool Path::operator!=(const Path &other) {
    return this->path != other.path;
}

/**
 * @brief   Removes tailing "\\"
 * @retval  updated Path
 */
Path Path::removeTrailingSeparator() const {
    Path self(path);

    if (self.isRoot()) return self;

    if (self.path[self.path.size() - 1] == SEP) {
        self.path = path.substr(0, self.path.size() - 1);
    }
    return self;
}

/**
 * @brief   validates root
 * @retval  true if path starts with '/'
 */
bool Path::isRoot() const {
    // TODO implement Windows code
    return path == "/";
}

/**
 * @brief   Relative Path
 * @retval  true if path doesnt start with '/'
 */
bool Path::isRelative() const {
    // TODO implement Windows code
    return path[0] != '/';
}

/**
 * @brief   validates path with other path
 * @param   other Other Path
 * @retval  true if both Paths are same else false
 */
bool Path::operator==(const Path &other) {
    return this->path == other.path;
}

/**
 * @brief   canonical path
 * @retval  returns Path
 */
Path Path::canonical() const {
#ifndef WIN32
    char buf[8192] = {0};
    realpath(this->path.c_str(), buf);
    string c(buf);
    return Path(c);
# else
    throw std::runtime_error("Not yet implemented on WIN32");
#endif
}
