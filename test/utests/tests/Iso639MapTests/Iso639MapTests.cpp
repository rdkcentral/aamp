/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <iostream>
#include <string>
#include <string.h>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include <iso639map.cpp>

static void TestHelper( const char *lang, LangCodePreference langCodePreference, const char *expectedResult )
{
    printf( "%s[", lang );
    switch( langCodePreference )
    {
    case ISO639_NO_LANGCODE_PREFERENCE:
        printf( "ISO639_NO_LANGCODE_PREFERENCE" );
        break;
    case ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE:
        printf( "ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE" );
        break;
    case ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE:
        printf( "ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE" );
        break;
    case ISO639_PREFER_2_CHAR_LANGCODE:
        printf( "ISO639_PREFER_2_CHAR_LANGCODE" );
        break;
    }
    char temp[256];
    strcpy( temp, lang );
    iso639map_NormalizeLanguageCode( temp, langCodePreference );
    printf( "] -> %s : ", temp );
    EXPECT_STREQ(temp, expectedResult);
}

TEST(_Iso639Map, test1)
{
    TestHelper( "en", ISO639_NO_LANGCODE_PREFERENCE, "en" );
    TestHelper( "en", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "eng" );
    TestHelper( "en", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "eng" );
    TestHelper( "en", ISO639_PREFER_2_CHAR_LANGCODE, "en" );

    TestHelper( "eng", ISO639_NO_LANGCODE_PREFERENCE, "eng" );
    TestHelper( "eng", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "eng" );
    TestHelper( "eng", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "eng" );
    TestHelper( "eng", ISO639_PREFER_2_CHAR_LANGCODE, "en" );

    TestHelper( "de", ISO639_NO_LANGCODE_PREFERENCE, "de" );
    TestHelper( "de", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "de", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "de", ISO639_PREFER_2_CHAR_LANGCODE, "de" );
    
    TestHelper( "deu", ISO639_NO_LANGCODE_PREFERENCE, "deu" );
    TestHelper( "deu", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "deu", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "deu", ISO639_PREFER_2_CHAR_LANGCODE, "de" );
    
    TestHelper( "ger", ISO639_NO_LANGCODE_PREFERENCE, "ger" );
    TestHelper( "ger", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "ger", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "ger", ISO639_PREFER_2_CHAR_LANGCODE, "de" );

    //Uppercase lang codes case:
    TestHelper( "DE", ISO639_NO_LANGCODE_PREFERENCE, "de" );
    TestHelper( "DE", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "DE", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "DE", ISO639_PREFER_2_CHAR_LANGCODE, "de" );

    TestHelper( "DEU", ISO639_NO_LANGCODE_PREFERENCE, "deu" );
    TestHelper( "DEU", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "DEU", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "DEU", ISO639_PREFER_2_CHAR_LANGCODE, "de" );

    TestHelper( "GER", ISO639_NO_LANGCODE_PREFERENCE, "ger" );
    TestHelper( "GER", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "GER", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "GER", ISO639_PREFER_2_CHAR_LANGCODE, "de" );
}


