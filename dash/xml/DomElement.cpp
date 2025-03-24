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
//  DomElement.cpp
//

/**
 * @file DomElement.cpp
 * @brief
 */

#include "DomElement.h"
#include "DomNodeIter.h"
#include "DomTextNode.h"

/**
 * @brief   gets Tagname
 * @retval  xml node name
 */
string DomElement::tagName() const {
    return string((char *) _xmlNode->name);
}

/**
 * @brief   gets attribute value
 * @param   name Name
 * @param   defaultValue default value
 * @retval  attribute value
 */
string DomElement::attribute(const string &name, const string &defaultValue) const {
    xmlChar *v = xmlGetProp(_xmlNode, (const xmlChar *) name.c_str());
    if (v) {
        string prop((char *) v);
        xmlFree(v);
        return prop;
    } else {
        return defaultValue;
    }
}


/**
 * @brief   gets attribute value
 * @param   ns namespace
 * @param   name Name
 * @param   defaultValue default value
 * @retval  attribute value
 */
string DomElement::attribute(string ns, string name, string defaultValue) {
    auto xmlns = ownerDocument()->getNamespace(this, ns);
    if (!xmlns) return defaultValue;
    xmlChar *v = xmlGetNsProp(_xmlNode, (const xmlChar *) name.c_str(), xmlns->href);
    if (v) {
        string prop((char *) v);
        xmlFree(v);
        return prop;
    } else {
        return defaultValue;
    }
}

/**
 * @brief   Gets Text
 * @retval  Text
 */
string DomElement::text() {
    xmlChar *text =  xmlNodeGetContent(_xmlNode);
    string ret;
    if (text) {
        ret = (char *)text;
        xmlFree(text);
    }
    return ret;
}

/**
 * @brief   Sets Text to child Node
 * @param   text Text value to set
 */
void DomElement::setText(const std::string &text) {
    auto iter = DomNodeIter(this);
    for (auto node:iter) {
        if (node.isText()) {
            DomTextNode(node).setValue(text);
            return;
        }
    }

    // if not text node found
    auto textNode = DomTextNode(_doc, text);
    addChild(textNode);
}

/**
 * @brief   Adds child text node
 * @param   textNode text Node
 */
void DomElement::addChild(DomTextNode &textNode) {
    xmlAddChild(_xmlNode, textNode._xmlNode);
}

/**
 * @brief   Sets Attribute
 * @param   name key name
 * @param   value value
 */
void DomElement::setAttribute(string name, string value) {
    xmlSetProp(_xmlNode, (const xmlChar *) name.c_str(), (const xmlChar *) value.c_str());
}


/**
 * @brief   DomElement constructor
 * @param   doc dom document
 * @param   node xml Node
 */
DomElement::DomElement(DomDocument *doc, xmlNode *node) : DomNode(doc, node) {

}

/**
 * @brief   Sets Attribute
 * @param   ns namespace
 * @param   name key name
 * @param   value key value
 */
void DomElement::setAttribute(string ns, string name, string value) {
    auto xmlns = ownerDocument()->getNamespace(this, ns);
    if (!xmlns) {
        throw std::runtime_error("Could not find namespace " + ns);
    }
    xmlSetNsProp(_xmlNode, xmlns, (const xmlChar *) name.c_str(), (const xmlChar *) value.c_str());
}

/**
 * @brief   Adds Namespace
 * @param   prefix prefix
 * @param   href href
 * @retval  new namespace
 */
xmlNs *DomElement::addNamespace(const string &prefix, const string &href) {
    auto xmlns = xmlNewNs(_xmlNode, (const xmlChar *) href.c_str(), (const xmlChar *) prefix.c_str());
    return xmlns;
}


/**
 * @brief   gets first child element
 * @param   tagName Tag Name
 * @retval  dom child element
 */
DomElement DomElement::firstChildElement(string tagName) {
    DomNode child = firstChild();
    while (!child.isNull()) {
        if (child.isElement()) {
            auto e = DomElement(child);
            if (e.tagName() == tagName)
                return e;
        }
        child = child.nextSiblingNode();
    }
    return DomElement(_doc, NULL);
}

/**
 * @brief   gets last child element
 * @param   tagName Tag Name
 * @retval  dom child element
 */
DomElement DomElement::lastChildElement(string tagName) {
    DomNode child = lastChild();
    while (!child.isNull()) {
        if (child.isElement()) {
            auto e = DomElement(child);
            if (e.tagName() == tagName)
                return e;
        }
        child = child.prevSiblingNode();
    }
    return DomElement(_doc, NULL);
}

/**
 * @brief   gets next sibling element
 * @param   tagName Tag Name
 * @retval  returns dom element
 */
DomElement DomElement::nextSiblingElement(string tagName) {
    DomNode next = nextSiblingNode();
    while (!next.isNull()) {
        if (next.isElement()) {
            auto e = DomElement(next);
            if (e.tagName() == tagName) return e;
        }
        next = next.nextSiblingNode();
    }
    return DomElement(_doc, NULL);
}

/**
 * @brief   Adds child element
 * @param   attrs attributes
 * @param   text text
 * @retval  dom element
 */
DomElement DomElement::addChildElement(const string &name, std::map<string, string> attrs, const string &text) {
    auto newNode = xmlNewChild(_xmlNode, NULL, BAD_CAST name.c_str(), BAD_CAST text.c_str());

    for (auto it = attrs.begin(); it != attrs.end(); ++it) {
        xmlNewProp(newNode, BAD_CAST it->first.c_str(), BAD_CAST it->second.c_str());
    }
    return DomElement(this->_doc, newNode);
}

/**
 * @brief   Adds child element
 * @param   name name of child element
 * @param   text text to be added for child element
 * @retval  dom element
 */
DomElement DomElement::addChildElement(const string &name, const string &text) {
    std::map<string, string> empty_attrs;
    return addChildElement(name, empty_attrs, text);
}

/**
 * @brief   Adds child element
 * @param   name name
 * @retval  added child element
 */
DomElement DomElement::addChildElement(const string &name) {
    std::map<string, string> empty_attrs;
    return addChildElement(name, empty_attrs, "");
}

/**
 * @brief   removes attributes
 * @param   name Name
 */
void DomElement::removeAttribute(const string &name) {
    auto prop = xmlHasProp(_xmlNode, BAD_CAST name.c_str());
    if (prop)
        xmlRemoveProp(prop);
}

/**
 * @brief   clones parents document
 * @param   parent Dom Element
 * @param   first flag to indicate clone of first child
 * @retval  dom element
 */
DomElement DomElement::cloneTo(DomElement &parent, bool first) {
    auto copyNode = xmlDocCopyNode(_xmlNode, parent.ownerDocument()->_xmlDoc, 1);
    if (parent._xmlNode) {
        auto sibling = parent._xmlNode->children;
        if (!first || sibling == nullptr) {
            xmlAddChild(parent._xmlNode, copyNode);
        } else {
            xmlAddPrevSibling(sibling, copyNode);
        }
    }
    return DomElement(parent.ownerDocument(), copyNode);
}

/**
 * @brief   clones element to specific index inside parents document
 * @param   parent Dom Element
 * @param   index to indicate the position to be inserted
 * @retval  dom element
 */
DomElement DomElement::cloneAt(DomElement &parent, int index) {
    auto copyNode = xmlDocCopyNode(_xmlNode, parent.ownerDocument()->_xmlDoc, 1);
    if (parent._xmlNode) {
        auto sibling = parent._xmlNode->children;
        while (sibling && ((char *)sibling->name != tagName())) {
            sibling = sibling->next;
        }
        for(int i=0;i<index;i++) {
            if(sibling) {
                do {
                    sibling = sibling->next;
                } while (sibling && ((char *)sibling->name != tagName()));
            }
            else {
                break;
            }
        }
        if (sibling == nullptr) {
            xmlAddChild(parent._xmlNode, copyNode);
        } else {
            xmlAddPrevSibling(sibling, copyNode);
        }
    }
    return DomElement(parent.ownerDocument(), copyNode);
}
