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
//  DomElement.h
//

/**
 * @file DomElement.h
 * @brief
 */

#ifndef FOG_CLI_DOMELEMENT_H
#define FOG_CLI_DOMELEMENT_H

#include <string>
#include <memory>
#include <map>
#include <stdexcept>
#include "DomDocument.h"
#include "DomNode.h"
#include "DomTextNode.h"


class DomDocument;

class DomNode;


/**
 * @class DomElement
 * @brief sub class of Dom Node
 */
class DomElement : public DomNode {


public:
    DomElement(DomDocument *doc, xmlNode *node);

    explicit DomElement(DomNode node) : DomNode(node._doc, node._xmlNode) {};

    string tagName() const;

    string attribute(const string &name, const string &defaultValue = "") const;

    string attribute(string ns, string name, string defaultValue);

    void setAttribute(string name, string value);

    void setAttribute(string ns, string name, string value);

    void removeAttribute(const string &name);

    string text();

    void setText(const std::string &text);

    xmlNs *addNamespace(const string &href, const string &uri);

    DomElement firstChildElement(string tagName);

    DomElement lastChildElement(string tagName);

    DomElement nextSiblingElement(string tagName);

    DomElement addChildElement(const string &name, std::map<string, string> attrs, const string &text);

    DomElement addChildElement(const string &name, const string &text);

    DomElement addChildElement(const string &name);

    void addChild(DomTextNode &textNode);

    DomElement cloneTo(DomElement &parent, bool first=false);

    DomElement cloneAt(DomElement &parent, int index=-1);
};


#endif //FOG_CLI_DOMELEMENT_H
