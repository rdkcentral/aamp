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
 * @file DomNode.cpp
 * @brief
 */

#include "DomNode.h"
#include "AampLogManager.h"
#include "AampConfig.h"
//#include "../DebugLog.h"


/**
 * @brief verify XML Node is of type XML_TEXT_NODE
 * @retval  true if XML Node is of type XML_TEXT_NODE
 */
bool DomNode::isText() {
    return _xmlNode->type == XML_TEXT_NODE;
}


/**
 * @brief   Gets owner document
 * @retval  Dom document
 */
DomDocument * DomNode::ownerDocument() const{
    return _doc;
}

/**
 * @brief   removes child node
 * @param   child Dom Node to be removed
 */
void DomNode::removeChild(DomNode &child) {
    if (child._xmlNode != NULL){
        xmlUnlinkNode(child._xmlNode);
        xmlFreeNode(child._xmlNode);
        child._xmlNode = NULL;
    }
}

/**
 * @brief   Gets first Child
 * @retval  Dom Node
 */
DomNode DomNode::firstChild() {
    if (_xmlNode == NULL) {
        return DomNode(_doc, _xmlNode);
    }
    xmlNode* child = _xmlNode->children;
    return DomNode(_doc, child);
}

/**
 * @brief   Gets Last Child
 * @retval  Dom Node
 */
DomNode DomNode::lastChild() {
    if (_xmlNode == NULL) {
        return DomNode(_doc, _xmlNode);
    }
    xmlNode* child = _xmlNode->last;
    return DomNode(_doc, child);
}

/**
 * @brief   Gets Parent Node
 * @retval  Dom Node
 */
DomNode DomNode::parentNode() {
    if (_xmlNode == NULL) {
        return DomNode(_doc, _xmlNode);
    }
    return DomNode(_doc, _xmlNode->parent);
}

/**
 * @brief   verifies XML Node is empty
 * @retval  true if XML Node is empty
 */
bool DomNode::isNull() const {
    return _xmlNode == NULL;
}

/**
 * @brief   verifies xmlNode is of type XML_ELEMENT_NODE
 * @retval  True if xmlNode is of type XML_ELEMENT_NODE else false
 */
bool DomNode::isElement() {
    if (_xmlNode == NULL) {
        return false;
    }
    return (_xmlNode->type == XML_ELEMENT_NODE);
}

/**
 * @brief   Gets Next sibling Node
 * @retval  Dom Node
 */
DomNode DomNode::nextSiblingNode() {
    if (_xmlNode == NULL) {
        return DomNode(_doc, _xmlNode);
    }
    return DomNode(_doc, _xmlNode->next);
}

/**
 * @brief   Gets Previous sibling Node
 * @retval  Dom Node
 */
DomNode DomNode::prevSiblingNode() {
    if (_xmlNode == NULL) {
        return DomNode(_doc, _xmlNode);
    }
    return DomNode(_doc, _xmlNode->prev);
}

/**
 * @brief   DomNode constructor
 * @param   doc DomDocument
 */
DomNode::DomNode(DomDocument *doc): _doc(doc), _xmlNode(NULL) {

}

/**
 * @brief   compares xml nodes and doc
 * @param   other Node to be compared
 * @retval  True or false
 */
bool DomNode::operator==(const DomNode &other) {
    return (_xmlNode == other._xmlNode) && (_doc == other._doc);
}


