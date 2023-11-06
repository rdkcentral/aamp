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
//  DomDocument.cpp
//

/**
 * @file DomDocument.h
 * @brief
 */

#ifndef FOG_CLI_DOMDOCUMENT_H
#define FOG_CLI_DOMDOCUMENT_H

#include <string>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "DomNode.h"
#include "DomElement.h"
#include <memory>

class DomElement;

/**
 * @class   DomDocument
 * @brief   DomDocument
 */
class DomDocument {


public:

    DomDocument(const string &file_path);

    xmlDoc* _xmlDoc;

    DomDocument() : _xmlDoc(nullptr) {

    }

    explicit DomDocument(xmlDoc *docptr);
    
    bool initialized(){ return _xmlDoc != nullptr; }

    DomElement documentElement();

    string toString();

    bool setContent(const string &content);

    std::shared_ptr<DomDocument> cloneDoc(bool deep);

    xmlNs* getNamespace(DomNode *node, const string &ns);

    ~DomDocument();

    DomDocument(const DomDocument&) = delete;

    DomDocument& operator=(const DomDocument&) = delete;
};


#endif //FOG_CLI_DOMDOCUMENT_H
