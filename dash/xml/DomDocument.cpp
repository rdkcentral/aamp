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
 * @file DomDocument.cpp
 * @brief
 */
#include "DomDocument.h"
#include "DomElement.h"
#include <stdexcept>
#include "AampConfig.h"
#include "AampLogManager.h"

/**
 * @brief   gets document Element
 * @retval  document element
 */
DomElement DomDocument::documentElement() {
    auto rootNode = xmlDocGetRootElement(_xmlDoc);
    return DomElement(this, rootNode);
}

/**
 * @brief   clones document
 * @param   deep flag to indicate deep copy
 * @retval  cloned document
 */
std::shared_ptr<DomDocument> DomDocument::cloneDoc(bool deep) {
    auto cloned = xmlCopyDoc(_xmlDoc, deep);
    return std::make_shared<DomDocument>(cloned);
}

/**
 * @brief   document data to string
 * @retval  xml buffer
 */
string DomDocument::toString() {
    xmlChar* xmlbuff;
    int buff_sz;
    xmlKeepBlanksDefault(0);
    xmlDocDumpFormatMemory(_xmlDoc, &xmlbuff, &buff_sz, 1);

    string out((char*)xmlbuff, buff_sz);
    xmlFree(xmlbuff);
    return out;
}

/**
 * @brief   sets data to document
 * @param   content content to be set to document
 * @retval  true or false
 */
bool DomDocument::setContent(const string &content) {
    if(_xmlDoc != nullptr){
        xmlFreeDoc(_xmlDoc);
    }

    _xmlDoc = xmlReadMemory(content.c_str(), (int)content.size(), "noname.xml", NULL, 0);
    if (_xmlDoc == NULL) {
        AAMPLOG_ERR("Could not parse XML content");
    }
    return _xmlDoc != nullptr;
}

/**
 * @brief   DomDocument destructor
 */
DomDocument::~DomDocument() {
    if (_xmlDoc) xmlFreeDoc(_xmlDoc);
}

/**
 * @brief   DomDocument constructor
 * @para    docptr pointer to xml document
 */
DomDocument::DomDocument(xmlDoc *docptr): _xmlDoc(docptr) {

}

/**
 * @brief   gets namespace for a node in document
 * @param   node Dom Node
 * @param   namespace
 * @retval  xml namespace
 */
xmlNs *DomDocument::getNamespace(DomNode *node, const string &ns) {
    auto xmlns = xmlSearchNs(_xmlDoc, node->_xmlNode, (const xmlChar *)(ns.c_str()));
    return xmlns;
}

/**
 * @brief   DomDocument constructor
 * @param   file_path file path to read
 */
DomDocument::DomDocument(const string &file_path) :_xmlDoc(xmlReadFile(file_path.c_str(), NULL, 0)){
    if (_xmlDoc == NULL) {
        AAMPLOG_ERR("Could not parse XML content");
    }
}

