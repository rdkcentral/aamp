/*
 * If not stated otherwise in this file or this component's license file the
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
#ifndef turbo_xml_hpp
#define turbo_xml_hpp

#include <map>
#include <vector>
#include <string>
#include <iostream> // for cout
#include "string_utils.hpp"
#include <assert.h>

class XmlInputStream
{
private:
	// pointer to in-memory raw xml
	const char *ptr;
	const char *fin;
public:
	XmlInputStream( const char *ptr, size_t len ) : ptr(ptr), fin(ptr+len){}

	int getNextByte( void )
	{
		if( ptr < fin )
		{
			return *ptr++;
		}
		else
		{ // end of stream
			return -1; // '\377'
		}
	}
};

class XmlNode
{
private:
	int getNextNonWhiteSpaceCharacter( XmlInputStream &inputStream )
	{
		for(;;)
		{
			int c = inputStream.getNextByte();
			if( c<0 || c>' ' ) return c;
		}
	}
	
	void ReadChildren( XmlInputStream &inputStream )
	{
		int c;
		for(;;)
		{
			L_NEXT_ELEMENT:
			// look for next <element> or content (text)
			c = getNextNonWhiteSpaceCharacter(inputStream);
			if( c<0 )
			{ // end of file
				return;
			}
			if( c!='<' )
			{ // collect child text content for element
				for(;;)
				{
					innerHTML += c;
					c = inputStream.getNextByte();
					if( c=='<' ) break;
				}
				// std::cout << "content:" << innerHTML << "\n";
			}
			
			std::string elementName;
			for(;;)
			{
				c = inputStream.getNextByte();
				if( c<=' ' || c=='>' )
				{
					break;
				}
				elementName += (char)c; // todo: walk pointer instead of building up char by char
			}
			if( starts_with(elementName,'/') )
			{ // </endtag>
				return; // pop!
			}
			
			XmlNode *child = new XmlNode(elementName);
			this->children.push_back( child );

			for(;;)
			{
				bool done = true;
				while( c<=' ' )
				{
					c = inputStream.getNextByte();
				}
				switch( c )
				{
					case '?': // XML tag
						c = inputStream.getNextByte();
						assert( c=='>' );
						break;
						
					case '/': // end tag
						c = inputStream.getNextByte();
						assert( c=='>' );
						break;
						
					case '>':
						child->ReadChildren(inputStream);
						break;
						
					default:
					{ // attrName="attrValue"
						std::string attrName;
						for(;;)
						{
							attrName += (char)c;
							c = inputStream.getNextByte();
							assert( c>=0 );
							if( c=='=' ) break;
						}
						c = inputStream.getNextByte();
						assert( c>=0 );
						assert(c=='"' || c=='\'');
						
						std::string attrValue;
						for(;;)
						{
							c = inputStream.getNextByte();
							assert( c>=0 );
							if( c=='"' || c=='\'' ) break;
							attrValue += (char)c;
						}
						child->attributes[attrName] = attrValue;
						
						c = inputStream.getNextByte();
						assert( c>=0 );
						
						done = false;
					} // attrName="attrValue"
				}
				if( done )
				{
					break;
				}
			} // for(;;) // next attribute-value pair
		} // next elememt
	} // ReadChildren
	
public:
	std::string tagName;
	std::vector<XmlNode *> children;
	// can we use std::vector<std::reference_wrapper<XmlNode *>> children; ?
	std::map<std::string,std::string> attributes;
	std::string innerHTML;
	
	const std::string &getAttribute( const std::string &attrName ) const
	{
		assert( hasAttribute(attrName) );
		return attributes.at(attrName);
	}
	
	bool hasAttribute( const std::string &attrName ) const
	{
		return attributes.find(attrName)!=attributes.end();
	}
	
	XmlNode( const std::string &tagName ) : tagName(tagName), attributes(), children(), innerHTML()
	{
	}

	XmlNode( const std::string &tagName, const char *ptr, size_t len ) : tagName(tagName), attributes(), children(), innerHTML()
	{
		XmlInputStream inputStream( ptr, len );
		ReadChildren( inputStream );
	}

	~XmlNode()
	{
	}
};

#endif /* turbo_xml_hpp */
