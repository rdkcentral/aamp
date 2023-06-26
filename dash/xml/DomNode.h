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
 * @file DomNode.h
 * @brief
 */

#ifndef FOG_CLI_DOMNODE_H
#define FOG_CLI_DOMNODE_H

#include <string>
#include <libxml/tree.h>
#include <memory>

using std::string;
using std::shared_ptr;

class DomNode;

class DomDocument;

/**
 * @class DomNode
 * @brief DomNode
 */
class DomNode {

protected:
    explicit DomNode(DomDocument *doc);

public:

    xmlNode *_xmlNode;

    DomDocument *_doc;

    DomNode(DomDocument *doc, xmlNode *_xmlNode) : _xmlNode(_xmlNode), _doc(doc) {}

    DomNode(const DomNode &node) = default;

    bool isElement();

    bool isText();

    DomDocument *ownerDocument() const;

    void removeChild(DomNode &child);

    DomNode firstChild();

    DomNode lastChild();

    DomNode parentNode();

    bool isNull() const;

    DomNode nextSiblingNode();

    DomNode prevSiblingNode();

    bool operator==(const DomNode& other);
};


#endif //FOG_CLI_DOMNODE_H
