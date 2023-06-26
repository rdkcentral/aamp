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
//  DomNode.cpp
//

/**
 * @file DomNodeIter.h
 * @brief
 */

#ifndef FOG_CLI_DOMNODELIST_H
#define FOG_CLI_DOMNODELIST_H

#include <vector>
#include "DomNode.h"

using std::vector;


/**
 * Class with similar interface to QNodeList.
 *
 */
/**
 * @class DomNodeIter
 * @brief Class with similar interface to QNodeList
 */
class DomNodeIter {

    xmlNode *current;
    DomNode *parent;
public:

    explicit DomNodeIter(DomNode *parent);

    DomNodeIter begin();

    DomNodeIter end() { return DomNodeIter(nullptr); };

    const DomNodeIter &operator++();

    bool operator!=(DomNodeIter other);

    DomNode operator*();

};


#endif //FOG_CLI_DOMNODELIST_H
