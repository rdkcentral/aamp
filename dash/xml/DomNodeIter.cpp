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
 * @file DomNodeIter.cpp
 * @brief
 */

#include <libxml/tree.h>
#include "DomNodeIter.h"
#include "DomNode.h"

/**
 * @brief   DomNodeIter constructor
 * @param   parent pointer to Dom Node
 */
DomNodeIter::DomNodeIter(DomNode *parent) : parent(parent), current(nullptr) {
    if (parent) {
        current = parent->_xmlNode->children;
    }
}

/**
 * @brief   makes current xml node points to next xml node
 * @retval  DomNodeIter
 */
const DomNodeIter &DomNodeIter::operator++() {
    if (current->next) {
        current = current->next;
    } else {
        current = nullptr;
    }
    return *this;
}

/**
 * @brief   compares two DomNodeIters
 * @param   other DomNodeIter
 * @retval  true if current xml node are same
 */
bool DomNodeIter::operator!=(DomNodeIter other) {
    return current != other.current;
}

/**
 * @brief   gets Dom Node
 * @retval  Dom Node
 */
DomNode DomNodeIter::operator*() {
    return DomNode(parent->ownerDocument(), current);
}

/**
 * @brief   DomNodeIter to beginning
 * @retval  DomNodeIter
 */
DomNodeIter DomNodeIter::begin() {
    return *this;
}

